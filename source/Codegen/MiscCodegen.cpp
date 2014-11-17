// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

llvm::Value* Root::codeGen()
{
	// two pass: first codegen all the declarations
	for(ForeignFuncDecl* f : this->foreignfuncs)
		f->codeGen();

	for(Func* f : this->functions)
		f->decl->codeGen();

	for(Struct* s : this->structs)
		s->codeGen();



	// then do the actual code
	for(Func* f : this->functions)
		f->codeGen();

	return nullptr;
}

