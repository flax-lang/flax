// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"



static sst::FunctionDefn* getOverloadedOperator(sst::TypecheckState* fs, const Location& loc, int kind, std::string op,
	std::vector<fir::Type*> args)
{
	auto tree = fs->stree;
	while(tree)
	{
		int best = 10000000;
		std::vector<sst::FunctionDefn*> cands;

		auto thelist = (kind == 0 ? &tree->infixOperatorOverloads : (kind == 1
			? &tree->prefixOperatorOverloads : &tree->postfixOperatorOverloads));

		for(auto ovp : (*thelist)[op])
		{
			int dist = fs->getOverloadDistance(util::map(ovp->params, [](auto p) { return p.type; }), args);
			if(dist == -1) continue;

			if(dist == best)
			{
				cands.push_back(ovp);
			}
			else if(dist < best)
			{
				best = dist;

				cands.clear();
				cands.push_back(ovp);
			}
		}

		if(cands.size() > 0)
		{
			if(cands.size() > 1)
			{
				auto err = SimpleError::make(loc, "Ambiguous use of overloaded operator '%s'", op);

				for(auto c : cands)
					err.append(SimpleError::make(MsgType::Note, c, "Potential overload candidate here:"));

				err.postAndQuit();
			}
			else
			{
				return cands[0];
			}
		}

		// only go up if we didn't find anything here.
		tree = tree->parent;
	}

	return 0;
}



fir::Type* sst::TypecheckState::getBinaryOpResultType(fir::Type* left, fir::Type* right, const std::string& op, sst::FunctionDefn** overloadFn)
{
	if(op == Operator::LogicalOr || op == Operator::LogicalAnd || op == Operator::LogicalNot)
	{
		return fir::Type::getBool();
	}
	else if(op == Operator::CompareEQ || op == Operator::CompareNEQ || op == Operator::CompareLT || op == Operator::CompareGT
		|| op == Operator::CompareLEQ || op == Operator::CompareGEQ)
	{
		return fir::Type::getBool();
	}
	else if(op == "cast")
	{
		return right;
	}
	else if(op == Operator::Plus)
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() + right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if(left->isStringType() && right->isStringType())
			return fir::Type::getString();

		else if((left->isStringType() && right->isCharType()) || (left->isCharType() && right->isStringType()))
			return fir::Type::getString();

		else if((left->isStringType() && right->isCharSliceType()) || (left->isCharSliceType() && right->isStringType()))
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
	else if(op == Operator::Minus)
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
	else if(op == Operator::Multiply)
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() * right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);

	}
	else if(op == Operator::Divide)
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::Type::getConstantNumber(left->toConstantNumberType()->getValue() / right->toConstantNumberType()->getValue());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);
	}
	else if(op == Operator::Modulo)
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
		auto oper = getOverloadedOperator(this, this->loc(), 0, op, { left, right });
		if(oper)
		{
			if(overloadFn) *overloadFn = oper;
			return oper->returnType;
		}
	}


	return 0;
}



