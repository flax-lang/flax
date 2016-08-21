// OperatorOverloads.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



Result_t SubscriptOpOverload::codegen(Codegen::CodegenInstance *cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}


Result_t AssignOpOverload::codegen(Codegen::CodegenInstance *cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}


Result_t OpOverload::codegen(CodegenInstance* cgi, std::deque<fir::Type*> args)
{
	if(!this->didCodegen)
	{
		if(this->func->decl->ident.kind != IdKind::Operator)
		{
			this->func->decl->ident.kind = IdKind::Operator;
			this->func->decl->ident.name = this->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
		}

		// check if the operator is generic
		if(this->func->decl->genericTypes.size() > 0)
		{
			// yes, yes we are.
			if(args.size() > 0)
			{
				FuncPair_t res = cgi->instantiateGenericFunctionUsingParameters(this, { }, this->func, args);

				if(res.first != 0)
					this->didCodegen = true;

				this->lfunc = res.first;
				return Result_t(res.first, 0);
			}
			else
			{
				// can't do shit.
				return Result_t(0, 0);
			}
		}
		else
		{
			this->didCodegen = true;

			auto res = this->func->codegen(cgi);
			this->lfunc = dynamic_cast<fir::Function*>(res.result.first);

			return res;
		}
	}
	else
	{
		iceAssert(this->lfunc);
		return Result_t(this->lfunc, 0);
	}
}


Result_t OpOverload::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return this->codegen(cgi, std::deque<fir::Type*>());
}









































