// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;






Result_t Alloc::codegen(CodegenInstance* cgi)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is one of the only places in the compiler where a hardcoded call is made to a non-provided function.

	FuncPair_t* fp = cgi->getDeclaredFunc("malloc");
	if(!fp)
	{
		VarDecl* fakefdmvd = new VarDecl(this->posinfo, "size", false);
		fakefdmvd->type = "Uint64";

		std::deque<VarDecl*> params;
		params.push_back(fakefdmvd);
		FuncDecl* fakefm = new FuncDecl(this->posinfo, "malloc", params, "Int8*");
		fakefm->isFFI = true;

		fakefm->codegen(cgi);

		assert((fp = cgi->getDeclaredFunc("malloc")));
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
			error(this, "Unknown type '%s'", this->typeName.c_str());

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
		llvm::Function* initfunc = cgi->mainModule->getFunction(((Struct*) typePair->second.first)->initFunc->getName());
		assert(initfunc);

		defaultValue = cgi->mainBuilder.CreateCall(initfunc, allocatedmem);
	}

	return Result_t(allocatedmem, 0);
}







Result_t Root::codegen(CodegenInstance* cgi)
{
	// we need to parse custom types first
	for(Struct* s : this->structs)
		s->createType(cgi);

	// two pass: first codegen all the declarations
	for(ForeignFuncDecl* f : this->foreignfuncs)
		f->codegen(cgi);

	for(Func* f : this->functions)
	{
		// special case to handle main(): always FFI top-level functions named 'main'
		if(f->decl->name == "main")
		{
			f->decl->attribs |= Attr_VisPublic;
			f->decl->isFFI = true;
		}

		f->decl->codegen(cgi);
	}

	for(Struct* s : this->structs)
		s->codegen(cgi);




	// then do the actual code
	for(Func* f : this->functions)
		f->codegen(cgi);

	return Result_t(0, 0);
}

