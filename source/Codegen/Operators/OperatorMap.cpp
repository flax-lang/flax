// OperatorMap.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	static OperatorMap _operatorMap;

	OperatorMap::OperatorMap()
	{
		this->theMap[ArithmeticOp::Add]					= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Subtract]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Multiply]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Divide]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Modulo]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::ShiftLeft]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::ShiftRight]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseAnd]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseOr]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseXor]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpLT]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpGT]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpLEq]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpGEq]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpEq]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpNEq]				= generalArithmeticOperator;

		this->theMap[ArithmeticOp::Assign]				= operatorAssign;
		this->theMap[ArithmeticOp::PlusEquals]			= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::MinusEquals]			= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::MultiplyEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::DivideEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::ModEquals]			= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::ShiftLeftEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::ShiftRightEquals]	= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseAndEquals]	= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseOrEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseXorEquals]	= generalCompoundAssignOperator;

		this->theMap[ArithmeticOp::LogicalNot]			= operatorLogicalNot;
		this->theMap[ArithmeticOp::LogicalAnd]			= operatorLogicalAnd;
		this->theMap[ArithmeticOp::LogicalOr]			= operatorLogicalOr;

		this->theMap[ArithmeticOp::Subscript]			= operatorSubscript;
		this->theMap[ArithmeticOp::Slice]				= operatorSlice;

		this->theMap[ArithmeticOp::Cast]				= operatorCast;
		this->theMap[ArithmeticOp::ForcedCast]			= operatorCast;

		this->theMap[ArithmeticOp::BitwiseNot]			= operatorBitwiseNot;
		this->theMap[ArithmeticOp::Plus]				= operatorUnaryPlus;
		this->theMap[ArithmeticOp::Minus]				= operatorUnaryMinus;
		this->theMap[ArithmeticOp::AddrOf]				= operatorAddressOf;
		this->theMap[ArithmeticOp::Deref]				= operatorDereference;
		this->theMap[ArithmeticOp::UserDefined]			= operatorCustom;
	}

	Result_t OperatorMap::call(ArithmeticOp op, CodegenInstance* cgi, Expr* usr, std::vector<Expr*> args)
	{
		auto fn = theMap[op > ArithmeticOp::UserDefined ? ArithmeticOp::UserDefined : op];
		iceAssert(fn);

		return fn(cgi, op, usr, args);
	}

	OperatorMap& OperatorMap::get()
	{
		return _operatorMap;
	}
}









