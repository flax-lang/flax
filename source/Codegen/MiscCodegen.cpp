// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

























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

