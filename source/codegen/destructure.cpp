// destructure.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"



CGResult sst::DecompDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	cs->generateDecompositionBindings(this->bindings, this->init->codegen(cs), this->immutable, true);

	return CGResult(0);
}




static void handleDefn(cgn::CodegenState* cs, sst::VarDefn* defn, CGResult res)
{
	// do a quick check for refcounting.
	//* note: due to the way vardefn codegen works, if we're assigning from an rvalue and the type is refcounted,
	//* we simply remove the rhs from the refcounting stack instead of changing refcounts around.
	//* so, since everything that we generate from destructuring is an rvalue, we always need to remove it.

	//* thus in order to remove it, we must first insert it.
	if(cs->isRefCountedType(res.value->getType()))
		cs->addRefCountedValue(res.value);

	if(defn)
	{
		auto v = new sst::RawValueExpr(defn->loc, res.value->getType());
		v->rawValue = res;

		defn->init = v;
		defn->codegen(cs);
	}
}

static void checkTuple(cgn::CodegenState* cs, const DecompMapping& bind, CGResult rhs, bool immut)
{
	iceAssert(!bind.array);

	auto rt = rhs.value->getType();
	iceAssert(rt->isTupleType());

	auto tty = rt->toTupleType();
	iceAssert(bind.inner.size() == tty->getElementCount());

	for(size_t i = 0; i < tty->getElementCount(); i++)
	{
		CGResult v;

		if(rhs.pointer)
		{
			auto gep = cs->irb.StructGEP(rhs.pointer, i);
			v = CGResult(cs->irb.Load(gep), gep);
		}
		else
		{
			v = CGResult(cs->irb.ExtractValue(rhs.value, { i }), 0);
		}

		cs->generateDecompositionBindings(bind.inner[i], v, immut, true);
	}
}

