// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)


bool isBitwiseOp(Operator op)
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

bool isAssignOp(Operator op)
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

bool isCompareOp(Operator op)
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


Operator getNonAssignOp(Operator op)
{
	switch(op)
	{
		case Operator::PlusEquals:			return Operator::Add;
		case Operator::MinusEquals:			return Operator::Subtract;
		case Operator::MultiplyEquals:		return Operator::Multiply;
		case Operator::DivideEquals:		return Operator::Divide;
		case Operator::ModuloEquals:		return Operator::Modulo;
		case Operator::ShiftLeftEquals:		return Operator::ShiftLeft;
		case Operator::ShiftRightEquals:	return Operator::ShiftRight;
		case Operator::BitwiseAndEquals:	return Operator::BitwiseAnd;
		case Operator::BitwiseOrEquals:		return Operator::BitwiseOr;
		case Operator::BitwiseXorEquals:	return Operator::BitwiseXor;

		default:
			error("no");
	}
}



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







	CGResult BinaryOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
	{
		iceAssert(!isAssignOp(this->op));

		// TODO: figure out a better way
		auto _lr = this->left->codegen(cs/*, inferred*/);
		auto _rr = this->right->codegen(cs/*, inferred*/);

		if(this->op == Operator::Cast)
		{
			auto val = _lr.value;
			if(val->getType()->isConstantIntType())
			{
				auto ci = dcast(fir::ConstantInt, val);
				iceAssert(ci);

				if(val->getType()->isSignedIntType())
				{
					val = fir::ConstantInt::getInt64(ci->getSignedValue());
				}
				else
				{
					size_t v = ci->getUnsignedValue();
					if(v <= INT64_MAX)
						val = fir::ConstantInt::getInt64(v);

					else
						val = fir::ConstantInt::getUint64(v);
				}
			}
			else if(val->getType()->isConstantFloatType())
			{
				auto cf = dcast(fir::ConstantFP, val);
				iceAssert(cf);

				val = fir::ConstantFP::getFloat80(cf->getValue());
			}


			auto target = _rr.value->getType();
			auto res = cs->irb.CreateAppropriateCast(val, target);

			if(!res)
			{
				error(this, "No appropriate cast from type '%s' to '%s'; use 'as!' to force a bitcast",
					val->getType()->str(), target->str());
			}

			return CGResult(res);
		}

		auto [ l, r ] = cs->autoCastValueTypes(_lr, _rr);
		if(!l.value || !r.value)
		{
			error(this, "Unsupported operator '%s' on types '%s' and '%s'", operatorToString(this->op), _lr.value->getType()->str(),
				_rr.value->getType()->str());
		}

		auto lt = l.value->getType();
		auto rt = r.value->getType();

		if(this->op == Operator::LogicalAnd || this->op == Operator::LogicalOr)
		{
			error("not supported");
		}

		// handle pointer arithmetic
		if((_lr.value->getType()->isPointerType() && _rr.value->getType()->isIntegerType())
			|| (_lr.value->getType()->isIntegerType() && _rr.value->getType()->isPointerType()))
		{
			error("not supported");
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
		else
		{
			return cs->performBinaryOperation(this->loc, { this->left->loc, l }, { this->right->loc, r }, this->op);
		}
	}






	CGResult UnaryOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
	{
		CGResult ex = this->expr->codegen(cs, inferred);
		auto ty = ex.value->getType();
		auto val = ex.value;

		switch(this->op)
		{
			case Operator::LogicalNot: {
				iceAssert(ty == fir::Type::getBool());
				if(auto c = dcast(fir::ConstantInt, val))
				{
					bool b = c->getSignedValue();
					return CGResult(fir::ConstantInt::getBool(!b));
				}
				else
				{
					return CGResult(cs->irb.CreateLogicalNot(val));
				}
			} break;

			case Operator::Plus:
				return ex;

			case Operator::Minus: {
				iceAssert(ty->isIntegerType() || ty->isFloatingPointType());
				if(auto ci = dcast(fir::ConstantInt, val))
				{
					iceAssert(ci->getType()->isSignedIntType());
					return CGResult(fir::ConstantInt::get(ci->getType(), -1 * ci->getSignedValue()));
				}
				else if(auto cf = dcast(fir::ConstantFP, val))
				{
					return CGResult(fir::ConstantFP::get(cf->getType(), -1 * cf->getValue()));
				}
				else
				{
					return CGResult(cs->irb.CreateNeg(val));
				}
			} break;

			case Operator::BitwiseNot: {
				iceAssert(ty->isIntegerType() && !ty->isSignedIntType());
				if(auto ci = dcast(fir::ConstantInt, val))
				{
					return CGResult(fir::ConstantInt::get(ci->getType(), ~(ci->getUnsignedValue())));
				}
				else
				{
					return CGResult(cs->irb.CreateBitwiseNOT(val));
				}
			} break;

			case Operator::Dereference: {
				iceAssert(ty->isPointerType());
				return CGResult(cs->irb.CreateLoad(val), val, CGResult::VK::LValue);
			} break;

			case Operator::AddressOf: {
				if(!ex.pointer || ex.kind != CGResult::VK::LValue)
					error(this, "Cannot take address of an rvalue");

				return CGResult(ex.pointer);
			} break;

			default:
				error(this, "not a unary op???");
		}
	}
}



namespace cgn
{
	CGResult CodegenState::performBinaryOperation(const Location& loc, std::pair<Location, CGResult> lhs,
		std::pair<Location, CGResult> rhs, Operator op)
	{
		auto l = lhs.second;
		auto r = rhs.second;

		auto lt = l.value->getType();
		auto rt = r.value->getType();

		if(isCompareOp(op))
		{
			// do comparison
			if((lt->isIntegerType() && rt->isIntegerType()) || (lt->isPointerType() && rt->isPointerType()))
			{
				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.CreateICmpEQ(l.value, r.value));
					case Operator::CompareNotEq:		return CGResult(this->irb.CreateICmpNEQ(l.value, r.value));
					case Operator::CompareGreater:		return CGResult(this->irb.CreateICmpGT(l.value, r.value));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.CreateICmpGEQ(l.value, r.value));
					case Operator::CompareLess:			return CGResult(this->irb.CreateICmpLT(l.value, r.value));
					case Operator::CompareLessEq:		return CGResult(this->irb.CreateICmpLEQ(l.value, r.value));
					default: error("no");
				}
			}
			else if(lt->isFloatingPointType() && rt->isFloatingPointType())
			{
				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.CreateFCmpEQ_ORD(l.value, r.value));
					case Operator::CompareNotEq:		return CGResult(this->irb.CreateFCmpNEQ_ORD(l.value, r.value));
					case Operator::CompareGreater:		return CGResult(this->irb.CreateFCmpGT_ORD(l.value, r.value));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.CreateFCmpGEQ_ORD(l.value, r.value));
					case Operator::CompareLess:			return CGResult(this->irb.CreateFCmpLT_ORD(l.value, r.value));
					case Operator::CompareLessEq:		return CGResult(this->irb.CreateFCmpLEQ_ORD(l.value, r.value));
					default: error("no");
				}
			}
			else
			{
				error("??");
			}
		}
		else
		{
			if(lt->isPrimitiveType() && rt->isPrimitiveType())
				return CGResult(this->irb.CreateBinaryOp(op, l.value, r.value));

			else
				error(loc, "not supported");
		}
	}
}






















