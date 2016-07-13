// ClassCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



static fir::Function* generateMemberFunctionDecl(CodegenInstance* cgi, ClassDef* cls, Func* fn)
{
	fir::IRBlock* ob = cgi->builder.getCurrentBlock();

	fir::Function* lfunc = dynamic_cast<fir::Function*>(fn->decl->codegen(cgi).result.first);

	cgi->builder.setCurrentBlock(ob);
	if(fn->decl->attribs & Attr_VisPublic)
	{
		cgi->addPublicFunc({ lfunc, fn->decl });
	}

	return lfunc;
}


static void generateMemberFunctionBody(CodegenInstance* cgi, ClassDef* cls, Func* fn, fir::Function* defaultInitFunc)
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





















Result_t ClassDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	this->createType(cgi);

	TypePair_t* _type = cgi->getType(this->ident);
	if(!_type) error(this, "how? generating class (%s) without type", this->ident.name.c_str());




	// if we're already done, don't.
	if(this->didCodegen)
		return Result_t(0, 0);

	this->didCodegen = true;

	fir::LinkageType linkageType;
	if(this->attribs & Attr_VisPublic)
	{
		linkageType = fir::LinkageType::External;
	}
	else
	{
		linkageType = fir::LinkageType::Internal;
	}






	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.first->codegen(cgi);
		cgi->popNestedTypeScope();
	}


	fir::StructType* str = this->createdType;

	// generate initialiser
	fir::Function* defaultInitFunc = cgi->module->getOrCreateFunction("__auto_init__" + str->getStructName(),
		fir::FunctionType::get({ str->getPointerTo() }, fir::PrimitiveType::getVoid(cgi->getContext()), false), linkageType);

	{
		fir::IRBlock* currentblock = cgi->builder.getCurrentBlock();

		fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser_" + this->ident.name, defaultInitFunc);
		cgi->builder.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = defaultInitFunc->getArguments().front();

		for(VarDecl* var : this->members)
		{
			if(!var->isStatic)
			{
				int i = this->nameMap[var->ident.name];
				iceAssert(i >= 0);

				fir::Value* ptr = cgi->builder.CreateStructGEP(self, i);

				auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
				var->doInitialValue(cgi, cgi->getTypeByString(var->type.strType), r.first, r.second, ptr, false);
			}
			else
			{
				// generate some globals for static variables.
				// mangle the variable name.

				// a bit hacky, but still well-defined.
				std::string varname = cgi->mangleMemberFunction(this, var->ident.name, std::deque<Ast::Expr*>());

				// generate a global variable
				fir::GlobalVariable* gv = cgi->module->createGlobalVariable(varname, var->inferredLType,
					fir::ConstantValue::getNullValue(var->inferredLType), var->immutable,
					(this->attribs & Attr_VisPublic) ? fir::LinkageType::External : fir::LinkageType::Internal);


				if(var->inferredLType->isStructType())
				{
					TypePair_t* cmplxtype = cgi->getType(var->inferredLType);
					iceAssert(cmplxtype);

					fir::Function* init = cgi->getStructInitialiser(var, cmplxtype, { gv });
					cgi->addGlobalConstructor(varname, init);
				}
				else
				{
					iceAssert(var->initVal);
					fir::Value* val = var->initVal->codegen(cgi, gv).result.first;
					if(dynamic_cast<fir::ConstantValue*>(val))
					{
						gv->setInitialValue(dynamic_cast<fir::ConstantValue*>(val));
					}
					else
					{
						error(this, "Static variables currently only support constant initialisers");
					}
				}
			}
		}

		cgi->builder.CreateReturnVoid();
		cgi->builder.setCurrentBlock(currentblock);
	}







	// generate the decls before the bodies, so we can (a) call recursively, and (b) call other member functions independent of
	// order of declaration.


	// pass 1
	for(Func* f : this->funcs)
	{
		for(auto fn : this->funcs)
		{
			if(f != fn)
			{
				std::string f1 = cgi->mangleFunctionName(fn->decl->ident.name, fn->decl->params);
				std::string f2 = cgi->mangleFunctionName(f->decl->ident.name, f->decl->params);

				if(f1 == f2)
				{
					info(fn->decl, "Previous declaration was here: %s", fn->decl->ident.name.c_str());
					error(f->decl, "Duplicate member function: %s", f->decl->ident.name.c_str());
				}
			}
		}

		fir::Function* ffn = generateMemberFunctionDecl(cgi, this, f);

		if(f->decl->ident.name == "init")
			this->initFuncs.push_back(ffn);

		this->lfuncs.push_back(ffn);
		this->functionMap[f] = ffn;
	}


	// do comprops here:
	// 1. we need to generate the decls separately (because they're fake)
	// 2. we need to *get* the fir::Function* to store somewhere to retrieve later
	// 3. we need the rest of the member decls to be in place, so we can call member functions
	// from the getters/setters.
	for(ComputedProperty* c : this->cprops)
	{
		std::string lenstr = std::to_string(c->ident.name.length()) + c->ident.name;

		if(c->getter)
		{
			std::deque<VarDecl*> params;
			FuncDecl* fakeDecl = new FuncDecl(c->pin, "_get" + lenstr, params, c->type.strType);
			Func* fakeFunc = new Func(c->pin, fakeDecl, c->getter);

			fakeDecl->parentClass = this;

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			c->getterFunc = fakeDecl;
			c->getterFFn = generateMemberFunctionDecl(cgi, this, fakeFunc);
			generateMemberFunctionBody(cgi, this, fakeFunc, 0);
		}
		if(c->setter)
		{
			VarDecl* setterArg = new VarDecl(c->pin, c->setterArgName, true);
			setterArg->type = c->type;

			std::deque<VarDecl*> params { setterArg };
			FuncDecl* fakeDecl = new FuncDecl(c->pin, "_set" + lenstr, params, VOID_TYPE_STRING);
			Func* fakeFunc = new Func(c->pin, fakeDecl, c->setter);

			fakeDecl->parentClass = this;

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			c->setterFunc = fakeDecl;
			c->setterFFn = generateMemberFunctionDecl(cgi, this, fakeFunc);
			generateMemberFunctionBody(cgi, this, fakeFunc, 0);
		}
	}

	// pass 2
	for(Func* f : this->funcs)
	{
		generateMemberFunctionBody(cgi, this, f, defaultInitFunc);
	}
























	if(initFuncs.size() == 0)
	{
		this->initFuncs.push_back(defaultInitFunc);
	}
	else
	{
		// handles generic types making more default initialisers

		bool found = false;
		for(auto f : initFuncs)
		{
			if(f->getType()->isTypeEqual(defaultInitFunc->getType()))
			{
				found = true;
				break;
			}
		}

		if(!found)
			this->initFuncs.push_back(defaultInitFunc);
	}


	cgi->addPublicFunc({ defaultInitFunc, 0 });






	for(AssignOpOverload* aoo : this->assignmentOverloads)
	{
		// note(anti-confusion): decl->codegen() looks at parentClass
		// and inserts an implicit self, so we don't need to do it.

		fir::IRBlock* ob = cgi->builder.getCurrentBlock();

		aoo->func->decl->ident.name = aoo->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
		aoo->func->decl->parentClass = this;

		if(this->attribs & Attr_VisPublic && !(aoo->func->decl->attribs & (Attr_VisPublic | Attr_VisPrivate | Attr_VisInternal)))
		{
			aoo->func->decl->attribs |= Attr_VisPublic;
		}

		fir::Value* val = aoo->func->decl->codegen(cgi).result.first;
		cgi->builder.setCurrentBlock(ob);

		aoo->lfunc = dynamic_cast<fir::Function*>(val);

		if(!aoo->lfunc->getReturnType()->isVoidType())
		{
			HighlightOptions ops;
			ops.caret = aoo->pin;

			if(aoo->func->decl->returnTypePos.file.size() > 0)
			{
				Parser::Pin hl = aoo->func->decl->returnTypePos;
				ops.underlines.push_back(hl);
			}

			error(aoo, ops, "Assignment operators cannot return a value (currently returning %s)", aoo->lfunc->getReturnType()->str().c_str());
		}

		if(aoo->func->decl->attribs & Attr_VisPublic || this->attribs & Attr_VisPublic)
			cgi->addPublicFunc({ aoo->lfunc, aoo->func->decl });

		ob = cgi->builder.getCurrentBlock();

		aoo->func->codegen(cgi);

		cgi->builder.setCurrentBlock(ob);
	}



	for(SubscriptOpOverload* soo : this->subscriptOverloads)
	{
		// note(anti-confusion): decl->codegen() looks at parentClass
		// and inserts an implicit self, so we don't need to do it.

		std::string opString = Parser::operatorToMangledString(cgi, ArithmeticOp::Subscript);

		iceAssert(soo->getterBody);
		{
			fir::IRBlock* ob = cgi->builder.getCurrentBlock();

			// do getter.
			BracedBlock* body = soo->getterBody;
			FuncDecl* decl = new FuncDecl(body->pin, "_get" + std::to_string(opString.length()) + opString,
				soo->decl->params, soo->decl->type.strType);

			decl->parentClass = this;

			if(this->attribs & Attr_VisPublic)
				decl->attribs |= Attr_VisPublic;

			soo->getterFunc = dynamic_cast<fir::Function*>(decl->codegen(cgi).result.first);
			iceAssert(soo->getterFunc);

			cgi->builder.setCurrentBlock(ob);

			Func* fn = new Func(decl->pin, decl, body);

			if(decl->attribs & Attr_VisPublic || this->attribs & Attr_VisPublic)
				cgi->addPublicFunc({ soo->getterFunc, decl });

			ob = cgi->builder.getCurrentBlock();
			fn->codegen(cgi);

			cgi->builder.setCurrentBlock(ob);
		}




		if(soo->setterBody)
		{
			fir::IRBlock* ob = cgi->builder.getCurrentBlock();

			VarDecl* setterArg = new VarDecl(soo->pin, soo->setterArgName, true);
			setterArg->type = soo->decl->type;

			std::deque<VarDecl*> params;
			params = soo->decl->params;
			params.push_back(setterArg);

			// do getter.
			BracedBlock* body = soo->setterBody;
			FuncDecl* decl = new FuncDecl(body->pin, "_set" + std::to_string(opString.length()) + opString, params, VOID_TYPE_STRING);

			decl->parentClass = this;

			if(this->attribs & Attr_VisPublic)
				decl->attribs |= Attr_VisPublic;

			soo->setterFunc = dynamic_cast<fir::Function*>(decl->codegen(cgi).result.first);
			iceAssert(soo->setterFunc);

			cgi->builder.setCurrentBlock(ob);

			Func* fn = new Func(decl->pin, decl, body);

			if(decl->attribs & Attr_VisPublic || this->attribs & Attr_VisPublic)
				cgi->addPublicFunc({ soo->setterFunc, decl });

			ob = cgi->builder.getCurrentBlock();
			fn->codegen(cgi);

			cgi->builder.setCurrentBlock(ob);
		}
	}




	return Result_t(0, 0);
}































