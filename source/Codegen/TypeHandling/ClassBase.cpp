// ClassBase.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "classbase.h"

using namespace Ast;
using namespace Codegen;

namespace Codegen
{
	static fir::Function* generateMemberFunctionDecl(CodegenInstance* cgi, ClassDef* cls, Func* fn)
	{
		fir::IRBlock* ob = cgi->builder.getCurrentBlock();

		fn->decl->ident.kind = IdKind::Method;
		fn->decl->ident.scope = cls->ident.scope;
		fn->decl->ident.scope.push_back(cls->ident.name);

		fir::Function* lfunc = dynamic_cast<fir::Function*>(fn->decl->codegen(cgi).result.first);
		iceAssert(lfunc);

		cgi->builder.setCurrentBlock(ob);
		if(fn->decl->attribs & Attr_VisPublic)
		{
			cgi->addPublicFunc(FuncDefPair(lfunc, fn->decl, fn));
		}

		return lfunc;
	}


	void generateMemberFunctionBody(CodegenInstance* cgi, ClassDef* cls, Func* fn, fir::Function* defaultInitFunc)
	{
		if(fn->decl->genericTypes.size() == 0)
		{
			fir::IRBlock* ob = cgi->builder.getCurrentBlock();

			fir::Function* ffn = dynamic_cast<fir::Function*>(fn->codegen(cgi).result.first);
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
				ffn->getBlockList().push_front(newBlock);

				cgi->builder.setCurrentBlock(newBlock);

				iceAssert(ffn->getArgumentCount() > 0);
				fir::Value* selfPtr = ffn->getArguments().front();

				cgi->builder.CreateCall1(defaultInitFunc, selfPtr);
				cgi->builder.CreateUnCondBranch(beginBlock);
			}

			cgi->builder.setCurrentBlock(ob);
		}
	}





	void doCodegenForMemberFunctions(CodegenInstance* cgi, ClassDef* cls)
	{
		// pass 1
		for(Func* f : cls->funcs)
		{
			for(auto fn : cls->funcs)
			{
				if(f != fn && f->decl->ident.name == fn->decl->ident.name)
				{
					int d = 0;
					std::deque<fir::Type*> ps;
					for(auto e : fn->decl->params)
						ps.push_back(e->getType(cgi, true));

					if(cgi->isValidFuncOverload(FuncDefPair(0, f->decl, f), ps, &d, true))
					{
						errorNoExit(f->decl, "Duplicate method declaration: %s", f->decl->ident.name.c_str());
						info(fn->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}

			for(auto m : cls->members)
			{
				if(f->decl->ident.name == m->ident.name)
				{
					errorNoExit(f->decl, "'%s' was previously declared as a property of type '%s', cannot redefine it as a function",
						m->ident.name.c_str(), m->concretisedType->cstr());

					info(m, "Previous declaration was here:");
					doTheExit();
				}
			}


			for(auto m : cls->cprops)
			{
				if(f->decl->ident.name == m->ident.name)
				{
					errorNoExit(f->decl, "'%s' was previously declared as a property of type '%s', cannot redefine it as a function",
						m->ident.name.c_str(), m->getType(cgi)->cstr());

					info(m, "Previous declaration was here:");
					doTheExit();
				}
			}



			f->decl->parentClass = cls;
			fir::Function* ffn = generateMemberFunctionDecl(cgi, cls, f);

			if(f->decl->ident.name == "init")
				cls->initFuncs.push_back(ffn);

			cls->lfuncs.push_back(ffn);
			cls->functionMap[f] = ffn;
		}
	}



	void doCodegenForComputedProperties(CodegenInstance* cgi, ClassDef* cls)
	{
		for(ComputedProperty* c : cls->cprops)
		{
			for(auto cp : cls->cprops)
			{
				if(c != cp && c->ident.name == cp->ident.name)
				{
					errorNoExit(c, "Duplicate property '%s' in class", c->ident.name.c_str());
					info(cp, "Previous declaration was here.");
					doTheExit();
				}
			}

			for(auto m : cls->members)
			{
				if(m->ident.name == c->ident.name)
				{
					errorNoExit(c, "Property '%s' was previously declared as a field", c->ident.name.c_str());
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
				std::deque<VarDecl*> params;
				FuncDecl* fakeDecl = new FuncDecl(c->pin, "_get" + lenstr, params, c->ptype);
				Func* fakeFunc = new Func(c->pin, fakeDecl, c->getter);

				fakeDecl->parentClass = cls;

				if((cls->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
					fakeDecl->attribs |= Attr_VisPublic;

				c->getterFunc = fakeDecl;
				c->getterFFn = generateMemberFunctionDecl(cgi, cls, fakeFunc);
				generateMemberFunctionBody(cgi, cls, fakeFunc, 0);
			}
			if(c->setter)
			{
				VarDecl* setterArg = new VarDecl(c->pin, c->setterArgName, true);
				setterArg->ptype = c->ptype;

				std::deque<VarDecl*> params { setterArg };
				FuncDecl* fakeDecl = new FuncDecl(c->pin, "_set" + lenstr, params, pts::NamedType::create(VOID_TYPE_STRING));
				Func* fakeFunc = new Func(c->pin, fakeDecl, c->setter);

				fakeDecl->parentClass = cls;

				if((cls->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
					fakeDecl->attribs |= Attr_VisPublic;

				c->setterFunc = fakeDecl;
				c->setterFFn = generateMemberFunctionDecl(cgi, cls, fakeFunc);
				generateMemberFunctionBody(cgi, cls, fakeFunc, 0);
			}
		}
	}


	void generateDeclForOperators(CodegenInstance* cgi, ClassDef* cls)
	{
		for(OpOverload* overl : cls->operatorOverloads)
		{
			for(auto oo : cls->operatorOverloads)
			{
				if(oo != overl && oo->op == overl->op)
				{
					int d = 0;
					std::deque<fir::Type*> ps;
					for(auto e : oo->func->decl->params)
						ps.push_back(e->getType(cgi));

					if(cgi->isValidFuncOverload(FuncDefPair(0, overl->func->decl, overl->func), ps, &d, true))
					{
						errorNoExit(oo->func->decl, "Duplicate operator overload for '%s'", Parser::arithmeticOpToString(cgi, oo->op).c_str());
						info(overl->func->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}


			// note(anti-confusion): decl->codegen() looks at parentClass
			// and inserts an implicit self, so we don't need to do it.

			overl->func->decl->ident.name = overl->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
			overl->func->decl->ident.kind = IdKind::Operator;
			overl->func->decl->parentClass = cls;

			if(cls->attribs & Attr_VisPublic && !(overl->func->decl->attribs & (Attr_VisPublic | Attr_VisPrivate | Attr_VisInternal)))
				overl->func->decl->attribs |= Attr_VisPublic;

			overl->func->decl->codegen(cgi);

			// fir::Value* val = overl->func->decl->codegen(cgi).result.first;
			// cgi->builder.setCurrentBlock(ob);

			// overl->lfunc = dynamic_cast<fir::Function*>(val);
			// iceAssert(overl->lfunc);

			// if(overl->func->decl->attribs & Attr_VisPublic || cls->attribs & Attr_VisPublic)
			// 	cgi->addPublicFunc({ overl->lfunc, overl->func->decl });

			// ob = cgi->builder.getCurrentBlock();

			// overl->func->codegen(cgi);

			// cgi->builder.setCurrentBlock(ob);
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
					std::deque<fir::Type*> ps;
					for(auto e : a->func->decl->params)
						ps.push_back(e->getType(cgi));

					if(cgi->isValidFuncOverload(FuncDefPair(0, aoo->func->decl, aoo->func), ps, &d, true))
					{
						errorNoExit(a->func->decl, "Duplicate operator overload for '%s'", Parser::arithmeticOpToString(cgi, a->op).c_str());
						info(aoo->func->decl, "Previous declaration was here.");
						doTheExit();
					}
				}
			}

			aoo->func->decl->ident.name = aoo->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
			aoo->func->decl->ident.kind = IdKind::Operator;
			aoo->func->decl->parentClass = cls;

			if(cls->attribs & Attr_VisPublic && !(aoo->func->decl->attribs & (Attr_VisPublic | Attr_VisPrivate | Attr_VisInternal)))
				aoo->func->decl->attribs |= Attr_VisPublic;

			fir::Value* val = aoo->func->decl->codegen(cgi).result.first;
			aoo->lfunc = dynamic_cast<fir::Function*>(val);
			iceAssert(aoo->lfunc);

			if(!aoo->lfunc->getReturnType()->isVoidType())
			{
				HighlightOptions ops;
				ops.caret = aoo->pin;

				if(aoo->func->decl->returnTypePos.file.size() > 0)
				{
					Parser::Pin hl = aoo->func->decl->returnTypePos;
					ops.underlines.push_back(hl);
				}

				error(aoo, ops, "Assignment operators cannot return a value (currently returning %s)",
					aoo->lfunc->getReturnType()->cstr());
			}

			if(aoo->func->decl->attribs & Attr_VisPublic || cls->attribs & Attr_VisPublic)
				cgi->addPublicFunc(FuncDefPair(aoo->lfunc, aoo->func->decl, aoo->func));
		}






		for(SubscriptOpOverload* soo : cls->subscriptOverloads)
		{
			for(auto s : cls->subscriptOverloads)
			{
				if(s != soo)
				{
					int d = 0;
					std::deque<fir::Type*> ps;
					for(auto e : s->decl->params)
						ps.push_back(e->getType(cgi));

					if(cgi->isValidFuncOverload(FuncDefPair(0, soo->getterFn->decl, soo->getterFn), ps, &d, true))
					{
						errorNoExit(s->decl, "Duplicate subscript operator");
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
				fir::IRBlock* ob = cgi->builder.getCurrentBlock();

				// do getter.
				BracedBlock* body = soo->getterBody;
				FuncDecl* decl = new FuncDecl(body->pin, "_get" + std::to_string(opString.length()) + opString,
					soo->decl->params, soo->decl->ptype);

				decl->parentClass = cls;
				decl->ident.kind = IdKind::Operator;

				if(cls->attribs & Attr_VisPublic)
					decl->attribs |= Attr_VisPublic;

				soo->getterFunc = dynamic_cast<fir::Function*>(decl->codegen(cgi).result.first);
				iceAssert(soo->getterFunc);

				cgi->builder.setCurrentBlock(ob);

				soo->getterFn = new Func(decl->pin, decl, body);

				if(decl->attribs & Attr_VisPublic || cls->attribs & Attr_VisPublic)
					cgi->addPublicFunc(FuncDefPair(soo->getterFunc, soo->getterFn->decl, soo->getterFn));
			}

			if(soo->setterBody)
			{
				fir::IRBlock* ob = cgi->builder.getCurrentBlock();

				VarDecl* setterArg = new VarDecl(soo->pin, soo->setterArgName, true);
				setterArg->ptype = soo->decl->ptype;

				std::deque<VarDecl*> params;
				params = soo->decl->params;
				params.push_back(setterArg);

				// do getter.
				BracedBlock* body = soo->setterBody;
				FuncDecl* decl = new FuncDecl(body->pin, "_set" + std::to_string(opString.length()) + opString, params,
					pts::NamedType::create(VOID_TYPE_STRING));

				decl->parentClass = cls;
				decl->ident.kind = IdKind::Operator;

				if(cls->attribs & Attr_VisPublic)
					decl->attribs |= Attr_VisPublic;

				soo->setterFunc = dynamic_cast<fir::Function*>(decl->codegen(cgi).result.first);
				iceAssert(soo->setterFunc);

				cgi->builder.setCurrentBlock(ob);

				soo->setterFn = new Func(decl->pin, decl, body);

				if(decl->attribs & Attr_VisPublic || cls->attribs & Attr_VisPublic)
					cgi->addPublicFunc(FuncDefPair(soo->setterFunc, soo->setterFn->decl, soo->setterFn));
			}
		}
	}















	void doCodegenForGeneralOperators(CodegenInstance* cgi, ClassDef* cls)
	{
		for(OpOverload* overl : cls->operatorOverloads)
		{
			overl->codegen(cgi);
		}
	}


	void doCodegenForAssignmentOperators(CodegenInstance* cgi, ClassDef* cls)
	{
		for(AssignOpOverload* aoo : cls->assignmentOverloads)
		{
			fir::IRBlock* ob = cgi->builder.getCurrentBlock();

			aoo->func->codegen(cgi);
			cgi->builder.setCurrentBlock(ob);
		}
	}

	void doCodegenForSubscriptOperators(CodegenInstance* cgi, ClassDef* cls)
	{
		for(SubscriptOpOverload* soo : cls->subscriptOverloads)
		{
			// note(anti-confusion): decl->codegen() looks at parentClass
			// and inserts an implicit self, so we don't need to do it.

			iceAssert(soo->getterBody);
			{
				fir::IRBlock* ob = cgi->builder.getCurrentBlock();

				soo->getterFn->codegen(cgi);
				cgi->builder.setCurrentBlock(ob);
			}

			if(soo->setterBody)
			{
				fir::IRBlock* ob = cgi->builder.getCurrentBlock();

				soo->setterFn->codegen(cgi);
				cgi->builder.setCurrentBlock(ob);
			}
		}
	}
}












