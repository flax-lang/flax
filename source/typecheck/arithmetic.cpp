// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "memorypool.h"


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
			int dist = sst::getOverloadDistance(zfu::map(ovp->params, [](const auto& p) { return p.type; }), args);
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
				auto err = SimpleError::make(loc, "ambiguous use of overloaded operator '%s'", op);

				for(auto c : cands)
					err->append(SimpleError::make(MsgType::Note, c->loc, "potential overload candidate here:"));

				err->postAndQuit();
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
	else if(op == Operator::CompareEQ || op == Operator::CompareNEQ)
	{
		if(left == right || fir::getCastDistance(left, right) >= 0 || fir::getCastDistance(right, left) >= 0
			|| (left->isConstantNumberType() && right->isConstantNumberType()))
		{
			return fir::Type::getBool();
		}
	}
	else if(op == Operator::CompareLT || op == Operator::CompareGT || op == Operator::CompareLEQ || op == Operator::CompareGEQ)
	{
		// we handle this separately because we only want to check for number types and string types.
		bool ty_compat = (left == right || fir::getCastDistance(left, right) >= 0 || fir::getCastDistance(right, left) >= 0
			|| (left->isConstantNumberType() && right->isConstantNumberType()));

		bool ty_comparable = (left->isStringType() || left->isArraySliceType() || left->isArrayType() || left->isDynamicArrayType()
			|| left->isPrimitiveType() || left->isEnumType() || left->isConstantNumberType());

		if(ty_compat && ty_comparable)
		{
			return fir::Type::getBool();
		}
	}
	else if(op == Operator::TypeIs)
	{
		return fir::Type::getBool();
	}
	else if(op == Operator::TypeCast)
	{
		if(right->isUnionVariantType())
			return right->toUnionVariantType()->getInteriorType();

		return right;
	}
	else if(op == Operator::Plus)
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::unifyConstantTypes(left->toConstantNumberType(), right->toConstantNumberType());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if(left->isStringType() && (right->isStringType() || right->isCharSliceType() || right->isCharType()))
			return fir::Type::getString();

		else if(left->isDynamicArrayType() && right->isDynamicArrayType() && left == right)
			return left;

		else if(left->isDynamicArrayType() && left->getArrayElementType() == right)
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
			return fir::unifyConstantTypes(left->toConstantNumberType(), right->toConstantNumberType());

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
			return fir::unifyConstantTypes(left->toConstantNumberType(), right->toConstantNumberType());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);

	}
	else if(op == Operator::Divide)
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
			return fir::unifyConstantTypes(left->toConstantNumberType(), right->toConstantNumberType());

		else if(left->isPrimitiveType() && right->isPrimitiveType() && left == right)
			return left;

		else if((left->isConstantNumberType() && right->isPrimitiveType()) || (left->isPrimitiveType() && right->isConstantNumberType()))
			return (left->isConstantNumberType() ? right : left);
	}
	else if(op == Operator::Modulo)
	{
		if(left->isConstantNumberType() && right->isConstantNumberType())
		{
			return fir::unifyConstantTypes(left->toConstantNumberType(), right->toConstantNumberType());
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
	else if(zfu::match(op, Operator::BitwiseOr, Operator::BitwiseAnd, Operator::BitwiseXor))
	{
		if(left == right)
			return left;
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
	fs->pushLoc(this);
	defer(fs->popLoc());

	iceAssert(!Operator::isAssignment(this->op));

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
				warn(this, "redundant cast: type '%s' is already %smutable", l->type, mte->mut ? "" : "im");

			if(mte->mut)    r = sst::TypeExpr::make(mte->loc, l->type->getMutablePointerVersion());
			else            r = sst::TypeExpr::make(mte->loc, l->type->getImmutablePointerVersion());
		}
		else if(l->type->isArraySliceType())
		{
			if(l->type->toArraySliceType()->isMutable() == mte->mut)
				warn(this, "redundant cast: type '%s' is already %smutable", l->type, mte->mut ? "" : "im");

			r = sst::TypeExpr::make(mte->loc, fir::ArraySliceType::get(l->type->getArrayElementType(), mte->mut));
		}
		else
		{
			error(this, "invalid cast: type '%s' does not distinguish between mutable and immutable variants", l->type);
		}
	}
	else
	{
		this->right->checkAsType = (this->op == Operator::TypeCast || this->op == Operator::TypeIs);
		if(this->right->checkAsType && l->type->isUnionType())
			inferred = l->type;

		r = this->right->typecheck(fs, inferred).expr();

		if(this->right->checkAsType)
			r = util::pool<sst::TypeExpr>(r->loc, r->type);

		// final check -- see if we're trying to unwrap to a union variant with no value (ie. a void type!)
		// note: this is valid if we're just checking 'if' -- but not if we're casting (ie unwrapping!)
		if(this->op == Operator::TypeCast && l->type->isUnionType() && r->type->isUnionVariantType()
			&& r->type->toUnionVariantType()->getInteriorType()->isVoidType())
		{
			error(this->right, "unwrapping a value (of union type '%s') to the variant '%s' does not yield a value (the variant has no data)",
				l->type, r->type->toUnionVariantType()->getName());
		}
	}

	iceAssert(l && r);

	auto lt = l->type;
	auto rt = r->type;

	sst::FunctionDefn* overloadFn = 0;

	fir::Type* rest = fs->getBinaryOpResultType(lt, rt, this->op, &overloadFn);
	if(!rest)
	{
		SpanError::make(SimpleError::make(this->loc, "unsupported operator '%s' between types '%s' and '%s'", this->op, lt, rt))
			->add(util::ESpan(this->left->loc, strprintf("type '%s'", lt)))
			->add(util::ESpan(this->right->loc, strprintf("type '%s'", rt)))
			->postAndQuit();
	}

	auto ret = util::pool<sst::BinaryOp>(this->loc, rest);

	ret->left = dcast(sst::Expr, l);
	ret->right = dcast(sst::Expr, r);
	ret->op = this->op;

	ret->overloadedOpFunction = overloadFn;

	return TCResult(ret);
}

