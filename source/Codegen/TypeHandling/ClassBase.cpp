// ClassBase.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "codegen.h"

#include "classbase.h"

using namespace Ast;
using namespace Codegen;

namespace Codegen
{
	static fir::Function* generateMemberFunctionDecl(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, Func* fn, std::map<std::string, fir::Type*> tm)
	{
		fir::IRBlock* ob = cgi->irb.getCurrentBlock();

		fn->decl->ident.kind = IdKind::Method;
		fn->decl->ident.scope = cls->ident.scope;
		fn->decl->ident.scope.push_back(cls->ident.name);

		fir::Function* lfunc = 0;
		// if(cls->genericTypes.size() > 0)
		// {
		// 	lfunc = dynamic_cast<fir::Function*>(fn->decl->generateDeclForGenericFunction(cgi, tm).value);
		// }
		// else
		{
			lfunc = dynamic_cast<fir::Function*>(fn->decl->codegen(cgi).value);
			if(cls->genericTypes.size() > 0)
				fn->decl->didCodegen = false;
		}

		iceAssert(lfunc);

		cgi->irb.setCurrentBlock(ob);
		return lfunc;
	}


	void generateMemberFunctionBody(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, Func* fn, fir::Function* defaultInitFunc,
		fir::Function* fdecl, std::map<std::string, fir::Type*> tm)
	{
		if(fn->decl->genericTypes.size() == 0)
		{
			fir::IRBlock* ob = cgi->irb.getCurrentBlock();

			iceAssert(fdecl);

			fir::Function* ffn = 0;
			if(cls->genericTypes.size() > 0)
			{
				// ffn = cgi->instantiateGenericFunctionUsingMapping(fn->decl, tm, fn, 0, 0).firFunc;
				// iceAssert(ffn && "failed to instantiate function");
			}
			// else
			{
				// fdecl = dynamic_cast<fir::Function*>(fn->decl->generateDeclForGenericFunction(cgi, tm).value);
				ffn = dynamic_cast<fir::Function*>(fn->codegen(cgi, fdecl).value);
			}

			iceAssert(ffn);


			if(fn->decl->ident.name == "init")
			{
				// note: a bit hacky, but better than constantly fucking with creating fake ASTs
				// get the first block of the function
				// create a new block *before* that
				// call the auto init in the new block, then uncond branch to the old block.

				fir::IRBlock* beginBlock = ffn->getBlockList().front();
				fir::IRBlock* newBlock = new fir::IRBlock();

				newBlock->setFunction(ffn);
				newBlock->setName("call_autoinit");
				auto& blockList = ffn->getBlockList();
				blockList.insert(blockList.begin(), newBlock);

				cgi->irb.setCurrentBlock(newBlock);

				iceAssert(ffn->getArgumentCount() > 0);
				fir::Value* selfPtr = ffn->getArguments().front();

				cgi->irb.CreateCall1(defaultInitFunc, selfPtr);
				cgi->irb.CreateUnCondBranch(beginBlock);
			}

			cgi->irb.setCurrentBlock(ob);
		}
	}





