// OperatorOverloads.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



Result_t SubscriptOpOverload::codegen(Codegen::CodegenInstance *cgi, fir::Type* extratype, fir::Value* target)
{
	return Result_t(0, 0);
}

fir::Type* SubscriptOpOverload::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	iceAssert(0);
}


Result_t AssignOpOverload::codegen(Codegen::CodegenInstance *cgi, fir::Type* extratype, fir::Value* target)
{
	return Result_t(0, 0);
}

fir::Type* AssignOpOverload::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	iceAssert(0);
}








fir::Type* OpOverload::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return 0;
}

Result_t OpOverload::codegenOp(CodegenInstance* cgi, std::vector<fir::Type*> args)
{
	if(!this->didCodegen)
	{
		if(this->func->decl->ident.kind != IdKind::Operator)
		{
			this->func->decl->ident.kind = IdKind::Operator;
			// this->func->decl->ident.name = this->func->decl->ident.name.substr(9 /*strlen("operator#")*/);
		}

		// check if the operator is generic
		if(this->func->decl->genericTypes.size() > 0)
		{
			// yes, yes we are.
			if(args.size() > 0)
			{
				std::string err; Expr* e = 0;
				FuncDefPair res = cgi->instantiateGenericFunctionUsingParameters(this, this->func, args, &err, &e);

				this->lfunc = res.firFunc;
				return Result_t(res.firFunc, 0);
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
			this->lfunc = dynamic_cast<fir::Function*>(res.value);

			return res;
		}
	}
	else
	{
		iceAssert(this->lfunc);
		return Result_t(this->lfunc, 0);
	}
}


Result_t OpOverload::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	return this->codegenOp(cgi, std::vector<fir::Type*>());
}









































