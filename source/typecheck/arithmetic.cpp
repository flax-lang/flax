// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

// std::string operatorToString(const Operator& op)
// {
// 	switch(op)
// 	{
// 		case Operator::Invalid:				return "invalid";
// 		case Operator::Add:					return "+";
// 		case Operator::Subtract:			return "-";
// 		case Operator::Multiply:			return "*";
// 		case Operator::Divide:				return "÷";
// 		case Operator::Modulo:				return "%";
// 		case Operator::Assign:				return "=";
// 		case Operator::BitwiseOr:			return "|";
// 		case Operator::BitwiseAnd:			return "&";
// 		case Operator::BitwiseXor:			return "^";
// 		case Operator::LogicalOr:			return "||";
// 		case Operator::LogicalAnd:			return "&&";
// 		case Operator::LogicalNot:			return "!";
// 		case Operator::ShiftLeft:			return "<<";
// 		case Operator::ShiftRight:			return ">>";
// 		case Operator::CompareEq:			return "==";
// 		case Operator::CompareNotEq:		return "!=";
// 		case Operator::CompareGreater:		return ">";
// 		case Operator::CompareGreaterEq:	return ">=";
// 		case Operator::CompareLess:			return "<";
// 		case Operator::CompareLessEq:		return "<=";
// 		case Operator::Cast:				return "cast";
// 		case Operator::DotOperator:			return ".";
// 		case Operator::BitwiseNot:			return "~";
// 		case Operator::PlusEquals:			return "+=";
// 		case Operator::MinusEquals:			return "-=";
// 		case Operator::MultiplyEquals:		return "*=";
// 		case Operator::DivideEquals:		return "÷=";
// 		case Operator::ModuloEquals:		return "%=";
// 		case Operator::ShiftLeftEquals:		return "<<=";
// 		case Operator::ShiftRightEquals:	return ">>=";
// 		case Operator::BitwiseAndEquals:	return "&=";
// 		case Operator::BitwiseOrEquals:		return "|=";
// 		case Operator::BitwiseXorEquals:	return "^=";

// 		default: iceAssert(0 && "invalid operator");
// 	}
// }

fir::Type* TCS::getBinaryOpResultType(fir::Type* left, fir::Type* right, std::string op, sst::FunctionDefn** overloadFn)
{
	if(op == "||" || op == "&&" || op == "!")
	{
		return fir::Type::getBool();
	}
	else if(op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=")
	{
		return fir::Type::getBool();
	}
	else if(op == "cast")
	{
		return right;
	}
	else if(op == "+")
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() + right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if(left->isStringType() && right->isStringType())
			return fir::Type::getString();

		else if((left->isStringType() && right->isCharType()) || (left->isCharType() && right->isStringType()))
			return fir::Type::getString();

		else if(left->isDynamicArrayType() && right->isDynamicArrayType() && left == right)
			return left;

		else if(left->isDynamicArrayType() && right == left->getArrayElementType())
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);

		else if(left->isPointerType() && (right->isIntegerType() || right->isConstantNumberType()))
			return left;

		else if(right->isPointerType() && (left->isIntegerType() || left->isConstantNumberType()))
			return right;
	}
	else if(op == "-")
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() - right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);

		else if(left->isPointerType() && (right->isIntegerType() || right->isConstantNumberType()))
			return left;

		else if(right->isPointerType() && (left->isIntegerType() || left->isConstantNumberType()))
			return right;
	}
	else if(op == "*")
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() * right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);

	}
	else if(op == "/")
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() / right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);
	}
	else if(op == "%")
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
		{
			return fir::Type::getConstantNumber( mpfr::fmod(left->toConstantNumberType()->getValue(),
				right->toConstantNumberType()->getValue()));
		}
		else if((left->isIntegerType() && right->isIntegerType()) || (left->isFloatingPointType() && right->isFloatingPointType()))
		{
			return (left->getBitWidth() > right->getBitWidth()) ? left : right;
		}
		else if((left->isIntegerType() && right->isFloatingPointType()) || (left->isFloatingPointType() && right->isIntegerType()))
		{
			return (left->isFloatingPointType() ? left : right);
		}
		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
		{
			return (left->isConstantNumberType() ? right : left);
		}
		else
		{
			return left;
		}
	}


	// ok, check the operator map.
	{
		auto tree = this->stree;
		while(tree)
		{
			for(auto op : tree->infixOperatorOverloads[op])
			{
				if(this->isDuplicateOverload(util::map(op->params, [](auto p) { return p.type; }), { left, right }))
				{
					if(overloadFn) *overloadFn = op;
					return op->returnType;
				}
			}

			tree = tree->parent;
		}
	}

	return 0;
}



