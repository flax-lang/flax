// ClassCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t ClassDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	this->createType(cgi);

	if(this->genericTypes.size() > 0 && !this->didCreateType)
		return Result_t(0, 0);


	TypePair_t* _type = cgi->getType(this->name);
	if(!_type)
		_type = cgi->getType(this->mangledName);

	if(!_type)
		GenError::unknownSymbol(cgi, this, this->name + " (mangled: " + this->mangledName + ")", SymbolType::Type);


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

	// printf("created init func for %s -- %s :: %s\n", this->name.c_str(), defaultInitFunc->getName().c_str(),
	// 	defaultInitFunc->getType()->str().c_str());
	{
		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->pin, defaultInitFunc->getName(), { fakeSelf }, VOID_TYPE_STRING);
		cgi->addFunctionToScope({ defaultInitFunc, fd });
	}


	fir::IRBlock* currentblock = cgi->builder.getCurrentBlock();

	fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser" + this->name, defaultInitFunc);
	cgi->builder.setCurrentBlock(iblock);

	// create the local instance of reference to self
	fir::Value* self = defaultInitFunc->getArguments().front();

	for(VarDecl* var : this->members)
	{
		if(!var->isStatic)
		{
			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			fir::Value* ptr = cgi->builder.CreateStructGEP(self, i);

			auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
			var->doInitialValue(cgi, cgi->getType(var->type.strType), r.first, r.second, ptr, false);
		}
		else
		{
			// generate some globals for static variables.
			// mangle the variable name.

			// a bit hacky, but still well-defined.
			std::string varname = cgi->mangleMemberFunction(this, var->name, std::deque<Ast::Expr*>());

			// generate a global variable
			fir::GlobalVariable* gv = cgi->module->createGlobalVariable(varname, var->inferredLType,
				fir::ConstantValue::getNullValue(var->inferredLType), var->immutable,
				this->attribs & Attr_VisPublic ? fir::LinkageType::External : fir::LinkageType::Internal);


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



	// check if we have any user-defined init() functions.
	// if we do, then we can stick the extension-init()-calls there
	// if not, then we need to do it here

	bool didFindInit = false;
	for(Func* f : this->funcs)
	{
		if(f->decl->name == "init")
		{
			didFindInit = true;
			break;
		}
	}

	if(!didFindInit)
	{
		// extra stuff, if any.
	}

	cgi->builder.CreateReturnVoid();
	cgi->builder.setCurrentBlock(currentblock);



	// add getters/setters (ie. computed properties)
	// as functions to simplify our code
	for(ComputedProperty* c : this->cprops)
	{
		VarDecl* fakeSelf = new VarDecl(c->pin, "self", true);
		fakeSelf->type = this->name + "*";

		if(c->getter)
		{
			std::deque<VarDecl*> params { fakeSelf };
			FuncDecl* fakeDecl = new FuncDecl(c->pin, "_get" + std::to_string(c->name.length()) + c->name, params, c->type.strType);
			Func* fakeFunc = new Func(c->pin, fakeDecl, c->getter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->getterFunc = fakeDecl;
		}
		if(c->setter)
		{
			VarDecl* setterArg = new VarDecl(c->pin, c->setterArgName, true);
			setterArg->type = c->type;

			std::deque<VarDecl*> params { fakeSelf, setterArg };
			FuncDecl* fakeDecl = new FuncDecl(c->pin, "_set" + std::to_string(c->name.length()) + c->name, params, VOID_TYPE_STRING);
			Func* fakeFunc = new Func(c->pin, fakeDecl, c->setter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->setterFunc = fakeDecl;
		}
	}





	// issue here is that functions aren't codegened (ie. don't have the fir::Function*)
	// before their bodies are codegened, so this makes functions in structs order-dependent.

	// pass 1
	for(Func* f : this->funcs)
	{
		fir::IRBlock* ob = cgi->builder.getCurrentBlock();

		fir::Value* val = nullptr;

		// todo for generics:
		// function expects first parameter not to be there
		// but since we've already been through this, it'll be there.

		val = f->decl->codegen(cgi).result.first;

		if(f->decl->name == "init")
			this->initFuncs.push_back(dynamic_cast<fir::Function*>(val));


		cgi->builder.setCurrentBlock(ob);
		this->lfuncs.push_back(dynamic_cast<fir::Function*>(val));


		if(f->decl->attribs & Attr_VisPublic)
		{
			// make the functions public as well
			cgi->addPublicFunc({ dynamic_cast<fir::Function*>(val), f->decl });
		}
	}

	// pass 2
	for(Func* f : this->funcs)
	{
		fir::IRBlock* ob = cgi->builder.getCurrentBlock();

		if(f->decl->name == "init")
		{
			std::deque<Expr*> todeque;

			VarRef* svr = new VarRef(this->pin, "self");
			todeque.push_back(svr);

			f->block->statements.push_front(new FuncCall(this->pin, "__auto_init__" + this->mangledName, todeque));
		}

		f->codegen(cgi);
		cgi->builder.setCurrentBlock(ob);
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

		aoo->func->decl->name = aoo->func->decl->name.substr(9 /*strlen("operator#")*/);
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
	if(this->genericTypes.size() > 0 && instantiatedGenericTypes.empty())
		return 0;

	if(this->didCreateType && instantiatedGenericTypes.size() == 0)
		return this->createdType;




	if(instantiatedGenericTypes.size() > 0)
	{
		cgi->pushGenericTypeStack();
		for(auto t : instantiatedGenericTypes)
			cgi->pushGenericType(t.first, t.second);
	}




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
		TypePair_t* type = cgi->getType(super);
		if(type == 0)
			error(this, "Type %s does not exist", super.c_str());

		if(type->second.second != TypeKind::Protocol)
		{
			error(this, "%s is not a protocol, and cannot be conformed to.", super.c_str());
		}

		// protcols not supported yet.
		error(this, "enotsup");
	}


	fir::Type** types = new fir::Type*[this->members.size()];

	// create a bodyless struct so we can use it
	std::deque<std::string> fullScope = cgi->getFullScope();
	this->mangledName = cgi->mangleWithNamespace(this->name, fullScope, false);
	std::string genericTypeMangle;

	if(instantiatedGenericTypes.size() > 0)
	{
		for(auto t : instantiatedGenericTypes)
			genericTypeMangle += "_" + t.first + ":" + t.second->str();
	}

	this->mangledName += genericTypeMangle;


	if(cgi->isDuplicateType(this->mangledName))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Type);


	fir::StructType* str = fir::StructType::createNamedWithoutBody(this->mangledName, cgi->getContext());


	this->scope = fullScope;
	{
		std::string oldname = this->name;

		// only add the base type if we haven't *ever* created it
		if(this->createdType == 0)
			cgi->addNewType(str, this, TypeKind::Class);

		this->name += genericTypeMangle;

		if(genericTypeMangle.length() > 0)
			cgi->addNewType(str, this, TypeKind::Class);

		this->name = oldname;

		// fprintf(stderr, "added type %s\n", this->mangledName.c_str());
	}







	// because we can't (and don't want to) mangle names in the parser,
	// we could only build an incomplete name -> index map
	// finish it here.


	for(Func* func : this->funcs)
	{
		// only override if we don't have one.
		if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			func->decl->attribs |= Attr_VisPublic;

		func->decl->parentClass = this;
		std::string mangled = cgi->mangleFunctionName(func->decl->name, func->decl->params);
		if(this->nameMap.find(mangled) != this->nameMap.end())
		{
			error(this, "Duplicate member '%s'", func->decl->name.c_str());
		}
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
			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			types[i] = cgi->getExprType(var);
		}
	}










	std::vector<fir::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec);

	this->didCreateType = true;

	delete[] types;

	this->createdType = str;

	if(instantiatedGenericTypes.size() > 0)
		cgi->popGenericTypeStack();


	cgi->module->addNamedType(str->getStructName(), str);

	return str;
}




















