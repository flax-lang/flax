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
		error(this, "Can only index on pointer or array types.");


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
					error(this, "Compile-time bounds checking detected index '%d' is out of bounds of %s[%d]", n->ival, this->var->name.c_str(), at->getNumElements());
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
	assert(this->didCreateType);
	llvm::StructType* str = llvm::cast<llvm::StructType>(getType(this->name)->first);



	// generate initialiser
	{
		llvm::Function* func = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(getContext()), llvm::PointerType::get(str, 0), false), llvm::Function::ExternalLinkage, "__automatic_init#" + this->name, mainModule);

		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(getContext(), "initialiser", func);
		mainBuilder.SetInsertPoint(iblock);

		// create the local instance of reference to self
		llvm::Value* self = &func->getArgumentList().front();

		for(VarDecl* var : this->members)
		{
			int i = this->nameMap[var->name];
			llvm::Value* ptr = mainBuilder.CreateStructGEP(self, i, "memberPtr_" + var->name);

			var->initVal = autoCastType(var, var->initVal);
			mainBuilder.CreateStore(var->initVal ? var->initVal->codeGen().first : getDefaultValue(var), ptr);
		}

		for(Func* f : this->funcs)
		{
			int i = this->nameMap[f->decl->name];
			llvm::Value* ptr = mainBuilder.CreateStructGEP(self, i, "memberPtr_" + f->decl->name);
			llvm::BasicBlock* ob = mainBuilder.GetInsertBlock();


			llvm::Value* val = nullptr;
			if(f == this->ifunc)
			{
				f->decl->name = mangleName(this, f->decl->name);
				val = f->decl->codeGen().first;

				std::deque<Expr*> fuckingshit;

				VarRef* svr = new VarRef("self");
				fuckingshit.push_back(svr);

				FuncCall* fc = new FuncCall("__automatic_init#" + this->name, fuckingshit);
				f->closure->statements.push_front(fc);

				auto oi = mainBuilder.GetInsertBlock();
				this->initFunc = llvm::cast<llvm::Function>(f->codeGen().first);
				mainBuilder.SetInsertPoint(oi);
			}
			else
			{
				// mangle
				f->decl->name = mangleName(this, f->decl->name);
				val = f->decl->codeGen().first;
				f->codeGen();
			}

			mainBuilder.SetInsertPoint(ob);
			mainBuilder.CreateStore(val, ptr);
		}

		mainBuilder.CreateRetVoid();

		llvm::verifyFunction(*func);
		this->defifunc = func;
	}



	if(!this->ifunc)
	{
		this->initFunc = this->defifunc;
	}

	return ValPtr_p(nullptr, nullptr);
}

void Struct::createType()
{
	if(isDuplicateType(this->name))
		error(this, "Redefinition of type '%s'", this->name.c_str());

	llvm::Type** types = new llvm::Type*[this->funcs.size() + this->members.size()];

	if(isDuplicateType(this->name))
		error(this, "Duplicate type '%s'", this->name.c_str());

	// check if there's an explicit initialiser
	this->ifunc = nullptr;

	// create a bodyless struct so we can use it
	llvm::StructType* str = llvm::StructType::create(getContext(), this->name);
	getVisibleTypes()[this->name] = TypePair_t(str, TypedExpr_t(this, ExprType::Struct));

	for(Func* func : this->funcs)
	{
		if(func->decl->name == "init")
			this->ifunc = func;

		std::vector<llvm::Type*> args;

		// implicit first paramter, is not shown
		VarDecl* implicit_self = new VarDecl("self", true);
		implicit_self->type = this->name + "Ptr";
		func->decl->params.push_front(implicit_self);

		for(VarDecl* v : func->decl->params)
			args.push_back(getLlvmType(v));

		types[this->nameMap[func->decl->name]] = llvm::PointerType::get(llvm::FunctionType::get(getLlvmType(func), llvm::ArrayRef<llvm::Type*>(args), false), 0);
	}


	// create llvm types
	for(VarDecl* var : this->members)
		types[this->nameMap[var->name]] = getLlvmType(var);


	std::vector<llvm::Type*> vec(types, types + (this->funcs.size() + this->members.size()));
	str->setBody(vec);

	this->didCreateType = true;

	delete types;
}
















ValPtr_p MemberAccess::codeGen()
{
	// gen the var ref on the left.
	ValPtr_p p = this->target->codeGen();

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;
	bool isPtr = false;

	llvm::Type* type = getLlvmType(this->target);
	if(!type)
		error("(%s:%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	if(!type->isStructTy())
	{
		if(type->isPointerTy() && type->getPointerElementType()->isStructTy())
			type = type->getPointerElementType(), isPtr = true;

		else
			error(this, "Cannot do member access on non-aggregate types");
	}

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


		// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
		llvm::Value* ptr = mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + (fc ? fc->name : var->name));
		llvm::Value* val = mainBuilder.CreateLoad(ptr);

		if(fc)
		{
			// now we need to determine if it exists, and its params.
			Func* callee = nullptr;
			for(Func* f : str->funcs)
			{
				// when comparing, we need to remangle the first bit that is the implicit self pointer
				if(f->decl->name == mangleName(str, fc->name))
				{
					callee = f;
					break;
				}
			}

			if(!callee)
				error(this, "No such function with name '%s' as member of struct '%s'", fc->name.c_str(), str->name.c_str());

			// do some casting
			for(int i = 0; i < fc->params.size(); i++)
				fc->params[i] = autoCastType(callee->decl->params[i], fc->params[i]);


			std::vector<llvm::Value*> args;
			args.push_back(isPtr ? self : selfPtr);

			for(Expr* e : fc->params)
			{
				args.push_back(e->codeGen().first);
				if(args.back() == nullptr)
					return ValPtr_p(0, 0);
			}

			return ValPtr_p(mainBuilder.CreateCall(val, args), 0);
		}
		else if(var)
		{
			return ValPtr_p(val, ptr);
		}
	}


	error("(%s:%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	return ValPtr_p(0, 0);
}













