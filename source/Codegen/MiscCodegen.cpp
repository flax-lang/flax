// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

ValPtr_p Root::codeGen()
{
	// we need to parse custom types first
	for(Struct* s : this->structs)
		s->createType();

	// two pass: first codegen all the declarations
	for(ForeignFuncDecl* f : this->foreignfuncs)
		f->codeGen();

	for(Func* f : this->functions)
	{
		// special case to handle main(): always FFI top-level functions named 'main'
		if(f->decl->name == "main")
			f->decl->isFFI = true;

		f->decl->codeGen();
	}

	for(Struct* s : this->structs)
		s->codeGen();




	// then do the actual code
	for(Func* f : this->functions)
		f->codeGen();

	return ValPtr_p(0, 0);
}

