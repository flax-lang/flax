// OperatorOverloads.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t OpOverload::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// this is never really called for actual codegen. operators are handled as functions,
	// so we just put them into the structs' funcs.
	// BinOp will do a lookup on the opMap, but never call codegen for this.

	// however, this will get called, because we need to know if the parameters for
	// the operator overload are legit. people ignore our return value.

	FuncDecl* decl = this->func->decl;
	if(this->op == ArithmeticOp::Assign)
	{
		if(!this->isInType && decl->params.size() != 2)
		{
			error(this, "Operator overload for '=' can only have one argument, have %zu", decl->params.size());
		}

		// needs to return pointer to self
		fir::Type* ret = cgi->getExprType(decl);
		fir::Type* front = cgi->getExprType(decl->params.front());
		if(ret == fir::PrimitiveType::getVoid(cgi->getContext()))
		{
			error(this, "Operator overload for '=' must return a pointer to the LHS being assigned to (got void)");
		}
		else if(ret != (this->isInType ? front : front->getPointerTo()))
		{
			error(this, "Operator overload for '=' must return a pointer to the LHS being assigned to (got %s, need %s)",
				cgi->getReadableType(ret).c_str(), cgi->getReadableType(front).c_str());
		}

		// we can't actually do much, because they can assign to anything
	}
	else if(this->op == ArithmeticOp::CmpEq)
	{
		if(decl->params.size() != 2)
			error(this, "Operator overload for '==' can only have two arguments, have %zu", decl->params.size());

		if(cgi->getExprType(decl) != fir::PrimitiveType::getBool(cgi->getContext()))
			error(this, "Operator overload for '==' must return a boolean value");
	}
	else if(this->op == ArithmeticOp::Add || this->op == ArithmeticOp::Subtract || this->op == ArithmeticOp::Multiply
		|| this->op == ArithmeticOp::Divide || this->op == ArithmeticOp::PlusEquals || this->op == ArithmeticOp::MinusEquals
		|| this->op == ArithmeticOp::MultiplyEquals || this->op == ArithmeticOp::DivideEquals)
	{
		if(decl->params.size() != 2)
			error(this, "Operator overload can only have two arguments, have %zu", decl->params.size());
	}
	else if(decl->params.size() > 2)
	{
		// custom operator... but we have no way to handle 2 arguments
		// todo: will change when we allow operator definitions outside of class bodies.

		error(this, "Cannot currently handle operator overloads with more than two arguments");
	}

	return Result_t(0, 0);
}





























