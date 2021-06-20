// destructure.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"
#include "memorypool.h"


CGResult sst::DecompDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	cs->generateDecompositionBindings(this->bindings, this->init->codegen(cs), true);

	return CGResult(0);
}




static void handleDefn(cgn::CodegenState* cs, sst::VarDefn* defn, CGResult res)
{
	if(defn)
	{
		auto v = util::pool<sst::RawValueExpr>(defn->loc, res.value->getType());
		v->rawValue = res;

		defn->init = v;
		defn->codegen(cs);
	}
}

static void checkTuple(cgn::CodegenState* cs, const DecompMapping& bind, CGResult rhs)
{
	iceAssert(!bind.array);

	auto rt = rhs.value->getType();
	iceAssert(rt->isTupleType());

	auto tty = rt->toTupleType();
	iceAssert(bind.inner.size() == tty->getElementCount());

	for(size_t i = 0; i < tty->getElementCount(); i++)
	{
		CGResult v;

		if(rhs->islvalue())
		{
			auto gep = cs->irb.StructGEP(rhs.value, i);
			v = CGResult(gep);
		}
		else
		{
			v = CGResult(cs->irb.ExtractValue(rhs.value, { i }));
		}

		cs->generateDecompositionBindings(bind.inner[i], v, true);
	}
}

static void checkArray(cgn::CodegenState* cs, const DecompMapping& bind, CGResult rhs)
{
	iceAssert(bind.array);

	auto rt = rhs.value->getType();
	bool shouldSliceBeMutable = sst::getMutabilityOfSliceOfType(rt);

	if(!rt->isArrayType() && !rt->isArraySliceType())
		error(bind.loc, "expected array type in destructuring declaration; found type '%s' instead", rt);

	auto array = rhs.value;
	fir::Value* arrlen = 0;

	auto numbinds = fir::ConstantInt::getNative(bind.inner.size());
	{
		if(rt->isArrayType())               arrlen = fir::ConstantInt::getNative(rt->toArrayType()->getArraySize());
		else if(rt->isArraySliceType())     arrlen = cs->irb.GetArraySliceLength(array);
		else                                iceAssert(0);
	}

	// # if 0
	if(!rhs->islvalue() && rt->isArrayType())
	{
		//* because of the way LLVM is designed, and hence by extension how we are designed,
		//* fixed-sized arrays are kinda dumb. If we don't have a pointer to the array (for whatever reason???),
		//* then we can't do a GEP access, and hence can't get a pointer to use for the 'rest' binding. So,
		//* we error on that case but allow binding the rest.

		//* theoretically if the compiler is well designed we should never hit this case, but who knows?

		size_t idx = 0;
		for(auto& b : bind.inner)
		{
			auto v = CGResult(cs->irb.ExtractValue(array, { idx }));
			cs->generateDecompositionBindings(b, v, false);

			idx++;
		}

		warn(bind.loc, "destructure of array without pointer (shouldn't happen!)");
		if(!bind.restName.empty())
			error(bind.loc, "could not get pointer to array (of type '%s') to create binding for '...'", rt);
	}
	else
	// #endif

	{
		fir::Value* data = 0;

		if(rt->isArrayType())               data = cs->irb.ConstGEP2(rhs.value, 0, 0);
		else if(rt->isArraySliceType())     data = cs->irb.GetArraySliceData(array);
		else                                iceAssert(0);


		size_t idx = 0;
		for(auto& b : bind.inner)
		{
			auto ptr = cs->irb.GetPointer(data, fir::ConstantInt::getNative(idx));

			auto v = CGResult(cs->irb.Dereference(ptr));
			cs->generateDecompositionBindings(b, v, true);

			idx++;
		}

		if(!bind.restName.empty())
		{
			if(bind.restRef)
			{
				auto sty = fir::ArraySliceType::get(rt->getArrayElementType(), shouldSliceBeMutable);

				auto remaining = cs->irb.Subtract(arrlen, numbinds);

				auto slice = cs->irb.CreateValue(sty);
				slice = cs->irb.SetArraySliceData(slice, cs->irb.GetPointer(data, numbinds));
				slice = cs->irb.SetArraySliceLength(slice, remaining);

				handleDefn(cs, bind.restDefn, CGResult(slice));
			}
			else
			{
				error("gone");
			}
		}
	}
}

void cgn::CodegenState::generateDecompositionBindings(const DecompMapping& bind, CGResult rhs, bool allowref)
{
	auto rt = rhs.value->getType();

	if(!bind.name.empty())
	{
		if(bind.ref && !allowref)
			error(bind.loc, "cannot bind to value of type '%s' by reference", rt);

		if(bind.ref)
		{
			rhs.value = this->irb.AddressOf(rhs.value, false);
		}
		else
		{
			// rhs.value = this->irb.Dereference(rhs.value);
		}

		handleDefn(this, bind.createdDefn, rhs);
	}
	else if(bind.array)
	{
		checkArray(this, bind, rhs);
	}
	else
	{
		checkTuple(this, bind, rhs);
	}
}















