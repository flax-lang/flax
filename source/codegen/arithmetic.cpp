// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

namespace sst
{
	template <typename T>
	static T doConstantThings(T l, T r, Operator op)
	{
		switch(op)
		{
			case Operator::Invalid:				error("invalid op");
			case Operator::Add:					return l + r;
			case Operator::Subtract:			return l - r;
			case Operator::Multiply:			return l * r;
			case Operator::Divide:				return l / r;
			case Operator::CompareEq:			return l == r;
			case Operator::CompareNotEq:		return l != r;
			case Operator::CompareGreater:		return l > r;
			case Operator::CompareGreaterEq:	return l >= r;
			case Operator::CompareLess:			return l < r;
			case Operator::CompareLessEq:		return l <= r;

			case Operator::Modulo:				return l % r;
			case Operator::BitwiseOr:			return l | r;
			case Operator::BitwiseAnd:			return l & r;
			case Operator::BitwiseXor:			return l ^ r;
			case Operator::ShiftLeft:			return l << r;
			case Operator::ShiftRight:			return l >> r;

			default:
				error("not supported in const op");
		}
	}

	template <typename T>
	static T doConstantFPThings(T l, T r, Operator op)
	{
		switch(op)
		{
			case Operator::Invalid:				error("invalid op");
			case Operator::Add:					return l + r;
			case Operator::Subtract:			return l - r;
			case Operator::Multiply:			return l * r;
			case Operator::Divide:				return l / r;
			case Operator::CompareEq:			return l == r;
			case Operator::CompareNotEq:		return l != r;
			case Operator::CompareGreater:		return l > r;
			case Operator::CompareGreaterEq:	return l >= r;
			case Operator::CompareLess:			return l < r;
			case Operator::CompareLessEq:		return l <= r;

			default:
				error("not supported in const op");
		}
	}

	static bool isBitwiseOp(Operator op)
	{
		switch(op)
		{
			case Operator::Modulo:
			case Operator::BitwiseOr:
			case Operator::BitwiseAnd:
			case Operator::BitwiseXor:
			case Operator::ShiftLeft:
			case Operator::ShiftRight:
				return true;

			default:
				return false;
		}
	}

	static bool isAssignOp(Operator op)
	{
		switch(op)
		{
			case Operator::Assign:
			case Operator::PlusEquals:
			case Operator::MinusEquals:
			case Operator::MultiplyEquals:
			case Operator::DivideEquals:
			case Operator::ModuloEquals:
			case Operator::ShiftLeftEquals:
			case Operator::ShiftRightEquals:
			case Operator::BitwiseAndEquals:
			case Operator::BitwiseOrEquals:
			case Operator::BitwiseXorEquals:
				return true;

			default:
				return false;
		}
	}

	static bool isCompareOp(Operator op)
	{
		switch(op)
		{
			case Operator::CompareEq:
			case Operator::CompareNotEq:
			case Operator::CompareGreater:
			case Operator::CompareGreaterEq:
			case Operator::CompareLess:
			case Operator::CompareLessEq:
				return true;

			default:
				return false;
		}
	}