	std::map<Ast::Func*, fir::Function*> doCodegenForMemberFunctions(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType,
		std::map<std::string, fir::Type*> tm)
	{
		std::map<Ast::Func*, fir::Function*> ret;

		// pass 1
		for(Func* f : cls->funcs)
		{
			for(auto fn : cls->funcs)
			{
				if(f != fn && f->decl->ident.name == fn->decl->ident.name)
				{
					int d = 0;
					std::vector<fir::Type*> ps;
					for(auto e : fn->decl->params)
						ps.push_back(e->getType(cgi, 0, true));

					if(cgi->isValidFuncOverload(FuncDefPair(0, f->decl, f), ps, &d, true))
					{
						exitless_error(f->decl, "Duplicate method declaration: %s", f->decl->ident.name.c_str());
						info(fn->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}

			for(auto m : cls->members)
			{
				if(f->decl->ident.name == m->ident.name)
				{
					exitless_error(f->decl, "'%s' was previously declared as a property of type '%s', cannot redefine it as a function",
						m->ident.name.c_str(), m->concretisedType->str().c_str());

					info(m, "Previous declaration was here:");
					doTheExit();
				}
			}


			for(auto m : cls->cprops)
			{
				if(f->decl->ident.name == m->ident.name)
				{
					exitless_error(f->decl, "'%s' was previously declared as a property of type '%s', cannot redefine it as a function",
						m->ident.name.c_str(), m->getType(cgi)->str().c_str());

					info(m, "Previous declaration was here:");
					doTheExit();
				}
			}



			f->decl->parentClass = { cls, clsType };
			fir::Function* ffn = generateMemberFunctionDecl(cgi, cls, clsType, f, tm);

			if(f->decl->ident.name == "init")
				cls->initFuncs.push_back(ffn);

			cls->lfuncs.push_back(ffn);
			cls->functionMap[f] = ffn;


			ret[f] = ffn;
		}

		return ret;
	}



	void doCodegenForComputedProperties(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm)
	{
		for(ComputedProperty* c : cls->cprops)
		{
			for(auto cp : cls->cprops)
			{
				if(c != cp && c->ident.name == cp->ident.name)
				{
					exitless_error(c, "Duplicate property '%s' in class", c->ident.name.c_str());
					info(cp, "Previous declaration was here.");
					doTheExit();
				}
			}

			for(auto m : cls->members)
			{
				if(m->ident.name == c->ident.name)
				{
					exitless_error(c, "Property '%s' was previously declared as a field", c->ident.name.c_str());
					info(m, "Previous declaration was here.");
					doTheExit();
				}
			}

			if(cls->attribs & Attr_VisPublic && !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			{
				c->attribs |= Attr_VisPublic;
			}

			std::string lenstr = std::to_string(c->ident.name.length()) + c->ident.name;

			if(c->getter)
			{
				std::vector<VarDecl*> params;
				FuncDecl* fakeDecl = new FuncDecl(c->pin, "_get" + lenstr, params, c->ptype);
				Func* fakeFunc = new Func(c->pin, fakeDecl, c->getter);

				fakeDecl->isStatic = c->isStatic;
				fakeDecl->parentClass = { cls, clsType };

				if((cls->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
					fakeDecl->attribs |= Attr_VisPublic;

				c->getterFunc = fakeDecl;
				c->getterFFn = generateMemberFunctionDecl(cgi, cls, clsType, fakeFunc, tm);
				generateMemberFunctionBody(cgi, cls, clsType, fakeFunc, 0, c->getterFFn, tm);
			}
			if(c->setter)
			{
				VarDecl* setterArg = new VarDecl(c->pin, c->setterArgName, true);
				setterArg->ptype = c->ptype;

				std::vector<VarDecl*> params { setterArg };
				FuncDecl* fakeDecl = new FuncDecl(c->pin, "_set" + lenstr, params, pts::NamedType::create(VOID_TYPE_STRING));
				Func* fakeFunc = new Func(c->pin, fakeDecl, c->setter);

				fakeDecl->isStatic = c->isStatic;
				fakeDecl->parentClass = { cls, clsType };

				if((cls->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
					fakeDecl->attribs |= Attr_VisPublic;

				c->setterFunc = fakeDecl;
				c->setterFFn = generateMemberFunctionDecl(cgi, cls, clsType, fakeFunc, tm);
				generateMemberFunctionBody(cgi, cls, clsType, fakeFunc, 0, c->setterFFn, tm);
			}
		}
	}


	void generateDeclForOperators(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm)
	{
		for(OpOverload* overl : cls->operatorOverloads)
		{
			for(auto oo : cls->operatorOverloads)
			{
				if(oo != overl && oo->op == overl->op)
				{
					int d = 0;
					std::vector<fir::Type*> ps;
					for(auto e : oo->func->decl->params)
						ps.push_back(e->getType(cgi));

					if(cgi->isValidFuncOverload(FuncDefPair(0, overl->func->decl, overl->func), ps, &d, true))
					{
						exitless_error(oo->func->decl, "Duplicate operator overload for '%s'", Parser::arithmeticOpToString(cgi, oo->op).c_str());
						info(overl->func->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}


			// note(anti-confusion): decl->codegen() looks at parentClass
			// and inserts an implicit self, so we don't need to do it.

			// overl->func->decl->ident.name = overl->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
			overl->func->decl->ident.kind = IdKind::Operator;
			overl->func->decl->ident.scope = cls->ident.scope;
			overl->func->decl->ident.scope.push_back(cls->ident.name);
			overl->func->decl->parentClass = { cls, clsType };

			if(cls->attribs & Attr_VisPublic && !(overl->func->decl->attribs & (Attr_VisPublic | Attr_VisPrivate | Attr_VisInternal)))
				overl->func->decl->attribs |= Attr_VisPublic;

			overl->func->decl->codegen(cgi);
		}




		for(AssignOpOverload* aoo : cls->assignmentOverloads)
		{
			// note(anti-confusion): decl->codegen() looks at parentClass
			// and inserts an implicit self, so we don't need to do it.

			for(auto a : cls->assignmentOverloads)
			{
				if(a != aoo && a->op == aoo->op)
				{
					int d = 0;
					std::vector<fir::Type*> ps;
					for(auto e : a->func->decl->params)
						ps.push_back(e->getType(cgi));

					if(cgi->isValidFuncOverload(FuncDefPair(0, aoo->func->decl, aoo->func), ps, &d, true))
					{
						exitless_error(a->func->decl, "Duplicate operator overload for '%s'", Parser::arithmeticOpToString(cgi, a->op).c_str());
						info(aoo->func->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}

			// aoo->func->decl->ident.name = aoo->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
			aoo->func->decl->ident.kind = IdKind::Operator;
			aoo->func->decl->ident.scope = cls->ident.scope;
			aoo->func->decl->ident.scope.push_back(cls->ident.name);
			aoo->func->decl->parentClass = { cls, clsType };

			if(cls->attribs & Attr_VisPublic && !(aoo->func->decl->attribs & (Attr_VisPublic | Attr_VisPrivate | Attr_VisInternal)))
				aoo->func->decl->attribs |= Attr_VisPublic;

			fir::Value* val = aoo->func->decl->codegen(cgi).value;
			aoo->lfunc = dynamic_cast<fir::Function*>(val);
			iceAssert(aoo->lfunc);

			if(!aoo->lfunc->getReturnType()->isVoidType())
			{
				HighlightOptions ops;
				ops.caret = aoo->pin;

				Parser::Pin hl = aoo->func->decl->returnTypePos;
				ops.underlines.push_back(hl);

				error(aoo, ops, "Assignment operators cannot return a value (currently returning %s)",
					aoo->lfunc->getReturnType()->str().c_str());
			}
		}






		for(SubscriptOpOverload* soo : cls->subscriptOverloads)
		{
			for(auto s : cls->subscriptOverloads)
			{
				if(s != soo)
				{
					int d = 0;
					std::vector<fir::Type*> ps;
					for(auto e : s->decl->params)
						ps.push_back(e->getType(cgi));

					if(cgi->isValidFuncOverload(FuncDefPair(0, soo->getterFn->decl, soo->getterFn), ps, &d, true))
					{
						exitless_error(s->decl, "Duplicate subscript operator");
						info(soo->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}


			// note(anti-confusion): decl->codegen() looks at parentClass
			// and inserts an implicit self, so we don't need to do it.

			std::string opString = Parser::operatorToMangledString(cgi, ArithmeticOp::Subscript);

			iceAssert(soo->getterBody);
			{
				fir::IRBlock* ob = cgi->irb.getCurrentBlock();

				// do getter.
				BracedBlock* body = soo->getterBody;
				FuncDecl* decl = new FuncDecl(body->pin, "_get" + std::to_string(opString.length()) + opString,
					soo->decl->params, soo->decl->ptype);

				decl->parentClass = { cls, clsType };
				decl->ident.kind = IdKind::Operator;
				decl->ident.scope = cls->ident.scope;
				decl->ident.scope.push_back(cls->ident.name);

				if(cls->attribs & Attr_VisPublic)
					decl->attribs |= Attr_VisPublic;

				soo->getterFunc = dynamic_cast<fir::Function*>(decl->codegen(cgi).value);
				iceAssert(soo->getterFunc);

				cgi->irb.setCurrentBlock(ob);

				soo->getterFn = new Func(decl->pin, decl, body);
			}

			if(soo->setterBody)
			{
				fir::IRBlock* ob = cgi->irb.getCurrentBlock();

				VarDecl* setterArg = new VarDecl(soo->pin, soo->setterArgName, true);
				setterArg->ptype = soo->decl->ptype;

				std::vector<VarDecl*> params;
				params = soo->decl->params;
				params.push_back(setterArg);

				// do getter.
				BracedBlock* body = soo->setterBody;
				FuncDecl* decl = new FuncDecl(body->pin, "_set" + std::to_string(opString.length()) + opString, params,
					pts::NamedType::create(VOID_TYPE_STRING));

				decl->parentClass = { cls, clsType };
				decl->ident.kind = IdKind::Operator;
				decl->ident.scope = cls->ident.scope;

				if(cls->attribs & Attr_VisPublic)
					decl->attribs |= Attr_VisPublic;

				soo->setterFunc = dynamic_cast<fir::Function*>(decl->codegen(cgi).value);
				iceAssert(soo->setterFunc);

				cgi->irb.setCurrentBlock(ob);

				soo->setterFn = new Func(decl->pin, decl, body);
			}
		}
	}















	void doCodegenForGeneralOperators(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm)
	{
		for(OpOverload* overl : cls->operatorOverloads)
		{
			overl->codegen(cgi);
		}
	}


	void doCodegenForAssignmentOperators(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm)
	{
		for(AssignOpOverload* aoo : cls->assignmentOverloads)
		{
			fir::IRBlock* ob = cgi->irb.getCurrentBlock();

			aoo->func->codegen(cgi);
			cgi->irb.setCurrentBlock(ob);
		}
	}

	void doCodegenForSubscriptOperators(CodegenInstance* cgi, ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm)
	{
		for(SubscriptOpOverload* soo : cls->subscriptOverloads)
		{
			// note(anti-confusion): decl->codegen() looks at parentClass
			// and inserts an implicit self, so we don't need to do it.

			iceAssert(soo->getterBody);
			{
				fir::IRBlock* ob = cgi->irb.getCurrentBlock();

				soo->getterFn->codegen(cgi);
				cgi->irb.setCurrentBlock(ob);
			}

			if(soo->setterBody)
			{
				fir::IRBlock* ob = cgi->irb.getCurrentBlock();

				soo->setterFn->codegen(cgi);
				cgi->irb.setCurrentBlock(ob);
			}
		}
	}
}