TCResult ast::UnaryOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto v = this->expr->typecheck(fs, inferred).expr();

	auto t = v->type;
	fir::Type* out = 0;

	// check for custom ops first, i guess.
	{
		auto oper = getOverloadedOperator(fs, this->loc, this->isPostfix ? 2 : 1, this->op, { t });
		if(oper)
		{
			auto ret = util::pool<sst::UnaryOp>(this->loc, oper->returnType);
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
			error(this, "invalid use of logical-not-operator '!' on non-boolean type '%s'", t);

		out = fir::Type::getBool();
	}
	else if(this->op == Operator::UnaryPlus || this->op == Operator::UnaryMinus)
	{
		if(t->isConstantNumberType())
		{
			out = (op == "-" ? fir::ConstantNumberType::get(t->toConstantNumberType()->isSigned(),
				t->toConstantNumberType()->isFloating(), t->toConstantNumberType()->getMinBits()) : t);
		}
		else if(!t->isIntegerType() && !t->isFloatingPointType())
		{
			error(this, "invalid use of unary plus/minus operator '+'/'-' on non-numerical type '%s'", t);
		}
		else if(op == "-" && t->isIntegerType() && !t->isSignedIntType())
		{
			error(this, "invalid use of unary negation operator '-' on unsigned integer type '%s'", t);
		}
		out = t;
	}
	else if(this->op == Operator::BitwiseNot)
	{
		if(t->isConstantNumberType())
			error(this, "bitwise operations are not supported on literal numbers");

		else if(!t->isIntegerType())
			error(this, "invalid use of bitwise not operator '~' on non-integer type '%s'", t);

		else if(t->isSignedIntType())
			error(this, "invalid use of bitwise not operator '~' on signed integer type '%s'", t);

		out = t;
	}
	else if(this->op == Operator::PointerDeref)
	{
		if(!t->isPointerType())
			error(this, "invalid use of dereference operator '*' on non-pointer type '%s'", t);

		out = t->getPointerElementType();
	}
	else if(this->op == Operator::AddressOf)
	{
		if(t->isFunctionType())
			error(this, "cannot take the address of a function; use it as a value type");

		out = t->getPointerTo();
	}
	else
	{
		error(this, "unsupported unary operator '%s' on type '%s'", this->op, v->type);
	}


	auto ret = util::pool<sst::UnaryOp>(this->loc, out);
	ret->op = this->op;
	ret->expr = v;

	return TCResult(ret);
}







TCResult ast::ComparisonOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	iceAssert(this->exprs.size() == this->ops.size() + 1);

	/*
		basically, we transform us into a series of chained "&&" binops.
		eg:

		10 < 20 < 30 > 25 > 15

		becomes

		(10 < 20) && (20 < 30) && (30 > 25) && (25 > 15)
	*/

	std::vector<std::pair<BinaryOp*, Location>> bins;

	// loop till the second last.
	for(size_t i = 0; i < this->exprs.size() - 1; i++)
	{
		auto left = this->exprs[i];
		auto right = this->exprs[i + 1];

		auto op = this->ops[i];
		bins.push_back({ util::pool<BinaryOp>(op.second, op.first, left, right), op.second });
	}
	iceAssert(bins.size() > 0);


	// we handle single-comparisons too, so make sure to account for that.
	if(bins.size() == 1)
	{
		return bins[0].first->typecheck(fs, inferred);
	}
	else
	{
		// make a binop combining everything, left-associatively.
		iceAssert(bins.size() > 1);
		BinaryOp* lhs = util::pool<BinaryOp>(bins[0].second, Operator::LogicalAnd, bins[0].first, bins[1].first);

		for(size_t i = 2; i < bins.size(); i++)
			lhs = util::pool<BinaryOp>(bins[i].second, Operator::LogicalAnd, lhs, bins[i].first);

		return lhs->typecheck(fs, inferred);
	}
}




































