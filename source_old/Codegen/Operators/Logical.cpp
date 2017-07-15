// Logical.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	Result_t operatorLogicalAnd(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s, have %zu", Parser::arithmeticOpToString(cgi, op).c_str(), args.size());

		// get ourselves some short circuiting goodness

		fir::IRBlock* currentBlock = cgi->irb.getCurrentBlock();

		fir::IRBlock* checkRight = cgi->irb.addNewBlockAfter("checkRight", currentBlock);
		fir::IRBlock* setTrue = cgi->irb.addNewBlockAfter("setTrue", currentBlock);
		fir::IRBlock* merge = cgi->irb.addNewBlockAfter("merge", currentBlock);

		cgi->irb.setCurrentBlock(currentBlock);

		fir::Value* resPtr = cgi->getStackAlloc(fir::Type::getBool());
		cgi->irb.CreateStore(fir::ConstantInt::getBool(false), resPtr);

		// always codegen left.
		fir::Value* lhs = args[0]->codegen(cgi).value;
		if(lhs->getType() != fir::Type::getBool())
			error(args[0], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		// branch to either setTrue or merge.
		cgi->irb.CreateCondBranch(lhs, checkRight, merge);


		// in this block, the first condition is true.
		// so if the second condition is also true, then we can jump to merge.
		cgi->irb.setCurrentBlock(checkRight);

		fir::Value* rhs = args[1]->codegen(cgi).value;

		if(lhs->getType() != fir::Type::getBool())
			error(args[1], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		cgi->irb.CreateCondBranch(rhs, setTrue, merge);


		// this block's sole purpose is to set the thing to true.
		cgi->irb.setCurrentBlock(setTrue);

		cgi->irb.CreateStore(fir::ConstantInt::getBool(true), resPtr);
		cgi->irb.CreateUnCondBranch(merge);

		// go back to the merge.
		cgi->irb.setCurrentBlock(merge);


		return Result_t(cgi->irb.CreateLoad(resPtr), resPtr);
	}

	Result_t operatorLogicalOr(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s, have %zu", Parser::arithmeticOpToString(cgi, op).c_str(), args.size());


		// much the same as logical and,
		// except the first branch goes to setTrue/checkRight, instead of checkRight/merge.


		fir::IRBlock* currentBlock = cgi->irb.getCurrentBlock();

		fir::IRBlock* checkRight = cgi->irb.addNewBlockAfter("checkRight", currentBlock);
		fir::IRBlock* setTrue = cgi->irb.addNewBlockAfter("setTrue", currentBlock);
		fir::IRBlock* merge = cgi->irb.addNewBlockAfter("merge", currentBlock);


		cgi->irb.setCurrentBlock(currentBlock);

		fir::Value* resPtr = cgi->getStackAlloc(fir::Type::getBool());
		cgi->irb.CreateStore(fir::ConstantInt::getBool(false), resPtr);

		// always codegen left.
		fir::Value* lhs = args[0]->codegen(cgi).value;
		if(lhs->getType() != fir::Type::getBool())
			error(args[0], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		// branch to either setTrue or merge.
		cgi->irb.CreateCondBranch(lhs, setTrue, checkRight);


		// in this block, the first condition is true.
		// so if the second condition is also true, then we can jump to merge.
		cgi->irb.setCurrentBlock(checkRight);

		fir::Value* rhs = args[1]->codegen(cgi).value;

		if(lhs->getType() != fir::Type::getBool())
			error(args[1], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

		cgi->irb.CreateCondBranch(rhs, setTrue, merge);


		// this block's sole purpose is to set the thing to true.
		cgi->irb.setCurrentBlock(setTrue);

		cgi->irb.CreateStore(fir::ConstantInt::getBool(true), resPtr);
		cgi->irb.CreateUnCondBranch(merge);


		// go back to the merge.
		cgi->irb.setCurrentBlock(merge);


		return Result_t(cgi->irb.CreateLoad(resPtr), resPtr);
	}

	Result_t operatorLogicalNot(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		if(args.size() != 1)
			error(user, "Expected 1 argument for logical not, have %zu", args.size());

		auto res = args[0]->codegen(cgi);
		return Result_t(cgi->irb.CreateLogicalNot(res.value), res.pointer);
	}
}











