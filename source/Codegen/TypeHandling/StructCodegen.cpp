// StructCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"

using namespace Ast;
using namespace Codegen;


Result_t Struct::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	iceAssert(this->didCreateType);
	TypePair_t* _type = cgi->getType(this->name);
	if(!_type)
		_type = cgi->getType(this->mangledName);

	if(!_type)
		GenError::unknownSymbol(cgi, this, this->name + " (mangled: " + this->mangledName + ")", SymbolType::Type);


	// if we're already done, don't.
	if(this->didCodegen)
		return Result_t(0, 0);

	this->didCodegen = true;





	llvm::GlobalValue::LinkageTypes linkageType;
	if(this->attribs & Attr_VisPublic)
	{
		linkageType = llvm::Function::ExternalLinkage;
	}
	else
	{
		linkageType = llvm::Function::InternalLinkage;
	}


	llvm::StructType* str = llvm::cast<llvm::StructType>(_type->first);
	cgi->rootNode->publicTypes.push_back(std::pair<StructBase*, llvm::Type*>(this, str));

	// generate initialiser
	{
		this->initFuncs.push_back(llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), linkageType, "__auto_init__" + this->mangledName, cgi->module));

		llvm::Function* defifunc = this->initFuncs.back();

		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->pin, defifunc->getName(), { fakeSelf }, "Void");
		cgi->addFunctionToScope({ defifunc, fd });


		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", defifunc);
		cgi->builder.SetInsertPoint(iblock);

		// create the local instance of reference to self
		llvm::Value* self = &defifunc->getArgumentList().front();

		for(VarDecl* var : this->members)
		{
			// not supported in structs
			iceAssert(!var->isStatic);

			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			llvm::Value* ptr = cgi->builder.CreateStructGEP(self, i, "memberPtr_" + var->name);

			auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
			var->doInitialValue(cgi, cgi->getType(var->type.strType), r.first, r.second, ptr, false);
		}




		cgi->builder.CreateRetVoid();
		llvm::verifyFunction(*defifunc);

		cgi->addPublicFunc({ defifunc, fd });
	}


	// create memberwise initialiser
	{
		std::vector<llvm::Type*> types;
		types.push_back(str->getPointerTo());
		for(auto e : str->elements())
			types.push_back(e);

		this->initFuncs.push_back(llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()),
			types, false), linkageType, "__auto_mem_init__" + this->mangledName, cgi->module));

		llvm::Function* memifunc = this->initFuncs.back();


		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		std::deque<VarDecl*> args;
		args.push_back(fakeSelf);

		for(auto m : this->members)
			args.push_back(m);

		FuncDecl* fd = new FuncDecl(this->pin, memifunc->getName(), args, "Void");
		cgi->addFunctionToScope({ memifunc, fd });




		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", memifunc);
		cgi->builder.SetInsertPoint(iblock);

		// create the local instance of reference to self
		llvm::Value* self = &memifunc->getArgumentList().front();


		for(size_t i = 0; i < this->members.size(); i++)
		{
			llvm::Value* v = getArgumentNOfFunction(memifunc, i + 1);

			v->setName("memberPtr_" + std::to_string(i));
			llvm::Value* ptr = cgi->builder.CreateStructGEP(self, i, "memberPtr_" + std::to_string(i));

			cgi->builder.CreateStore(v, ptr);
		}


		cgi->builder.CreateRetVoid();
		llvm::verifyFunction(*memifunc);

		cgi->addPublicFunc({ memifunc, fd });
	}




	// todo: number 3
	//
	// operators should be allowed to be defined outside of structs
	// for infix binops these would take 2 parameters, and work exactly the same.



	// todo: number 4
	//
	// @precedence should be changed to @operator[bla], to be able to specify more attributes
	// (infix/prefix/postfix, precedence, options for compiler autogen)






	// todo: number 5
	//
	// for any given operator(a, b), the compiler will assume that it is commutative, ie.
	// a `op` b == b `op` a.
	//
	// this would obviously be disabled (by default) if overriding '-' or '/'.
	// it can also be disabled/enabled for custom operators, and for overrides of normal ones
	// can also change this (ie. cross product, a x b == -(b x a))
	//
	// assuming commutation means that, defining a given type Foo, it is enough to define
	// operator*() and operator=(), to get *=, scalar * Foo, Foo * scalar and Foo *= scalar.
	// scalar *= Foo will not work however, since you can't commutate that -- assuming Foo * scalar
	// returns Foo.












	for(OpOverload* oo : this->opOverloads)
	{
		llvm::BasicBlock* ob = cgi->builder.GetInsertBlock();
		Func* f = oo->func;

		f->decl->name = f->decl->name.substr(9 /*strlen("operator#")*/ );
		f->decl->parentClass = this;

		llvm::Value* val = f->decl->codegen(cgi).result.first;

		cgi->builder.SetInsertPoint(ob);
		ArithmeticOp ao = cgi->determineArithmeticOp(f->decl->name);
		this->lOpOverloads.push_back(std::make_pair(ao, llvm::cast<llvm::Function>(val)));

		// make the functions public as well
		cgi->addPublicFunc({ llvm::cast<llvm::Function>(val), f->decl });


		ob = cgi->builder.GetInsertBlock();

		oo->func->codegen(cgi);
		cgi->builder.SetInsertPoint(ob);
	}


	return Result_t(0, 0);
}








llvm::Type* Struct::createType(CodegenInstance* cgi)
{
	if(this->didCreateType)
		return 0;

	// check our inheritances??
	llvm::Type** types = new llvm::Type*[this->members.size()];

	// create a bodyless struct so we can use it
	this->mangledName = cgi->mangleWithNamespace(this->name, cgi->getFullScope(), false);

	if(cgi->isDuplicateType(this->mangledName))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Type);



	llvm::StructType* str = llvm::StructType::create(llvm::getGlobalContext(), this->mangledName);

	this->scope = cgi->namespaceStack;
	cgi->addNewType(str, this, TypeKind::Struct);




	// because we can't (and don't want to) mangle names in the parser,
	// we could only build an incomplete name -> index map
	// finish it here.

	for(auto p : this->opOverloads)
	{
		// before calling codegen (that checks for valid overloads), insert the "self" parameter
		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);

		std::string fulltype;
		for(auto s : cgi->getFullScope())
			fulltype += s + "::";

		fakeSelf->type = fulltype + this->name + "*";

		p->func->decl->params.push_front(fakeSelf);

		p->codegen(cgi);

		// remove it after
		iceAssert(p->func->decl->params.front() == fakeSelf);
		p->func->decl->params.pop_front();
	}

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		llvm::Type* type = cgi->getLlvmType(var);
		if(type == str)
		{
			error(this, "Cannot have non-pointer member of type self");
		}

		if(!var->isStatic)
		{
			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			types[i] = cgi->getLlvmType(var);
		}
	}










	std::vector<llvm::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec, this->packed);

	this->didCreateType = true;

	delete types;

	return str;
}




















