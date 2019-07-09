// destructure.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"
#include "mpool.h"


CGResult sst::DecompDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	cs->generateDecompositionBindings(this->bindings, this->init->codegen(cs), true);

	return CGResult(0);
}




static void handleDefn(cgn::CodegenState* cs, sst::VarDefn* defn, CGResult res)
{
	// do a quick check for refcounting.
	//* note: due to the way vardefn codegen works, if we're assigning from an rvalue and the type is refcounted,
	//* we simply remove the rhs from the refcounting stack instead of changing refcounts around.
	//* so, since everything that we generate from destructuring is an rvalue, we always need to remove it.

	//* thus in order to remove it, we must first insert it.

	//* also, since the vardefn adds itself to the counting stack, when it dies we will get decremented.
	//* however, this cannot be allowed to happen, because we want a copy and not a move.
	if(fir::isRefCountedType(res->getType()))
	{
		cs->addRefCountedValue(res.value);
		cs->incrementRefCount(res.value);
	}

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

	if(!rt->isArrayType() && !rt->isDynamicArrayType() && !rt->isArraySliceType() && !rt->isStringType())
		error(bind.loc, "expected array type in destructuring declaration; found type '%s' instead", rt);

	if(rt->isStringType())
	{
		// do a bounds check.
		auto numbinds = fir::ConstantInt::getNative(bind.inner.size());
		{
			auto checkf = cgn::glue::string::getBoundsCheckFunction(cs, true);
			if(checkf)
			{
				auto strloc = fir::ConstantString::get(bind.loc.toString());
				cs->irb.Call(checkf, cs->irb.GetSAALength(rhs.value), numbinds, strloc);
			}
		}

		//* note: special-case this, because 1. we want to return chars
		auto strdat = cs->irb.PointerTypeCast(cs->irb.GetSAAData(rhs.value), fir::Type::getMutInt8Ptr());
		{
			size_t idx = 0;
			for(auto& b : bind.inner)
			{
				auto v = CGResult(cs->irb.ReadPtr(cs->irb.GetPointer(strdat, fir::ConstantInt::getNative(idx))));
				cs->generateDecompositionBindings(b, v, false);

				idx++;
			}
		}

		if(!bind.restName.empty())
		{
			if(bind.restRef)
			{
				// make a slice of char.
				auto remaining = cs->irb.Subtract(cs->irb.GetSAALength(rhs.value), numbinds);

				auto slice = cs->irb.CreateValue(fir::Type::getCharSlice(shouldSliceBeMutable));
				slice = cs->irb.SetArraySliceData(slice, cs->irb.GetPointer(strdat, numbinds));
				slice = cs->irb.SetArraySliceLength(slice, remaining);

				handleDefn(cs, bind.restDefn, CGResult(slice));
			}
			else
			{
				// make string.
				// auto remaining = cs->irb.Subtract(cs->irb.GetSAALength(rhs.value), numbinds);

				auto clonef = cgn::glue::string::getCloneFunction(cs);
				iceAssert(clonef);

				auto string = cs->irb.Call(clonef, rhs.value, numbinds);

				handleDefn(cs, bind.restDefn, CGResult(string));
			}
		}
	}
	else
	{
		auto array = rhs.value;
		fir::Value* arrlen = 0;

		auto numbinds = fir::ConstantInt::getNative(bind.inner.size());
		{
			if(rt->isArrayType())               arrlen = fir::ConstantInt::getNative(rt->toArrayType()->getArraySize());
			else if(rt->isArraySliceType())     arrlen = cs->irb.GetArraySliceLength(array);
			else if(rt->isDynamicArrayType())   arrlen = cs->irb.GetSAALength(array);
			else                                iceAssert(0);

			//* note: 'true' means we're performing a decomposition, so print a more appropriate error message on bounds failure.
			auto checkf = cgn::glue::array::getBoundsCheckFunction(cs, true);
			if(checkf)
			{
				auto strloc = fir::ConstantString::get(bind.loc.toString());
				cs->irb.Call(checkf, arrlen, numbinds, strloc);
			}
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
			else if(rt->isDynamicArrayType())   data = cs->irb.GetSAAData(array);
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
					// always return a dynamic array here.
					//* note: in order to make our lives somewhat easier, for fixed arrays, we create a fake slice pointing to its data, then we
					//* call clone on that instead.

					fir::Value* clonee = 0;
					if(rt->isArrayType())
					{
						clonee = cs->irb.CreateValue(fir::ArraySliceType::get(rt->getArrayElementType(), shouldSliceBeMutable));
						clonee = cs->irb.SetArraySliceData(clonee, data);
						clonee = cs->irb.SetArraySliceLength(clonee, fir::ConstantInt::getNative(rt->toArrayType()->getArraySize()));
					}
					else
					{
						clonee = array;
					}

					auto clonef = cgn::glue::array::getCloneFunction(cs, clonee->getType());
					iceAssert(clonef);

					auto ret = cs->irb.Call(clonef, clonee, numbinds);

					handleDefn(cs, bind.restDefn, CGResult(ret));
				}
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