	CGResult BinaryOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
	{
		// TODO: figure out a better way
		auto _lr = this->left->codegen(cs/*, inferred*/);
		auto _rr = this->right->codegen(cs/*, inferred*/);


		if(isAssignOp(this->op) || this->op == Operator::Cast || this->op == Operator::LogicalAnd
			|| this->op == Operator::LogicalNot || this->op == Operator::LogicalOr)
		{
			error("not supported");
		}

		// handle pointer arithmetic
		if((_lr.value->getType()->isPointerType() && _rr.value->getType()->isIntegerType())
			|| (_lr.value->getType()->isIntegerType() && _rr.value->getType()->isPointerType()))
		{
			error("not supported");
		}



		auto [ l, r ] = cs->autoCastValueTypes(_lr, _rr);
		auto lt = l.value->getType();
		auto rt = r.value->getType();

		if(!l.value || !r.value)
		{
			error(this, "Unsupported operator '%s' on types '%s' and '%s'", operatorToString(this->op), _lr.value->getType()->str(),
				_rr.value->getType()->str());
		}



		if(isCompareOp(this->op))
		{
			// do comparison
			if((lt->isIntegerType() && rt->isIntegerType()) || (lt->isPointerType() && rt->isPointerType()))
			{
				switch(this->op)
				{
					case Operator::CompareEq:			return CGResult(cs->irb.CreateICmpEQ(l.value, r.value));
					case Operator::CompareNotEq:		return CGResult(cs->irb.CreateICmpNEQ(l.value, r.value));
					case Operator::CompareGreater:		return CGResult(cs->irb.CreateICmpGT(l.value, r.value));
					case Operator::CompareGreaterEq:	return CGResult(cs->irb.CreateICmpGEQ(l.value, r.value));
					case Operator::CompareLess:			return CGResult(cs->irb.CreateICmpLT(l.value, r.value));
					case Operator::CompareLessEq:		return CGResult(cs->irb.CreateICmpLEQ(l.value, r.value));
					default: iceAssert(0);
				}
			}
			else if(lt->isFloatingPointType() && rt->isFloatingPointType())
			{
				switch(this->op)
				{
					case Operator::CompareEq:			return CGResult(cs->irb.CreateFCmpEQ_ORD(l.value, r.value));
					case Operator::CompareNotEq:		return CGResult(cs->irb.CreateFCmpNEQ_ORD(l.value, r.value));
					case Operator::CompareGreater:		return CGResult(cs->irb.CreateFCmpGT_ORD(l.value, r.value));
					case Operator::CompareGreaterEq:	return CGResult(cs->irb.CreateFCmpGEQ_ORD(l.value, r.value));
					case Operator::CompareLess:			return CGResult(cs->irb.CreateFCmpLT_ORD(l.value, r.value));
					case Operator::CompareLessEq:		return CGResult(cs->irb.CreateFCmpLEQ_ORD(l.value, r.value));
					default: iceAssert(0);
				}
			}
		}




		// circumvent the '1 + 2 + 3'-expression-has-no-type issue by just computing whatever
		// since both sides are constants, all should work just fine
		if(lt->isConstantNumberType() && rt->isConstantNumberType())
		{
			if(lt->isConstantIntType() && rt->isConstantIntType())
			{
				// todo: is this the best way?
				// note: convert unsigned types to signed types if one of the operands is signed
				// then complain on overflow
				if(lt->toPrimitiveType()->isSigned() || rt->toPrimitiveType()->isSigned())
				{
					int64_t a = 0;
					if(lt->toPrimitiveType()->isSigned())
					{
						a = dcast(fir::ConstantInt, l.value)->getSignedValue();
					}
					else
					{
						auto val = dcast(fir::ConstantInt, l.value)->getUnsignedValue();
						if(val > INT64_MAX)
							error(this, "Unsigned integer literal '%s' will overflow when converted to a signed type", std::to_string(val));

						a = (int64_t) val;
					}


					int64_t b = 0;
					if(rt->toPrimitiveType()->isSigned())
					{
						b = dcast(fir::ConstantInt, r.value)->getSignedValue();
					}
					else
					{
						auto val = dcast(fir::ConstantInt, r.value)->getUnsignedValue();
						if(val > INT64_MAX)
							error(this, "Unsigned integer literal '%s' will overflow when converted to a signed type", std::to_string(val));

						b = (int64_t) val;
					}

					auto ret = doConstantThings(a, b, this->op);
					auto t = inferred;
					if(t && !t->isIntegerType())
						error(this, "Inferred non-integer type ('%s') for integer expression", t->str());

					return CGResult(fir::ConstantInt::get(t ? t : fir::PrimitiveType::getConstantSignedInt(), ret));
				}
				else
				{
					uint64_t a = dcast(fir::ConstantInt, l.value)->getUnsignedValue();
					uint64_t b = dcast(fir::ConstantInt, r.value)->getUnsignedValue();

					auto ret = doConstantThings(a, b, this->op);
					auto t = inferred;
					if(t && !t->isIntegerType())
						error(this, "Inferred non-integer type ('%s') for integer expression", t->str());

					return CGResult(fir::ConstantInt::get(t ? t : fir::PrimitiveType::getConstantUnsignedInt(), ret));
				}
			}
			else if(lt->isConstantFloatType() && rt->isConstantFloatType())
			{
				long double a = dcast(fir::ConstantFP, l.value)->getValue();
				long double b = dcast(fir::ConstantFP, r.value)->getValue();

				if(isBitwiseOp(this->op))
					error(this, "Bitwise operations are not supported on floating-point types");

				auto ret = doConstantFPThings(a, b, this->op);
				auto t = inferred;
				if(t && !t->isFloatingPointType())
					error(this, "Inferred non floating-point type ('%s') for floating-point expression", t->str());

				return CGResult(fir::ConstantFP::get(t ? t : fir::PrimitiveType::getConstantFloat(), ret));
			}
			else
			{
				error(this, "how?");
			}
		}


		if(lt->isPrimitiveType() && rt->isPrimitiveType())
			return CGResult(cs->irb.CreateBinaryOp(this->op, l.value, r.value));

		else
			error(this, "not supported");

		return CGResult(0);
	}
}