sst::Expr* ast::BinaryOp::typecheck(TCS* fs, fir::Type* inferred)
{
	iceAssert(!isAssignOp(this->op));

	// TODO: infer the types properly for literal numbers
	// this has always been a thorn, dammit

	auto l = this->left->typecheck(fs, inferred);
	auto r = this->right->typecheck(fs, inferred);

	auto lt = l->type;
	auto rt = r->type;

	sst::FunctionDefn* overloadFn = 0;

	fir::Type* rest = fs->getBinaryOpResultType(lt, rt, this->op, &overloadFn);
	if(!rest)
	{
		HighlightOptions ho;
		ho.caret = this->loc;
		ho.underlines.push_back(this->left->loc);
		ho.underlines.push_back(this->right->loc);

		ho.drawCaret = true;
		error(this, ho, "Unsupported operator '%s' between types '%s' and '%s'", this->op, lt, rt);
	}

	auto ret = new sst::BinaryOp(this->loc, rest);

	ret->left = dynamic_cast<sst::Expr*>(l);
	ret->right = dynamic_cast<sst::Expr*>(r);
	ret->op = this->op;

	ret->overloadedOpFunction = overloadFn;

	return ret;
}

sst::Expr* ast::UnaryOp::typecheck(TCS* fs, fir::Type* inferred)
{
	auto v = this->expr->typecheck(fs, inferred);

	auto t = v->type;
	fir::Type* out = 0;

	// check for custom ops first, i guess.
	{
		auto tree = fs->stree;
		while(tree)
		{
			for(auto ovp : tree->prefixOperatorOverloads[this->op])
			{
				if(fs->isDuplicateOverload(util::map(ovp->params, [](auto p) { return p.type; }), { t }))
				{
					auto ret = new sst::UnaryOp(this->loc, ovp->returnType);
					ret->op = this->op;
					ret->expr = v;

					ret->overloadedOpFunction = ovp;
					return ret;
				}
			}

			tree = tree->parent;
		}
	}



	if(this->op == "!")
	{
		// check if we're convertible to bool
		if(!t->isBoolType())
			error(this, "Invalid use of logical-not-operator '!' on non-boolean type '%s'", t);

		out = fir::Type::getBool();
	}
	else if(this->op == "+" || this->op == "-")
	{
		if(t->isConstantNumberType())
			out = (op == "-" ? fir::Type::getConstantNumber(-1 * t->toConstantNumberType()->getValue()) : t);

		else if(!t->isIntegerType() && !t->isFloatingPointType())
			error(this, "Invalid use of unary plus/minus operator '+'/'-' on non-numerical type '%s'", t);

		else if(op == "-" && t->isIntegerType() && !t->isSignedIntType())
			error(this, "Invalid use of unary negation operator '-' on unsigned integer type '%s'", t);

		out = t;
	}
	else if(this->op == "~")
	{
		if(t->isConstantNumberType())
			error(this, "Bitwise operations are not supported on literal numbers");

		else if(!t->isIntegerType())
			error(this, "Invalid use of bitwise not operator '~' on non-integer type '%s'", t);

		else if(t->isSignedIntType())
			error(this, "Invalid use of bitwise not operator '~' on signed integer type '%s'", t);

		out = t;
	}
	else if(this->op == "*")
	{
		if(!t->isPointerType())
			error(this, "Invalid use of derefernce operator '*' on non-pointer type '%s'", t);

		out = t->getPointerElementType();
	}
	else if(this->op == "&")
	{
		if(t->isFunctionType())
			error(this, "Cannot take the address of a function; use it as a value type");

		out = t->getPointerTo();
	}
	else
	{
		error(this, "unsupported unary op '%s'???", this->op);
	}


	auto ret = new sst::UnaryOp(this->loc, out);
	ret->op = this->op;
	ret->expr = v;

	return ret;
}











