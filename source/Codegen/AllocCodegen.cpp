// AllocCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


#define MALLOC_FUNC		"malloc"
#define FREE_FUNC		"free"


Result_t Alloc::codegen(CodegenInstance* cgi)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is one of the only places in the compiler where a hardcoded call is made to a non-provided function.

	FuncPair_t* fp = cgi->getDeclaredFunc(MALLOC_FUNC);
	if(!fp)
	{
		VarDecl* fakefdmvd = new VarDecl(this->posinfo, "size", false);
		fakefdmvd->type = "Uint64";

		std::deque<VarDecl*> params;
		params.push_back(fakefdmvd);
		FuncDecl* fakefm = new FuncDecl(this->posinfo, MALLOC_FUNC, params, "Int8*");
		fakefm->isFFI = true;

		fakefm->codegen(cgi);

		assert((fp = cgi->getDeclaredFunc(MALLOC_FUNC)));
	}

	llvm::Function* mallocf = fp->first;
	assert(mallocf);

	llvm::Type* allocType = 0;
	TypePair_t* typePair = 0;
	VarType builtinVt = Parser::determineVarType(this->typeName);
	if(builtinVt != VarType::UserDefined)
	{
		allocType = cgi->getLlvmTypeOfBuiltin(builtinVt);
	}
	else
	{
		typePair = cgi->getType(this->typeName);
		if(!typePair)
			GenError::unknownSymbol(this, this->typeName, SymbolType::Type);

		allocType = typePair->first;
	}

	assert(allocType);

	// call malloc
	uint64_t typesize = cgi->mainModule->getDataLayout()->getTypeSizeInBits(allocType) / 8;
	llvm::Value* allocatedmem = cgi->mainBuilder.CreateCall(mallocf, llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), typesize));


	// call the initialiser, if there is one
	llvm::Value* defaultValue = 0;
	allocatedmem = cgi->mainBuilder.CreatePointerCast(allocatedmem, allocType->getPointerTo());

	if(builtinVt != VarType::UserDefined)
	{
		defaultValue = llvm::Constant::getNullValue(allocType);
		cgi->mainBuilder.CreateStore(defaultValue, allocatedmem);
	}
	else
	{
		// todo: constructor params
		std::vector<llvm::Value*> args;
		args.push_back(allocatedmem);
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		llvm::Function* initfunc = cgi->getStructInitialiser(this, typePair, args);
		defaultValue = cgi->mainBuilder.CreateCall(initfunc, args);
	}

	return Result_t(allocatedmem, 0);
}


Result_t Dealloc::codegen(CodegenInstance* cgi)
{
	SymbolPair_t* sp = cgi->getSymPair(this, this->var->name);
	if(!sp)
		error(this, "Unknown symbol '%s'", this->var->name.c_str());

	sp->first.second = SymbolValidity::UseAfterDealloc;

	// call 'free'
	FuncPair_t* fp = cgi->getDeclaredFunc(FREE_FUNC);
	if(!fp)
	{
		VarDecl* fakefdmvd = new VarDecl(this->posinfo, "ptr", false);
		fakefdmvd->type = "Int8*";

		std::deque<VarDecl*> params;
		params.push_back(fakefdmvd);
		FuncDecl* fakefm = new FuncDecl(this->posinfo, MALLOC_FUNC, params, "Int8*");
		fakefm->isFFI = true;

		fakefm->codegen(cgi);

		assert((fp = cgi->getDeclaredFunc(FREE_FUNC)));
	}

	// this will be an alloca instance (aka pointer to whatever type it actually was)
	llvm::Value* varval = sp->first.first;

	// therefore, create a Load to get the actual value
	varval = cgi->mainBuilder.CreateLoad(varval);
	llvm::Value* freearg = cgi->mainBuilder.CreatePointerCast(varval, llvm::IntegerType::getInt8PtrTy(cgi->getContext()));

	cgi->mainBuilder.CreateCall(fp->first, freearg);
	return Result_t(0, 0);
}





















