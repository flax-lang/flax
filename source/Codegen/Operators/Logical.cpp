// Logical.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	Result_t operatorLogicalAnd(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		// get ourselves some short circuiting goodness

		fir::IRBlock* currentBlock = cgi->builder.getCurrentBlock();

		fir::IRBlock* checkRight = cgi->builder.addNewBlockAfter("checkRight", currentBlock);
		fir::IRBlock* setTrue = cgi->builder.addNewBlockAfter("setTrue", currentBlock);
		fir::IRBlock* merge = cgi->builder.addNewBlockAfter("merge", currentBlock);


		cgi->builder.setCurrentBlock(currentBlock);

		fir::Value* resPtr = cgi->getStackAlloc(fir::PrimitiveType::getBool());
		cgi->builder.CreateStore(fir::ConstantInt::getBool(false), resPtr);

		// always codegen left.
		fir::Value* lhs = args[0]->codegen(cgi).result.first;
		if(lhs->getType() != fir::PrimitiveType::getBool())
			error(args[0], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		// branch to either setTrue or merge.
		cgi->builder.CreateCondBranch(lhs, checkRight, merge);


		// in this block, the first condition is true.
		// so if the second condition is also true, then we can jump to merge.
		cgi->builder.setCurrentBlock(checkRight);

		fir::Value* rhs = args[1]->codegen(cgi).result.first;

		if(lhs->getType() != fir::PrimitiveType::getBool())
			error(args[1], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		cgi->builder.CreateCondBranch(rhs, setTrue, merge);


		// this block's sole purpose is to set the thing to true.
		cgi->builder.setCurrentBlock(setTrue);

		cgi->builder.CreateStore(fir::ConstantInt::getBool(true), resPtr);
		cgi->builder.CreateUnCondBranch(merge);

		// go back to the merge.
		cgi->builder.setCurrentBlock(merge);


		return Result_t(cgi->builder.CreateLoad(resPtr), resPtr);
	}

	Result_t operatorLogicalOr(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


		// much the same as logical and,
		// except the first branch goes to setTrue/checkRight, instead of checkRight/merge.


		fir::IRBlock* currentBlock = cgi->builder.getCurrentBlock();

		fir::IRBlock* checkRight = cgi->builder.addNewBlockAfter("checkRight", currentBlock);
		fir::IRBlock* setTrue = cgi->builder.addNewBlockAfter("setTrue", currentBlock);
		fir::IRBlock* merge = cgi->builder.addNewBlockAfter("merge", currentBlock);


		cgi->builder.setCurrentBlock(currentBlock);

		fir::Value* resPtr = cgi->getStackAlloc(fir::PrimitiveType::getBool());
		cgi->builder.CreateStore(fir::ConstantInt::getBool(false), resPtr);

		// always codegen left.
		fir::Value* lhs = args[0]->codegen(cgi).result.first;
		if(lhs->getType() != fir::PrimitiveType::getBool())
			error(args[0], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		// branch to either setTrue or merge.
		cgi->builder.CreateCondBranch(lhs, setTrue, checkRight);


		// in this block, the first condition is true.
		// so if the second condition is also true, then we can jump to merge.
		cgi->builder.setCurrentBlock(checkRight);

		fir::Value* rhs = args[1]->codegen(cgi).result.first;

		if(lhs->getType() != fir::PrimitiveType::getBool())
			error(args[1], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		cgi->builder.CreateCondBranch(rhs, setTrue, merge);


		// this block's sole purpose is to set the thing to true.
		cgi->builder.setCurrentBlock(setTrue);

		cgi->builder.CreateStore(fir::ConstantInt::getBool(true), resPtr);
		cgi->builder.CreateUnCondBranch(merge);


		// go back to the merge.
		cgi->builder.setCurrentBlock(merge);


		return Result_t(cgi->builder.CreateLoad(resPtr), resPtr);
	}

	Result_t operatorLogicalNot(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		iceAssert(0);
	}
}











