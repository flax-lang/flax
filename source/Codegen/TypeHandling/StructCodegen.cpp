// StructCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
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

	// generate initialiser
	this->initFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), linkageType, "__automatic_init__" + this->mangledName, cgi->module);

	{
		VarDecl* fakeSelf = new VarDecl(this->posinfo, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->posinfo, this->initFunc->getName(), { fakeSelf }, "Void");
		cgi->addFunctionToScope({ this->initFunc, fd });
	}


	llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", this->initFunc);
	cgi->builder.SetInsertPoint(iblock);

	// create the local instance of reference to self
	llvm::Value* self = &this->initFunc->getArgumentList().front();



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
	llvm::verifyFunction(*this->initFunc);

	cgi->rootNode->publicTypes.push_back(std::pair<StructBase*, llvm::Type*>(this, str));
	cgi->addPublicFunc({ this->initFunc, 0 });



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
		p->codegen(cgi);

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




















