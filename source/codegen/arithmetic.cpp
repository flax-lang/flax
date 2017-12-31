// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "typecheck.h"


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
				auto res = cs->irb.AppropriateCast(value, target);

				if(!res)
				{
					error(this, "No appropriate cast from type '%s' to '%s'; use 'as!' to force a bitcast",
						vt, target);
				}

				return CGResult(res);
			}
		}


		if(this->op == Operator::LogicalAnd || this->op == Operator::LogicalOr)
			return cs->performLogicalBinaryOperation(this);


		// TODO: figure out a better way
		auto _lr = this->left->codegen(cs/*, inferred*/);
		auto _rr = this->right->codegen(cs/*, inferred*/);

		auto [ l, r ] = std::make_tuple(_lr, _rr);
		auto [ lt, rt ] = std::make_tuple(l.value->getType(), r.value->getType());


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
					return CGResult(fir::ConstantBool::get((bool) res));

				else
					return CGResult(fir::ConstantNumber::get(res));
			}
		}
		else if(this->overloadedOpFunction)
		{
			// fantastic, just call this piece of shit.
			auto func = dcast(fir::Function, this->overloadedOpFunction->codegen(cs, 0).value);
			iceAssert(func);
			iceAssert(func->getArgumentCount() == 2);

			if(lt != func->getArguments()[0]->getType())
			{
				exitless_error(this->left, "Mismatched types for left side of overloaded binary operator '%s'; expected '%s', found '%s' instead",
					operatorToString(this->op), func->getArguments()[0]->getType(), lt);

				info(this->overloadedOpFunction, "Operator was overloaded here:");
				doTheExit();
			}
			else if(rt != func->getArguments()[1]->getType())
			{
				exitless_error(this->left, "Mismatched types for right side of overloaded binary operator '%s'; expected '%s', found '%s' instead",
					operatorToString(this->op), func->getArguments()[1]->getType(), lt);

				info(this->overloadedOpFunction, "Operator was overloaded here:");
				doTheExit();
			}

			// ok, call that guy.
			return CGResult(cs->irb.Call(func, l.value, r.value));
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


		if(this->overloadedOpFunction)
		{
			// fantastic, just call this piece of shit.
			auto func = dcast(fir::Function, this->overloadedOpFunction->codegen(cs, 0).value);
			iceAssert(func);
			iceAssert(func->getArgumentCount() == 1);

			if(ty != func->getArguments()[0]->getType())
			{
				exitless_error(this->expr, "Mismatched types for overloaded unary operator '%s'; expected '%s', found '%s' instead",
					operatorToString(this->op), func->getArguments()[0]->getType(), ty);

				info(this->overloadedOpFunction, "Operator was overloaded here:");
				doTheExit();
			}

			// ok, call that guy.
			return CGResult(cs->irb.Call(func, val));
		}



		switch(this->op)
		{
			case Operator::LogicalNot: {
				iceAssert(ty->isBoolType());
				if(auto c = dcast(fir::ConstantInt, val))
				{
					bool b = c->getSignedValue();
					return CGResult(fir::ConstantBool::get(!b));
				}
				else
				{
					return CGResult(cs->irb.LogicalNot(val));
				}
			} break;

			case Operator::Add:
				return ex;

			case Operator::Subtract: {
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
					return CGResult(cs->irb.Negate(val));
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
					return CGResult(cs->irb.BitwiseNOT(val));
				}
			} break;

			case Operator::Multiply: {
				iceAssert(ty->isPointerType());
				return CGResult(cs->irb.Load(val), val, CGResult::VK::LValue);
			} break;

			case Operator::BitwiseAnd: {
				if(ex.kind != CGResult::VK::LValue)
					error(this, "Cannot take address of a non-lvalue");

				else if(!ex.pointer && ex.value->getType()->isFunctionType())
					error(this, "Cannot take the address of a function; use it as a value type");

				else if(!ex.pointer)
					error(this, "Have lvalue without storage?");

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
		auto unsupportedError = [loc, op](const Location& al, fir::Type* a, const Location& bl, fir::Type* b) {

			HighlightOptions ho;
			ho.caret = loc;
			ho.underlines.push_back(al);
			ho.underlines.push_back(bl);

			ho.drawCaret = true;
			error(loc, ho, "Unsupported operator '%s' between types '%s' and '%s'", operatorToString(op),
				a, b);
		};


		auto l = lhs.second;
		auto r = rhs.second;

		auto lt = l.value->getType();
		auto rt = r.value->getType();

		auto [ lv, lp ] = std::make_pair(l.value, l.pointer);
		auto [ rv, rp ] = std::make_pair(r.value, r.pointer);


		if(isCompareOp(op))
		{
			// do comparison
			if((lt->isIntegerType() && rt->isIntegerType()) || (lt->isPointerType() && rt->isPointerType())
				|| (lt->isCharType() && rt->isCharType()))
			{
				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.ICmpEQ(lv, rv));
					case Operator::CompareNotEq:		return CGResult(this->irb.ICmpNEQ(lv, rv));
					case Operator::CompareGreater:		return CGResult(this->irb.ICmpGT(lv, rv));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.ICmpGEQ(lv, rv));
					case Operator::CompareLess:			return CGResult(this->irb.ICmpLT(lv, rv));
					case Operator::CompareLessEq:		return CGResult(this->irb.ICmpLEQ(lv, rv));
					default: error("no");
				}
			}
			else if(lt->isFloatingPointType() && rt->isFloatingPointType())
			{
				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.FCmpEQ_ORD(lv, rv));
					case Operator::CompareNotEq:		return CGResult(this->irb.FCmpNEQ_ORD(lv, rv));
					case Operator::CompareGreater:		return CGResult(this->irb.FCmpGT_ORD(lv, rv));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.FCmpGEQ_ORD(lv, rv));
					case Operator::CompareLess:			return CGResult(this->irb.FCmpLT_ORD(lv, rv));
					case Operator::CompareLessEq:		return CGResult(this->irb.FCmpLEQ_ORD(lv, rv));
					default: error("no");
				}
			}
			else if((lt->isPrimitiveType() && rt->isConstantNumberType()) || (lt->isConstantNumberType() && rt->isPrimitiveType()))
			{
				auto [ lr, rr ] = this->autoCastValueTypes(l, r);
				iceAssert(lr.value && rr.value);

				if(lr.value->getType()->isFloatingPointType())
				{
					switch(op)
					{
						case Operator::CompareEq:			return CGResult(this->irb.FCmpEQ_ORD(lr.value, rr.value));
						case Operator::CompareNotEq:		return CGResult(this->irb.FCmpNEQ_ORD(lr.value, rr.value));
						case Operator::CompareGreater:		return CGResult(this->irb.FCmpGT_ORD(lr.value, rr.value));
						case Operator::CompareGreaterEq:	return CGResult(this->irb.FCmpGEQ_ORD(lr.value, rr.value));
						case Operator::CompareLess:			return CGResult(this->irb.FCmpLT_ORD(lr.value, rr.value));
						case Operator::CompareLessEq:		return CGResult(this->irb.FCmpLEQ_ORD(lr.value, rr.value));
						default: error("no");
					}
				}
				else
				{
					switch(op)
					{
						case Operator::CompareEq:			return CGResult(this->irb.ICmpEQ(lr.value, rr.value));
						case Operator::CompareNotEq:		return CGResult(this->irb.ICmpNEQ(lr.value, rr.value));
						case Operator::CompareGreater:		return CGResult(this->irb.ICmpGT(lr.value, rr.value));
						case Operator::CompareGreaterEq:	return CGResult(this->irb.ICmpGEQ(lr.value, rr.value));
						case Operator::CompareLess:			return CGResult(this->irb.ICmpLT(lr.value, rr.value));
						case Operator::CompareLessEq:		return CGResult(this->irb.ICmpLEQ(lr.value, rr.value));
						default: error("no");
					}
				}
			}
			else if(lt->isStringType() && rt->isStringType())
			{
				auto cmpfn = cgn::glue::string::getCompareFunction(this);
				fir::Value* res = this->irb.Call(cmpfn, lv, rv);

				fir::Value* zero = fir::ConstantInt::getInt64(0);

				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.ICmpEQ(res, zero));
					case Operator::CompareNotEq:		return CGResult(this->irb.ICmpNEQ(res, zero));
					case Operator::CompareGreater:		return CGResult(this->irb.ICmpGT(res, zero));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.ICmpGEQ(res, zero));
					case Operator::CompareLess:			return CGResult(this->irb.ICmpLT(res, zero));
					case Operator::CompareLessEq:		return CGResult(this->irb.ICmpLEQ(res, zero));
					default: error("no");
				}
			}
			else if(lt->isEnumType() && lt == rt)
			{
				auto li = this->irb.GetEnumCaseIndex(lv);
				auto ri = this->irb.GetEnumCaseIndex(rv);

				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.ICmpEQ(li, ri));
					case Operator::CompareNotEq:		return CGResult(this->irb.ICmpNEQ(li, ri));
					case Operator::CompareGreater:		return CGResult(this->irb.ICmpGT(li, ri));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.ICmpGEQ(li, ri));
					case Operator::CompareLess:			return CGResult(this->irb.ICmpLT(li, ri));
					case Operator::CompareLessEq:		return CGResult(this->irb.ICmpLEQ(li, ri));
					default: error("no");
				}
			}
			else if(lt->isDynamicArrayType() && lt == rt)
			{
				//! use opf when we have operator overloads
				auto cmpfn = cgn::glue::array::getCompareFunction(this, lt->toDynamicArrayType(), 0);
				fir::Value* res = this->irb.Call(cmpfn, lv, rv);

				fir::Value* zero = fir::ConstantInt::getInt64(0);

				switch(op)
				{
					case Operator::CompareEq:			return CGResult(this->irb.ICmpEQ(res, zero));
					case Operator::CompareNotEq:		return CGResult(this->irb.ICmpNEQ(res, zero));
					case Operator::CompareGreater:		return CGResult(this->irb.ICmpGT(res, zero));
					case Operator::CompareGreaterEq:	return CGResult(this->irb.ICmpGEQ(res, zero));
					case Operator::CompareLess:			return CGResult(this->irb.ICmpLT(res, zero));
					case Operator::CompareLessEq:		return CGResult(this->irb.ICmpLEQ(res, zero));
					default: error("no");
				}
			}
			else
			{
				error("Unsupported comparison between types '%s' and '%s'", lt, rt);
			}
		}
		else
		{
			if((lt->isPrimitiveType() || lt->isConstantNumberType()) && (rt->isPrimitiveType() || rt->isConstantNumberType()))
			{
				auto [ lr, rr ] = this->autoCastValueTypes(l, r);

				return CGResult(this->irb.BinaryOp(op, lr.value, rr.value));
			}
			else if((lt->isPointerType() && (rt->isIntegerType() || rt->isConstantNumberType()))
				|| ((lt->isIntegerType() || lt->isConstantNumberType()) && rt->isPointerType()))
			{
				auto ofsv = (lt->isPointerType() ? rv : lv);
				auto ofs = this->oneWayAutocast(CGResult(ofsv), fir::Type::getInt64()).value;

				iceAssert(ofs->getType()->isIntegerType());

				auto ptr = (lt->isPointerType() ? lv : rv);
				ptr = this->irb.PointerAdd(ptr, ofs);

				return CGResult(ptr);
			}
			else if(lt->isStringType() && rt->isStringType())
			{
				if(op != Operator::Add)
					unsupportedError(lhs.first, lt, rhs.first, rt);

				#if 0
				// ok.
				// if we're both string literals, then fuck it, do it compile-time
				if(dcast(fir::ConstantString, lv) && dcast(fir::ConstantString, rv))
				{
					std::string cls = dcast(fir::ConstantString, lv)->getValue();
					std::string crs = dcast(fir::ConstantString, rv)->getValue();

					info(loc, "const strings");
					return CGResult(fir::ConstantString::get(cls + crs));
				}
				#endif


				auto appfn = cgn::glue::string::getAppendFunction(this);
				auto res = this->irb.Call(appfn, lv, rv);
				this->addRefCountedValue(res);

				return CGResult(res);
			}
			else if((lt->isStringType() && rt->isCharType()) || (lt->isCharType() && rt->isStringType()))
			{
				// make life easier
				if(lt->isCharType())
				{
					std::swap(lt, rt);
					std::swap(lv, rv);
				}


				#if 0
				if(dcast(fir::ConstantString, lv) && dcast(fir::ConstantChar, rv))
				{
					std::string cls = dcast(fir::ConstantString, lv)->getValue();
					char crs = dcast(fir::ConstantChar, rv)->getValue();

					info(loc, "const strings");
					return CGResult(fir::ConstantString::get(cls + crs));
				}
				#endif


				auto appfn = cgn::glue::string::getCharAppendFunction(this);
				auto res = this->irb.Call(appfn, lv, rv);
				this->addRefCountedValue(res);

				return CGResult(res);
			}
			else if(lt->isDynamicArrayType() && rt->isDynamicArrayType() && lt->getArrayElementType() == rt->getArrayElementType())
			{
				// check what we're doing
				if(op != Operator::Add)
					unsupportedError(lhs.first, lt, rhs.first, rt);

				// ok, do the append
				auto maketwof = cgn::glue::array::getConstructFromTwoFunction(this, lt->toDynamicArrayType());

				fir::Value* res = this->irb.Call(maketwof, lv, rv);
				this->addRefCountedValue(res);

				return CGResult(res);

				// error(loc, "i'm gonna stop you right here");
			}
			else
			{
				unsupportedError(lhs.first, lt, rhs.first, rt);
				doTheExit();
			}
		}
	}
}






















