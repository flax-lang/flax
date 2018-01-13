// assign.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

sst::AssignOp::AssignOp(const Location& l) : Expr(l, fir::Type::getVoid()) { }
sst::TupleAssignOp::TupleAssignOp(const Location& l) : Expr(l, fir::Type::getVoid()) { }


static void _handleRCAssign(cgn::CodegenState* cs, fir::Type* lt, fir::Type* rt, CGResult lr, CGResult rr)
{
	if(cs->isRefCountedType(lt))
	{
		if(rr.kind == CGResult::VK::LValue)
			cs->performRefCountingAssignment(lr, rr, false);

		else
			cs->moveRefCountedValue(lr, rr, false);
	}
	else
	{
		cs->irb.Store(rr.value, lr.pointer);
	}
}


CGResult sst::AssignOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());

	auto lr = this->left->codegen(cs);
	auto lt = lr.value->getType();

	if(!lr.pointer || lr.kind != CGResult::VK::LValue)
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		error(this, hs, "Cannot assign to non-lvalue (most likely a temporary) expression");
	}

	if(lr.value->isImmutable() || (lr.pointer && lr.pointer->isImmutable()))
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		error(this, hs, "Cannot assign to immutable expression");
	}


	// check if we're trying to modify a literal, first of all.
	// we do it here, because we need some special sauce to do stuff
	if(auto so = dcast(SubscriptOp, this->left); so && so->cgSubscriptee->getType()->isStringType())
	{
		// yes, yes we are.
		auto checkf = cgn::glue::string::getCheckLiteralWriteFunction(cs);
		auto locstr = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.Call(checkf, so->cgSubscriptee, so->cgIndex, locstr);
	}


	// okay, i guess
	auto rr = this->right->codegen(cs, lt);
	auto rt = rr.value->getType();

	if(this->op != "=")
	{
		// ok it's a compound assignment
		// auto [ newl, newr ] = cs->autoCastValueTypes(lr, rr);
		auto nonass = getNonAssignOp(this->op);

		// some things -- if we're doing +=, and the types are supported, then just call the actual
		// append function, instead of doing the + first then assigning it.

		if(nonass == "+")
		{
			if(lt->isDynamicArrayType() && lt == rt)
			{
				// right then.
				if(lr.kind != CGResult::VK::LValue)
					error(this, "Cannot append to an r-value array");

				iceAssert(lr.pointer);
				auto appendf = cgn::glue::array::getAppendFunction(cs, lt->toDynamicArrayType());

				//? are there any ramifications for these actions for ref-counted things?
				auto res = cs->irb.Call(appendf, lr.value, rr.value);

				cs->irb.Store(res, lr.pointer);
				return CGResult(0);
			}
			else if(lt->isDynamicArrayType() && lt->getArrayElementType() == rt)
			{
				// right then.
				if(lr.kind != CGResult::VK::LValue)
					error(this, "Cannot append to an r-value array");

				iceAssert(lr.pointer);
				auto appendf = cgn::glue::array::getElementAppendFunction(cs, lt->toDynamicArrayType());

				//? are there any ramifications for these actions for ref-counted things?
				auto res = cs->irb.Call(appendf, lr.value, rr.value);

				cs->irb.Store(res, lr.pointer);
				return CGResult(0);
			}
		}


		// do the op first
		auto res = cs->performBinaryOperation(this->loc, { this->left->loc, lr }, { this->right->loc, rr }, nonass);

		// assign the res to the thing
		rr = res;
	}

	rr = cs->oneWayAutocast(rr, lt);

	if(rr.value == 0)
	{
		error(this, "Invalid assignment from value of type '%s' to expected type '%s'", rr.value->getType(), lt);
	}

	// ok then
	if(lt != rr.value->getType())
		error(this, "What? left = %s, right = %s", lt, rr.value->getType());

	iceAssert(lr.pointer);
	iceAssert(rr.value->getType() == lr.pointer->getType()->getPointerElementType());

	_handleRCAssign(cs, lt, rt, lr, rr);

	return CGResult(0);
}





CGResult sst::TupleAssignOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());

	auto tuple = this->right->codegen(cs).value;
	if(!tuple->getType()->isTupleType())
		error(this->right, "Expected tuple type in assignment to tuple on left-hand-side; found type '%s' instead", tuple->getType());

	auto tty = tuple->getType()->toTupleType();

	std::vector<CGResult> results;

	size_t idx = 0;
	for(auto v : this->lefts)
	{
		auto res = v->codegen(cs, tty->getElementN(idx));
		if(res.kind != CGResult::VK::LValue)
			error(v, "Cannot assign to non-lvalue expression in tuple assignment");

		if(!res.pointer)
			error(v, "didn't get pointer???");


		iceAssert(res.pointer);
		results.push_back(res);

		idx++;
	}

	for(size_t i = 0; i < idx; i++)
	{
		auto lr = results[i];
		auto val = cs->irb.ExtractValue(tuple, { i });

		auto rr = cs->oneWayAutocast(CGResult(val, 0), lr.value->getType());
		if(!rr.value || rr.value->getType() != lr.value->getType())
		{
			error(this->right, "Mismatched types in assignment to tuple element %d; assigning type '%s' to '%s'",
				val->getType(), lr.value->getType());
		}

		_handleRCAssign(cs, tty->getElementN(i), rr.value->getType(), lr, rr);
	}

	return CGResult(0);
}



