fir::Type* ClassDef::createType(CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes)
{
	if(this->didCreateType)
		return this->createdType;


	this->ident.scope = cgi->getFullScope();

	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.second = nested.first->createType(cgi);
		cgi->popNestedTypeScope();
	}



	// check protocols
	for(auto super : this->protocolstrs)
	{
		// protcols not supported yet.
		error(this, "enotsup");
	}




	fir::Type** types = new fir::Type*[this->members.size()];

	// create a bodyless struct so we can use it

	if(cgi->isDuplicateType(this->ident))
		GenError::duplicateSymbol(cgi, this, this->ident.str(), SymbolType::Type);

	fir::StructType* str = fir::StructType::createNamedWithoutBody(this->ident.str(), cgi->getContext());

	iceAssert(this->createdType == 0);
	cgi->addNewType(str, this, TypeKind::Class);


	// because we can't (and don't want to) mangle names in the parser,
	// we could only build an incomplete name -> index map
	// finish it here.


	for(Func* func : this->funcs)
	{
		// only override if we don't have one.
		if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			func->decl->attribs |= Attr_VisPublic;

		func->decl->parentClass = this;
	}

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		// fir::Type* type = cgi->getExprType(var);

		iceAssert(var->inferredLType != 0);
		fir::Type* type = var->inferredLType;

		if(type == str)
		{
			error(this, "Cannot have non-pointer member of type self");
		}


		if(!var->isStatic)
		{
			int i = this->nameMap[var->ident.name];
			iceAssert(i >= 0);

			types[i] = cgi->getExprType(var);
		}
	}










	std::vector<fir::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec);

	this->didCreateType = true;

	delete[] types;

	this->createdType = str;

	cgi->module->addNamedType(str->getStructName(), str);

	return str;
}




















