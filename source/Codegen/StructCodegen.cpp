// StructCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


llvm::Value* Struct::codeGen()
{
	llvm::Type** types = new llvm::Type*[this->funcs.size() + this->members.size()];

	if(isDuplicateType(this->name))
		error("Duplicate type '%s'", this->name.c_str());

	// check if there's an explicit initialiser
	Func* ifunc = nullptr;

	for(Func* func : this->funcs)
	{
		if(func->decl->name == "init")
			ifunc = func;

		std::vector<llvm::Type*> args;
		for(VarDecl* v : func->decl->params)
			args.push_back(getLlvmType(v));

		types[this->nameMap[func->decl->name]] = llvm::PointerType::get(llvm::FunctionType::get(getLlvmType(func), llvm::ArrayRef<llvm::Type*>(args), false), 0);
	}


	// create llvm types
	for(VarDecl* var : this->members)
		types[this->nameMap[var->name]] = getLlvmType(var);


	std::vector<llvm::Type*> vec(types, types + (this->funcs.size() + this->members.size()));
	llvm::StructType* str = llvm::StructType::create(getContext(), llvm::ArrayRef<llvm::Type*>(vec), this->name);
	getVisibleTypes()[this->name] = TypePair_t(str, TypedExpr_t(this, ExprType::Struct));


	if(!ifunc)
	{
		// create one
		llvm::FunctionType* ft = llvm::FunctionType::get(str, llvm::PointerType::get(str, 0), false);
		llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "__automatic_init@" + this->name, mainModule);

		llvm::BasicBlock* block = llvm::BasicBlock::Create(getContext(), "initialiser", func);

		llvm::BasicBlock* old = mainBuilder.GetInsertBlock();
		mainBuilder.SetInsertPoint(block);

		// create the local instance of reference to self
		llvm::AllocaInst* self = allocateInstanceInBlock(func, str, "self");

		for(VarDecl* var : this->members)
		{
			int i = this->nameMap[var->name];
			llvm::Value* ptr = mainBuilder.CreateStructGEP(self, i, "memberPtr");
			mainBuilder.CreateStore(var->initVal ? var->initVal->codeGen() : getDefaultValue(var), ptr);
		}


		for(Func* f : this->funcs)
		{
			int i = this->nameMap[f->decl->name];
			llvm::Value* ptr = mainBuilder.CreateStructGEP(self, i, "memberPtr");

			// mangle
			f->decl->name = "__struct@" + this->name + "_" + f->decl->name;
			llvm::Value* val = f->decl->codeGen();

			mainBuilder.CreateStore(val, ptr);

			llvm::BasicBlock* ob = mainBuilder.GetInsertBlock();
			f->codeGen();
			mainBuilder.SetInsertPoint(ob);
		}
		mainBuilder.CreateRet(self);

		llvm::verifyFunction(*func);
		this->initFunc = func;

		printf("init function for '%s': [%s]\n", this->name.c_str(), getReadableType(func->getArgumentList().front().getType()).c_str());
	}
	else
	{
		this->initFunc = llvm::cast<llvm::Function>(ifunc->codeGen());
	}








	delete types;

	return 0;
}

llvm::Value* MemberAccess::codeGen()
{
	// gen the var ref on the left.
	this->target->codeGen();
	llvm::Type* type = getLlvmType(this->target);
	if(!type)
		error("(%s:%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	if(!type->isStructTy())
		error("Cannot do member access on non-aggregate types");

	TypePair_t* pair = getType(type->getStructName());
	if(!pair)
		error("(%s:%s:%d) -> Internal check failed: failed to retrieve type", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	if(pair->second.second == ExprType::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);
		assert(str);

		// get the index for the member
		Expr* rhs = this->member;
		int i = -1;

		VarRef* var = nullptr;
		FuncCall* fc = nullptr;
		if((var = dynamic_cast<VarRef*>(rhs)))
		{
			i = str->nameMap[var->name];
		}
		else if((fc = dynamic_cast<FuncCall*>(rhs)))
		{
			i = str->nameMap[fc->name];
		}
		else
		{
			error("(%s:%s:%d) -> Internal check failed: ?!?!", __FILE__, __PRETTY_FUNCTION__, __LINE__);
		}

		//

	}
	else
	{
		error("(%s:%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	}



	return 0;
}
