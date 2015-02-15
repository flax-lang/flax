// BuiltinTypes.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	Result_t handleBuiltinTypeAccess(CodegenInstance* cgi, MemberAccess* ma)
	{
		assert(cgi);
		assert(ma);


		// gen the var ref on the left.
		ValPtr_t p = ma->target->codegen(cgi).result;

		llvm::Value* self = p.first;
		llvm::Value* selfPtr = p.second;

		assert(cgi->isBuiltinType(self->getType()));

		if(!selfPtr)
			assert(!"selfPtr is null");

		if(self->getType()->isStructTy())
		{
			VarRef* vr		= dynamic_cast<VarRef*>(ma->member);
			FuncCall* fc	= dynamic_cast<FuncCall*>(ma->member);

			if(self->getType()->getStructName() == "__BuiltinStringType")
			{
				// todo: consolidate this into better code
				if(vr)
				{
					int sIndex = 0;
					if(vr->name == "length")
						sIndex = 0;

					else if(vr->name == "bytes")
						sIndex = 1;

					else
						error(ma, "String has no such property '%s'", vr->name.c_str());

					llvm::Value* retPtr = cgi->mainBuilder.CreateStructGEP(selfPtr, sIndex);
					llvm::Value* ret = cgi->mainBuilder.CreateLoad(retPtr);
					return Result_t(ret, retPtr);
				}
				else if(fc)
				{
					error(ma, "String has no method named '%s'", fc->name.c_str());
				}
				else
				{
					error(ma, "Invalid expression");
				}
			}
		}
		else
		{
			error(ma, "Can only call methods on struct types.");
		}

		return Result_t(0, 0);
	}
}































