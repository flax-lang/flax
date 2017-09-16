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
	bool constable(Operator op)
	{
		switch(op)
		{
			case Operator::Add:
			case Operator::Subtract:
			case Operator::Multiply:
			case Operator::Divide:
			case Operator::CompareEq:
			case Operator::CompareNotEq:
			case Operator::CompareGreater:
			case Operator::CompareGreaterEq:
			case Operator::CompareLess:
			case Operator::CompareLessEq:
			case Operator::Modulo:
				return true;

			default:
				return false;
		}
	}

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

			default:
				error("not supported in const op");
		}
	}


	CGResult BinaryOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
	{
		iceAssert(!isAssignOp(this->op));

		if(this->op == Operator::Cast)
		{
			auto target = this->right->codegen(cs).value->getType();
			auto value = this->left->codegen(cs).value;
			auto vt = value->getType();

			if(vt->isConstantNumberType() && (target->isFloatingPointType() || target->isIntegerType()))
			{
				auto cn = dcast(fir::ConstantNumber, value);
				if(!cn) error(this->left, "what");

				return CGResult(cs->unwrapConstantNumber(cn->getValue(), target));
				// return _csdoConstantCast(this, cn, target);
			}
			else
			{
				auto res = cs->irb.CreateAppropriateCast(value, target);

				if(!res)
				{
					error(this, "No appropriate cast from type '%s' to '%s'; use 'as!' to force a bitcast",
						vt->str(), target->str());
				}

				return CGResult(res);
			}
		}


		if(this->op == Operator::LogicalAnd || this->op == Operator::LogicalOr)
			return cs->performLogicalBinaryOperation(this);


		// TODO: figure out a better way
		auto _lr = this->left->codegen(cs/*, inferred*/);
		auto _rr = this->right->codegen(cs/*, inferred*/);

		auto [ l, r ] = cs->autoCastValueTypes(_lr, _rr);
		if(!l.value || !r.value)
		{
			error(this, "Unsupported operator '%s' on types '%s' and '%s'", operatorToString(this->op), _lr.value->getType()->str(),
				_rr.value->getType()->str());
		}

		auto lt = l.value->getType();
		auto rt = r.value->getType();

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
			auto lcn = dcast(fir::ConstantNumber, l.value);
			auto rcn = dcast(fir::ConstantNumber, r.value);

			iceAssert(lcn && rcn);
			auto lnum = lcn->getValue();
			auto rnum = rcn->getValue();

			{
				if(!constable(this->op))
				{
					error(this, "Could not infer appropriate type for operator '%s' between literal numbers",
						operatorToString(this->op));
				}

				auto res = doConstantThings(lnum, rnum, this->op);
				if(isCompareOp(op))
					return CGResult(fir::ConstantInt::getBool((bool) res));

				else
					return CGResult(fir::ConstantNumber::get(res));
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

		// HighlightOptions hs;
		// hs.underlines.push_back(this->expr->loc);
		// warn(this, hs, "");

		switch(this->op)
		{
			case Operator::LogicalNot: {
				iceAssert(ty->isBoolType());
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
				if(auto ci = dcast(fir::ConstantInt, val))
				{
					iceAssert(ci->getType()->isSignedIntType());
					return CGResult(fir::ConstantInt::get(ci->getType(), -1 * ci->getSignedValue()));
				}
				else if(auto cf = dcast(fir::ConstantFP, val))
				{
					return CGResult(fir::ConstantFP::get(cf->getType(), -1 * cf->getValue()));
				}
				else if(auto cn = dcast(fir::ConstantNumber, val))
				{
					return CGResult(fir::ConstantNumber::get(-1 * cn->getValue()));
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
				if(!ex.pointer || ex.kind == CGResult::VK::RValue)
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






















