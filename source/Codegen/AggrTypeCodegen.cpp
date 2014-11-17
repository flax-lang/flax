// AggrTypeCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


ValPtr_p ArrayIndex::codeGen()
{
	// get our array type
	llvm::Type* atype = getLlvmType(this->var);
	llvm::Type* etype = nullptr;

	if(atype->isArrayTy())
		etype = llvm::cast<llvm::ArrayType>(atype)->getArrayElementType();

	else if(atype->isPointerTy())
		etype = atype->getPointerElementType();

	else
		error("Can only index on pointer or array types.");


	// try and do compile-time bounds checking
	llvm::ArrayType* at = llvm::cast<llvm::ArrayType>(atype);
	if(atype->isArrayTy())
	{
		// dynamic arrays don't get bounds checking
		if(at->getNumElements() != 0)
		{
			Number* n = nullptr;
			if((n = dynamic_cast<Number*>(this->index)))
			{
				assert(!n->decimal);
				if(n->ival >= at->getNumElements())
					error("Compile-time bounds checking detected index '%d' is out of bounds of %s[%d]", n->ival, this->var->name.c_str(), at->getNumElements());
			}
		}
	}


	// todo: verify for pointers
	llvm::Value* lhs = this->var->codeGen().second;
	llvm::Value* ind = this->index->codeGen().first;

	std::vector<llvm::Value*> indices;
	indices.push_back(llvm::ConstantInt::getNullValue(llvm::Type::getInt64Ty(getContext())));
	indices.push_back(ind);

	llvm::Value* gep = mainBuilder.CreateGEP(lhs, indices, "indexPtr");
	return ValPtr_p(mainBuilder.CreateLoad(gep), gep);
}















ValPtr_p Struct::codeGen()
{
	llvm::Type** types = new llvm::Type*[this->funcs.size() + this->members.size()];

	if(isDuplicateType(this->name))
		error("Duplicate type '%s'", this->name.c_str());

	// check if there's an explicit initialiser
	Func* ifunc = nullptr;

	for(Func* func : this->funcs)
	{
		if(func->decl->name == "init")
		{
			// todo: verify the function
			ifunc = func;
		}

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
		llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::get(str, 0), llvm::PointerType::get(str, 0), false);
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
			mainBuilder.CreateStore(var->initVal ? var->initVal->codeGen().first : getDefaultValue(var), ptr);
		}

		for(Func* f : this->funcs)
		{
			int i = this->nameMap[f->decl->name];
			llvm::Value* ptr = mainBuilder.CreateStructGEP(self, i, "memberPtr");

			// mangle
			f->decl->name = "__struct@" + this->name + "_" + f->decl->name;
			llvm::Value* val = f->decl->codeGen().first;

			mainBuilder.CreateStore(val, ptr);

			llvm::BasicBlock* ob = mainBuilder.GetInsertBlock();
			f->codeGen();
			mainBuilder.SetInsertPoint(ob);
		}

		mainBuilder.CreateRet(self);

		llvm::verifyFunction(*func);
		this->initFunc = func;
	}
	else
	{
		this->initFunc = llvm::cast<llvm::Function>(ifunc->codeGen().first);
	}








	delete types;

	return ValPtr_p(nullptr, nullptr);
}

ValPtr_p MemberAccess::codeGen()
{
	// gen the var ref on the left.
	llvm::Value* self = this->target->codeGen().first;
	llvm::Type* type = getLlvmType(this->target);
	if(!type)
		error("(%s:%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	if(!type->isStructTy())
		error("Cannot do member access on non-aggregate types");

	TypePair_t* pair = getType(type->getStructName());
	if(!pair)
		error("(%s:%s:%d) -> Internal check failed: failed to retrieve type", __FILE__, __PRETTY_FUNCTION__, __LINE__);




	llvm::Function* insertfunc = mainBuilder.GetInsertBlock()->getParent();

	if(pair->second.second == ExprType::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);
		llvm::Type* str_t = pair->first;

		assert(str);
		assert(self);

		// get the index for the member
		Expr* rhs = this->member;
		int i = -1;

		VarRef* var = nullptr;
		FuncCall* fc = nullptr;
		if((var = dynamic_cast<VarRef*>(rhs)))
			i = str->nameMap[var->name];

		else if((fc = dynamic_cast<FuncCall*>(rhs)))
			i = str->nameMap[fc->name];

		else
			error("(%s:%s:%d) -> Internal check failed: no comprehendo", __FILE__, __PRETTY_FUNCTION__, __LINE__);


		// 1. we know this works because we codegen'ed it above
		// 2. we need this because the codegen returns the value of a CreateLoad, which is apparently the wrong type.
		llvm::Value* rawInst = getSymInst(this->target->name);
		llvm::Value* ptr = mainBuilder.CreateStructGEP(getSymInst(this->target->name), i, "memberPtr");
		llvm::Value* val = mainBuilder.CreateLoad(ptr);
		if(fc)
		{

			// now we need to determine if it exists, and its params.
			Func* callee = nullptr;
			for(Func* f : str->funcs)
			{
				if(f->decl->name == mangleName(str, fc->name))
				{
					callee = f;
					break;
				}
			}

			if(!callee)
				error("No such function with name '%s' as member of struct '%s'", fc->name.c_str(), str->name.c_str());


			// do some casting
			for(int i = 0; i < fc->params.size(); i++)
				fc->params[i] = autoCastType(callee->decl->params[i], fc->params[i]);


			std::vector<llvm::Value*> args;
			for(Expr* e : fc->params)
			{
				args.push_back(e->codeGen().first);
				if(args.back() == nullptr)
					return ValPtr_p(nullptr, nullptr);
			}

			return ValPtr_p(mainBuilder.CreateCall(val, args), nullptr);
		}
		else if(var)
		{
			return ValPtr_p(val, ptr);
		}
	}
	else
	{
		error("(%s:%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	}

	return ValPtr_p(0, 0);
}













