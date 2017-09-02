// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

std::string operatorToString(const Operator& op)
{
	switch(op)
	{
		case Operator::Invalid:				return "invalid";
		case Operator::Add:					return "+";
		case Operator::Subtract:			return "-";
		case Operator::Multiply:			return "*";
		case Operator::Divide:				return "÷";
		case Operator::Modulo:				return "%";
		case Operator::Assign:				return "=";
		case Operator::BitwiseOr:			return "|";
		case Operator::BitwiseAnd:			return "&";
		case Operator::BitwiseXor:			return "^";
		case Operator::LogicalOr:			return "||";
		case Operator::LogicalAnd:			return "&&";
		case Operator::LogicalNot:			return "!";
		case Operator::ShiftLeft:			return "<<";
		case Operator::ShiftRight:			return ">>";
		case Operator::CompareEq:			return "==";
		case Operator::CompareNotEq:		return "!=";
		case Operator::CompareGreater:		return ">";
		case Operator::CompareGreaterEq:	return ">=";
		case Operator::CompareLess:			return "<";
		case Operator::CompareLessEq:		return "<=";
		case Operator::Cast:				return "cast";
		case Operator::DotOperator:			return ".";
		case Operator::BitwiseNot:			return "~";
		case Operator::Minus:				return "-";
		case Operator::Plus:				return "+";
		case Operator::AddressOf:			return "&";
		case Operator::Dereference:			return "*";
		case Operator::PlusEquals:			return "+=";
		case Operator::MinusEquals:			return "-=";
		case Operator::MultiplyEquals:		return "*=";
		case Operator::DivideEquals:		return "÷=";
		case Operator::ModuloEquals:		return "%=";
		case Operator::ShiftLeftEquals:		return "<<=";
		case Operator::ShiftRightEquals:	return ">>=";
		case Operator::BitwiseAndEquals:	return "&=";
		case Operator::BitwiseOrEquals:		return "|=";
		case Operator::BitwiseXorEquals:	return "^=";
	}
}

fir::Type* TCS::getBinaryOpResultType(fir::Type* left, fir::Type* right, Operator op)
{
	switch(op)
	{
		case Operator::CompareEq:
		case Operator::CompareNotEq:
		case Operator::CompareGreater:
		case Operator::CompareGreaterEq:
		case Operator::CompareLess:
		case Operator::CompareLessEq:
			return fir::Type::getBool();

		case Operator::Cast:
			return right;

		case Operator::Add:
		case Operator::Subtract:
		case Operator::Multiply:
		case Operator::Divide:
		case Operator::Modulo: {

			if((left->isIntegerType() && right->isIntegerType()) || (left->isFloatingPointType() && right->isFloatingPointType()))
			{
				if(left->isConstantNumberType()) return right;

				return (left->getBitWidth() > right->getBitWidth()) ? left : right;
			}
			else if((left->isIntegerType() && right->isFloatingPointType()) || (left->isFloatingPointType() && right->isIntegerType()))
			{
				return (left->isFloatingPointType() ? left : right);
			}
		} break;


		default:
			error("not supported");
	}

	return 0;
}



sst::Stmt* ast::BinaryOp::typecheck(TCS* fs, fir::Type* inferred)
{
	// TODO: infer the types properly for literal numbers
	// this has always been a thorn, dammit

	auto l = dcast(sst::Expr, this->left->typecheck(fs, inferred));
	auto r = dcast(sst::Expr, this->right->typecheck(fs, inferred));

	if(!l)	error(l, "Statement cannot be used as an expression");
	if(!r)	error(r, "Statement cannot be used as an expression");

	auto lt = l->type;
	auto rt = r->type;

	fir::Type* rest = fs->getBinaryOpResultType(lt, rt, this->op);
	if(!rest)
	{
		HighlightOptions ho;
		ho.caret = this->loc;
		ho.underlines.push_back(this->left->loc);
		ho.underlines.push_back(this->right->loc);

		ho.drawCaret = true;
		error(this, ho, "Unsupported operator '%s' between types '%s' and '%s'", operatorToString(this->op), lt->str(), rt->str());
	}

	auto ret = new sst::BinaryOp(this->loc);

	ret->left = dynamic_cast<sst::Expr*>(l);
	ret->right = dynamic_cast<sst::Expr*>(r);
	ret->op = this->op;
	ret->type = rest;

	return ret;
}

sst::Stmt* ast::UnaryOp::typecheck(TCS* fs, fir::Type* inferred)
{
	auto v = dcast(sst::Expr, this->expr->typecheck(fs, inferred));
	if(!v) error(v, "Statement cannot be used as an expression");

	auto t = v->type;
	fir::Type* out = 0;
	switch(this->op)
	{
		case Operator::LogicalNot: {
			// check if we're convertible to bool
			if(t != fir::Type::getBool())
				error(this, "Invalid use of logical-not-operator '!' on non-boolean type '%s'", t->str());

			out = fir::Type::getBool();
		} break;

		case Operator::Plus:
		case Operator::Minus: {
			if(!t->isIntegerType() && !t->isFloatingPointType())
				error(this, "Invalid use of unary plus/minus operator '+'/'-' on non-numerical type '%s'", t->str());

			if(op == Operator::Minus && t->isIntegerType() && !t->isSignedIntType())
				error(this, "Invalid use of unary negation operator '-' on unsigned integer type '%s'", t->str());

			out = t;
		} break;

		case Operator::BitwiseNot: {
			if(!t->isIntegerType())
				error(this, "Invalid use of bitwise not operator '~' on non-integer type '%s'", t->str());

			if(t->isSignedIntType())
				error(this, "Invalid use of bitwise not operator '~' on signed integer type '%s'", t->str());

			out = t;
		} break;

		case Operator::Dereference: {
			if(!t->isPointerType())
				error(this, "Invalid use of derefernce operator '*' on non-pointer type '%s'", t->str());

			out = t->getPointerElementType();
		} break;

		case Operator::AddressOf: {
			out = t->getPointerTo();
		} break;

		default:
			error(this, "not a unary op???");
	}

	auto ret = new sst::UnaryOp(this->loc);
	ret->op = this->op;
	ret->type = out;
	ret->expr = v;

	return ret;
}