static void checkArray(cgn::CodegenState* cs, const DecompMapping& bind, CGResult rhs, bool immut)
{
	iceAssert(bind.array);

	auto rt = rhs.value->getType();

	if(!rt->isArrayType() && !rt->isDynamicArrayType() && !rt->isArraySliceType() && !rt->isStringType())
		error(bind.loc, "Expected array type in destructuring declaration; found type '%s' instead", rt);

	if(rt->isStringType())
	{
		// do a bounds check.
		auto numbinds = fir::ConstantInt::getInt64(bind.inner.size());
		{
			auto checkf = cgn::glue::string::getBoundsCheckFunction(cs);
			iceAssert(checkf);

			auto strloc = fir::ConstantString::get(bind.loc.toString());
			cs->irb.Call(checkf, rhs.value, numbinds, strloc);
		}

		//* note: special-case this, because 1. we want to return chars, but 2. strings are supposed to be immutable.
		auto strdat = cs->irb.PointerTypeCast(cs->irb.GetStringData(rhs.value), fir::CharType::get()->getPointerTo());
		{
			size_t idx = 0;
			for(auto& b : bind.inner)
			{
				auto v = CGResult(cs->irb.Load(cs->irb.PointerAdd(strdat, fir::ConstantInt::getInt64(idx))), 0);
				cs->generateDecompositionBindings(b, v, immut, false);

				idx++;
			}
		}

		if(!bind.restName.empty())
		{
			if(bind.restRef)
			{
				// make a slice of char.
				auto remaining = cs->irb.Subtract(cs->irb.GetStringLength(rhs.value), numbinds);

				auto slice = cs->irb.CreateValue(fir::ArraySliceType::get(fir::CharType::get()));
				slice = cs->irb.SetArraySliceData(slice, cs->irb.PointerAdd(strdat, numbinds));
				slice = cs->irb.SetArraySliceLength(slice, remaining);

				handleDefn(cs, bind.restDefn, CGResult(slice, 0));
			}
			else
			{
				// make string.
				// auto remaining = cs->irb.Subtract(cs->irb.GetStringLength(rhs.value), numbinds);

				auto clonef = cgn::glue::string::getCloneFunction(cs);
				iceAssert(clonef);

				auto string = cs->irb.Call(clonef, rhs.value, numbinds);

				handleDefn(cs, bind.restDefn, CGResult(string, 0));
			}
		}
	}
	else
	{
		auto array = rhs.value;
		fir::Value* arrlen = 0;

		auto numbinds = fir::ConstantInt::getInt64(bind.inner.size());
		{
			//* note: 'true' means we're performing a decomposition, so print a more appropriate error message on bounds failure.
			auto checkf = cgn::glue::array::getBoundsCheckFunction(cs, true);
			iceAssert(checkf);

			if(rt->isArrayType())               arrlen = fir::ConstantInt::getInt64(rt->toArrayType()->getArraySize());
			else if(rt->isArraySliceType())     arrlen = cs->irb.GetArraySliceLength(array);
			else if(rt->isDynamicArrayType())   arrlen = cs->irb.GetDynamicArrayLength(array);
			else                                iceAssert(0);

			auto strloc = fir::ConstantString::get(bind.loc.toString());
			cs->irb.Call(checkf, arrlen, numbinds, strloc);
		}

		if(!rhs.pointer && rt->isArrayType())
		{
			//* because of the way LLVM is designed, and hence by extension how we are designed,
			//* fixed-sized arrays are kinda dumb. If we don't have a pointer to the array (for whatever reason???),
			//* then we can't do a GEP access, and hence can't get a pointer to use for the 'rest' binding. So,
			//* we error on that case but allow binding the rest.

			//* theoretically if the compiler is well designed we should never hit this case, but who knows?

			size_t idx = 0;
			for(auto& b : bind.inner)
			{
				auto v = CGResult(cs->irb.ExtractValue(array, { idx }), 0);
				cs->generateDecompositionBindings(b, v, immut, false);

				idx++;
			}

			warn(bind.loc, "Destructure of array without pointer (shouldn't happen!)");
			if(!bind.restName.empty())
				error(bind.loc, "Could not get pointer to array (of type '%s') to create binding for '...'", rt);
		}
		else
		{
			fir::Value* data = 0;

			if(rt->isArrayType())               data = cs->irb.ConstGEP2(rhs.pointer, 0, 0);
			else if(rt->isArraySliceType())     data = cs->irb.GetArraySliceData(array);
			else if(rt->isDynamicArrayType())   data = cs->irb.GetDynamicArrayData(array);
			else                                iceAssert(0);


			size_t idx = 0;
			for(auto& b : bind.inner)
			{
				auto ptr = cs->irb.PointerAdd(data, fir::ConstantInt::getInt64(idx));

				auto v = CGResult(cs->irb.Load(ptr), ptr);
				cs->generateDecompositionBindings(b, v, immut, true);

				idx++;
			}

			if(!bind.restName.empty())
			{
				if(bind.restRef)
				{
					auto sty = fir::ArraySliceType::get(rt->getArrayElementType());

					auto remaining = cs->irb.Subtract(arrlen, numbinds);

					auto slice = cs->irb.CreateValue(sty);
					slice = cs->irb.SetArraySliceData(slice, cs->irb.PointerAdd(data, numbinds));
					slice = cs->irb.SetArraySliceLength(slice, remaining);

					handleDefn(cs, bind.restDefn, CGResult(slice, 0));
				}
				else
				{
					// always return a dynamic array here.
					//* note: in order to make our lives somewhat easier, for fixed arrays, we create a fake slice pointing to its data, then we
					//* call clone on that instead.

					fir::Value* clonee = 0;
					if(rt->isArrayType())
					{
						clonee = cs->irb.CreateValue(fir::ArraySliceType::get(rt->getArrayElementType()));
						clonee = cs->irb.SetArraySliceData(clonee, data);
						clonee = cs->irb.SetArraySliceLength(clonee, fir::ConstantInt::getInt64(rt->toArrayType()->getArraySize()));
					}
					else
					{
						clonee = array;
					}

					auto clonef = cgn::glue::array::getCloneFunction(cs, clonee->getType());
					iceAssert(clonef);

					auto ret = cs->irb.Call(clonef, clonee, numbinds);

					handleDefn(cs, bind.restDefn, CGResult(ret, 0));
				}
			}
		}
	}
}

void cgn::CodegenState::generateDecompositionBindings(const DecompMapping& bind, CGResult rhs, bool immut, bool allowref)
{
	auto rt = rhs.value->getType();

	if(!bind.name.empty())
	{
		if(bind.ref && !allowref)
			error(bind.loc, "Cannot bind to value of type '%s' by reference", rt);

		if(bind.ref)
		{
			rhs.value = rhs.pointer;
			rhs.pointer = 0;
		}

		handleDefn(this, bind.createdDefn, rhs);
	}
	else if(bind.array)
	{
		checkArray(this, bind, rhs, immut);
	}
	else
	{
		checkTuple(this, bind, rhs, immut);
	}
}















