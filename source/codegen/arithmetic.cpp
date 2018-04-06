// arithmetic.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"
#include "typecheck.h"

namespace sst
{
	// not a constable, but whether it's "const"-able
	static bool constable(const std::string& op)
	{
		return Operator::isArithmetic(op) || Operator::isComparison(op);
	}

	template <typename T>
	static T doConstantThings(T l, T r, std::string op)
	{
		if(op == Operator::Plus)            return l + r;
		else if(op == Operator::Minus)      return l - r;
		else if(op == Operator::Multiply)   return l * r;
		else if(op == Operator::Divide)     return l / r;
		else if(op == Operator::CompareEQ)  return l == r;
		else if(op == Operator::CompareNEQ) return l != r;
		else if(op == Operator::CompareLT)  return l < r;
		else if(op == Operator::CompareGT)  return l > r;
		else if(op == Operator::CompareLEQ) return l <= r;
		else if(op == Operator::CompareGEQ) return l >= r;

		error("not supported in const op");
	}


	CGResult BinaryOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
	{
		iceAssert(!Operator::isAssignment(this->op));

		if(this->op == "cast")
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
			else if(vt->isEnumType())
			{
				auto res = cs->irb.AppropriateCast(cs->irb.GetEnumCaseValue(value), target);

				if(!res)
				{
					error(this, "Case type of '%s' is '%s', cannot cast to type '%s'", vt, vt->toEnumType()->getCaseType(), target);
				}

				return CGResult(res);
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
					error(this, "Could not infer appropriate type for operator '%s' between literal numbers", this->op);
				}

				auto res = doConstantThings(lnum, rnum, this->op);
				if(Operator::isComparison(op))
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

			fir::Value* lv = cs->oneWayAutocast(l, func->getArguments()[0]->getType()).value;
			fir::Value* rv = cs->oneWayAutocast(r, func->getArguments()[0]->getType()).value;

			if(lv->getType() != func->getArguments()[0]->getType())
			{
				exitless_error(this->left, "Mismatched types for left side of overloaded binary operator '%s'; expected '%s', found '%s' instead",
					this->op, func->getArguments()[0]->getType(), lv->getType());

				info(this->overloadedOpFunction, "Operator was overloaded here:");
				doTheExit();
			}
			else if(rv->getType() != func->getArguments()[1]->getType())
			{
				exitless_error(this->right, "Mismatched types for right side of overloaded binary operator '%s'; expected '%s', found '%s' instead",
					this->op, func->getArguments()[1]->getType(), rv->getType());

				info(this->overloadedOpFunction, "Operator was overloaded here:");
				doTheExit();
			}

			// ok, call that guy.
			return CGResult(cs->irb.Call(func, lv, rv));
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

			val = cs->oneWayAutocast(ex, func->getArguments()[0]->getType()).value;

			if(val->getType() != func->getArguments()[0]->getType())
			{
				exitless_error(this->expr, "Mismatched types for overloaded unary operator '%s'; expected '%s', found '%s' instead",
					this->op, func->getArguments()[0]->getType(), val->getType());

				info(this->overloadedOpFunction, "Operator was overloaded here:");
				doTheExit();
			}

			// ok, call that guy.
			return CGResult(cs->irb.Call(func, val));
		}


		if(this->op == Operator::LogicalNot)
		{
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
		}
		else if(this->op == Operator::UnaryPlus)
		{
			return ex;
		}
		else if(this->op == Operator::UnaryMinus)
		{
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
		}
		else if(this->op == Operator::PointerDeref)
		{
			iceAssert(ty->isPointerType());
			return CGResult(cs->irb.Load(val), val, CGResult::VK::LValue);
		}
		else if(this->op == Operator::AddressOf)
		{
			if(ex.kind != CGResult::VK::LValue)
				error(this, "Cannot take address of a non-lvalue");

			else if(!ex.pointer && ex.value->getType()->isFunctionType())
				error(this, "Cannot take the address of a function; use it as a value type");

			else if(!ex.pointer)
				error(this, "Have lvalue without storage?");

			return CGResult(ex.pointer);
		}
		else if(this->op == Operator::BitwiseNot)
		{
			iceAssert(ty->isIntegerType() && !ty->isSignedIntType());
			if(auto ci = dcast(fir::ConstantInt, val))
			{
				return CGResult(fir::ConstantInt::get(ci->getType(), ~(ci->getUnsignedValue())));
			}
			else
			{
				return CGResult(cs->irb.BitwiseNOT(val));
			}
		}

		error(this, "not a unary op");
	}
}