TCResult ast::BinaryOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	iceAssert(!Operator::isAssignment(this->op));

	// TODO: infer the types properly for literal numbers
	// this has always been a thorn, dammit

	auto l = this->left->typecheck(fs, inferred).expr();

	sst::Expr* r = 0;

	//* this checks for the cast like this: `foo as mut` or `foo as !mut`
	//* the former makes an immutable thing mutable, and the latter vice versa.
	if(auto mte = dcast(MutabilityTypeExpr, this->right))
	{
		// see what the left side type is.
		if(l->type->isPointerType())
		{
			if(l->type->isMutablePointer() == mte->mut)
				warn(this, "Redundant cast: type '%s' is already %smutable", l->type, mte->mut ? "" : "im");

			if(mte->mut)    r = new sst::TypeExpr(mte->loc, l->type->getMutablePointerVersion());
			else            r = new sst::TypeExpr(mte->loc, l->type->getImmutablePointerVersion());
		}
		else if(l->type->isArraySliceType())
		{
			if(l->type->toArraySliceType()->isMutable() == mte->mut)
				warn(this, "Redundant cast: type '%s' is already %smutable", l->type, mte->mut ? "" : "im");

			r = new sst::TypeExpr(mte->loc, fir::ArraySliceType::get(l->type->getArrayElementType(), mte->mut));
		}
		else
		{
			error(this, "Invalid cast: type '%s' does not distinguish between mutable and immutable variants", l->type);
		}
	}
	else
	{
		r = this->right->typecheck(fs, inferred).expr();
	}

	iceAssert(l && r);

	auto lt = l->type;
	auto rt = r->type;

	sst::FunctionDefn* overloadFn = 0;

	fir::Type* rest = fs->getBinaryOpResultType(lt, rt, this->op, &overloadFn);
	if(!rest)
	{
		SpanError().set(SimpleError::make(this, "Unsupported operator '%s' between types '%s' and '%s'", this->op, lt, rt))
			.add(SpanError::Span(this->left->loc, strprintf("type '%s'", lt)))
			.add(SpanError::Span(this->right->loc, strprintf("type '%s'", rt)))
			.postAndQuit();
	}

	auto ret = new sst::BinaryOp(this->loc, rest);

	ret->left = dynamic_cast<sst::Expr*>(l);
	ret->right = dynamic_cast<sst::Expr*>(r);
	ret->op = this->op;

	ret->overloadedOpFunction = overloadFn;

	return TCResult(ret);
}

TCResult ast::UnaryOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	auto v = this->expr->typecheck(fs, inferred).expr();

	auto t = v->type;
	fir::Type* out = 0;

	// check for custom ops first, i guess.
	{
		auto oper = getOverloadedOperator(fs, this->loc, this->isPostfix ? 2 : 1, this->op, { t });
		if(oper)
		{
			auto ret = new sst::UnaryOp(this->loc, oper->returnType);
			ret->op = this->op;
			ret->expr = v;

			ret->overloadedOpFunction = oper;
			return TCResult(ret);
		}
	}



	if(this->op == Operator::LogicalNot)
	{
		// check if we're convertible to bool
		if(!t->isBoolType())
			error(this, "Invalid use of logical-not-operator '!' on non-boolean type '%s'", t);

		out = fir::Type::getBool();
	}
	else if(this->op == Operator::UnaryPlus || this->op == Operator::UnaryMinus)
	{
		if(t->isConstantNumberType())
			out = (op == "-" ? fir::Type::getConstantNumber(-1 * t->toConstantNumberType()->getValue()) : t);

		else if(!t->isIntegerType() && !t->isFloatingPointType())
			error(this, "Invalid use of unary plus/minus operator '+'/'-' on non-numerical type '%s'", t);

		else if(op == "-" && t->isIntegerType() && !t->isSignedIntType())
			error(this, "Invalid use of unary negation operator '-' on unsigned integer type '%s'", t);

		out = t;
	}
	else if(this->op == Operator::BitwiseNot)
	{
		if(t->isConstantNumberType())
			error(this, "Bitwise operations are not supported on literal numbers");

		else if(!t->isIntegerType())
			error(this, "Invalid use of bitwise not operator '~' on non-integer type '%s'", t);

		else if(t->isSignedIntType())
			error(this, "Invalid use of bitwise not operator '~' on signed integer type '%s'", t);

		out = t;
	}
	else if(this->op == Operator::PointerDeref)
	{
		if(!t->isPointerType())
			error(this, "Invalid use of derefernce operator '*' on non-pointer type '%s'", t);

		out = t->getPointerElementType();
	}
	else if(this->op == Operator::AddressOf)
	{
		if(t->isFunctionType())
			error(this, "Cannot take the address of a function; use it as a value type");

		out = t->getPointerTo();
	}
	else
	{
		error(this, "Unsupported unary operator '%s' on type '%s'", this->op, v->type);
	}


	auto ret = new sst::UnaryOp(this->loc, out);
	ret->op = this->op;
	ret->expr = v;

	return TCResult(ret);
}