namespace cgn
{
	CGResult CodegenState::performBinaryOperation(const Location& loc, std::pair<Location, CGResult> lhs,
		std::pair<Location, CGResult> rhs, std::string op)
	{
		auto unsupportedError = [loc, op](const Location& al, fir::Type* a, const Location& bl, fir::Type* b) {

			HighlightOptions ho;
			ho.caret = loc;
			ho.underlines.push_back(al);
			ho.underlines.push_back(bl);

			ho.drawCaret = true;
			error(loc, ho, "Unsupported operator '%s' between types '%s' and '%s'", op, a, b);
		};


		auto l = lhs.second;
		auto r = rhs.second;

		auto lt = l.value->getType();
		auto rt = r.value->getType();

		auto [ lv, lp ] = std::make_pair(l.value, l.pointer);
		auto [ rv, rp ] = std::make_pair(r.value, r.pointer);


		if(Operator::isComparison(op))
		{
			// do comparison
			if((lt->isIntegerType() && rt->isIntegerType()) || (lt->isPointerType() && rt->isPointerType())
				|| (lt->isCharType() && rt->isCharType()))
			{
				// we should cast these to be similar-ish.
				auto [ a, b ] = this->autoCastValueTypes(l, r);
				lv = a.value;
				rv = b.value;

				if(op == Operator::CompareEQ)   return CGResult(this->irb.ICmpEQ(lv, rv));
				if(op == Operator::CompareNEQ)  return CGResult(this->irb.ICmpNEQ(lv, rv));
				if(op == Operator::CompareLT)   return CGResult(this->irb.ICmpLT(lv, rv));
				if(op == Operator::CompareLEQ)  return CGResult(this->irb.ICmpLEQ(lv, rv));
				if(op == Operator::CompareGT)   return CGResult(this->irb.ICmpGT(lv, rv));
				if(op == Operator::CompareGEQ)  return CGResult(this->irb.ICmpGEQ(lv, rv));

				error("no");
			}
			else if((lt->isPrimitiveType() && rt->isConstantNumberType()) || (lt->isConstantNumberType() && rt->isPrimitiveType()))
			{
				auto [ lr, rr ] = this->autoCastValueTypes(l, r);
				iceAssert(lr.value && rr.value);

				if(lr.value->getType()->isFloatingPointType())
				{
					if(op == Operator::CompareEQ)   return CGResult(this->irb.FCmpEQ_ORD(lr.value, rr.value));
					if(op == Operator::CompareNEQ)  return CGResult(this->irb.FCmpNEQ_ORD(lr.value, rr.value));
					if(op == Operator::CompareLT)   return CGResult(this->irb.FCmpLT_ORD(lr.value, rr.value));
					if(op == Operator::CompareLEQ)  return CGResult(this->irb.FCmpLEQ_ORD(lr.value, rr.value));
					if(op == Operator::CompareGT)   return CGResult(this->irb.FCmpGT_ORD(lr.value, rr.value));
					if(op == Operator::CompareGEQ)  return CGResult(this->irb.FCmpGEQ_ORD(lr.value, rr.value));

					error("no");
				}
				else
				{
					if(op == Operator::CompareEQ)   return CGResult(this->irb.ICmpEQ(lr.value, rr.value));
					if(op == Operator::CompareNEQ)  return CGResult(this->irb.ICmpNEQ(lr.value, rr.value));
					if(op == Operator::CompareLT)   return CGResult(this->irb.ICmpLT(lr.value, rr.value));
					if(op == Operator::CompareLEQ)  return CGResult(this->irb.ICmpLEQ(lr.value, rr.value));
					if(op == Operator::CompareGT)   return CGResult(this->irb.ICmpGT(lr.value, rr.value));
					if(op == Operator::CompareGEQ)  return CGResult(this->irb.ICmpGEQ(lr.value, rr.value));

					error("no");
				}
			}
			else if(lt->isStringType() && rt->isStringType())
			{
				auto cmpfn = cgn::glue::string::getCompareFunction(this);
				fir::Value* res = this->irb.Call(cmpfn, lv, rv);

				fir::Value* zero = fir::ConstantInt::getInt64(0);

				if(op == Operator::CompareEQ)   return CGResult(this->irb.ICmpEQ(res, zero));
				if(op == Operator::CompareNEQ)  return CGResult(this->irb.ICmpNEQ(res, zero));
				if(op == Operator::CompareLT)   return CGResult(this->irb.ICmpLT(res, zero));
				if(op == Operator::CompareLEQ)  return CGResult(this->irb.ICmpLEQ(res, zero));
				if(op == Operator::CompareGT)   return CGResult(this->irb.ICmpGT(res, zero));
				if(op == Operator::CompareGEQ)  return CGResult(this->irb.ICmpGEQ(res, zero));

				error("no");
			}
			else if(lt->isEnumType() && lt == rt)
			{
				auto li = this->irb.GetEnumCaseIndex(lv);
				auto ri = this->irb.GetEnumCaseIndex(rv);

				if(op == Operator::CompareEQ)   return CGResult(this->irb.ICmpEQ(li, ri));
				if(op == Operator::CompareNEQ)  return CGResult(this->irb.ICmpNEQ(li, ri));
				if(op == Operator::CompareLT)   return CGResult(this->irb.ICmpLT(li, ri));
				if(op == Operator::CompareLEQ)  return CGResult(this->irb.ICmpLEQ(li, ri));
				if(op == Operator::CompareGT)   return CGResult(this->irb.ICmpGT(li, ri));
				if(op == Operator::CompareGEQ)  return CGResult(this->irb.ICmpGEQ(li, ri));

				error("no");
			}
			else if(lt->isDynamicArrayType() && lt == rt)
			{
				//! use opf when we have operator overloads
				auto cmpfn = cgn::glue::array::getCompareFunction(this, lt->toDynamicArrayType(), 0);
				fir::Value* res = this->irb.Call(cmpfn, lv, rv);

				fir::Value* zero = fir::ConstantInt::getInt64(0);

				if(op == Operator::CompareEQ)   return CGResult(this->irb.ICmpEQ(res, zero));
				if(op == Operator::CompareNEQ)  return CGResult(this->irb.ICmpNEQ(res, zero));
				if(op == Operator::CompareLT)   return CGResult(this->irb.ICmpLT(res, zero));
				if(op == Operator::CompareLEQ)  return CGResult(this->irb.ICmpLEQ(res, zero));
				if(op == Operator::CompareGT)   return CGResult(this->irb.ICmpGT(res, zero));
				if(op == Operator::CompareGEQ)  return CGResult(this->irb.ICmpGEQ(res, zero));

				error("no");
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
				if(op != Operator::Plus)
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
				if(op != Operator::Plus)
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






















