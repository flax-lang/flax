// arrays.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

#define BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME        "__array_boundscheck"
#define BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME "__array_boundscheckdecomp"

#define BUILTIN_ARRAY_CMP_FUNC_NAME                 "__array_compare"

#define BUILTIN_ARRAY_SET_ELEMENTS_DEFAULT_NAME     "__array_setelementsdefault"
#define BUILTIN_ARRAY_SET_ELEMENTS_NAME             "__array_setelements"
#define BUILTIN_ARRAY_CALL_CLASS_CONSTRUCTOR        "__array_callclassinit"

#define BUILTIN_DYNARRAY_CLONE_FUNC_NAME            "__darray_clone"
#define BUILTIN_DYNARRAY_APPEND_FUNC_NAME           "__darray_append"
#define BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME   "__darray_appendelement"
#define BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME         "__darray_popback"
#define BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME    "__darray_combinetwo"

#define BUILTIN_DYNARRAY_RESERVE_ENOUGH_NAME        "__darray_reservesufficient"
#define BUILTIN_DYNARRAY_RESERVE_EXTRA_NAME         "__darray_reserveextra"

#define BUITLIN_DYNARRAY_RECURSIVE_REFCOUNT_NAME    "__darray_recursiverefcount"

#define BUILTIN_SLICE_CLONE_FUNC_NAME               "__slice_clone"
#define BUILTIN_SLICE_APPEND_FUNC_NAME              "__slice_append"
#define BUILTIN_SLICE_APPEND_ELEMENT_FUNC_NAME      "__slice_appendelement"

#define BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME        "__loop_incr_refcount"
#define BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME        "__loop_decr_refcount"


namespace cgn {
namespace glue {
namespace array
{
	fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isPerformingDecomposition)
	{
		fir::Function* fn = cs->module->getFunction(Identifier(isPerformingDecomposition
			? BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME : BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(isPerformingDecomposition ? BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME : BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64(), fir::Type::getInt64(), fir::Type::getString() },
					fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* checkneg = cs->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			cs->irb.setCurrentBlock(entry);

			fir::Value* max = func->getArguments()[0];
			fir::Value* ind = func->getArguments()[1];

			fir::Value* res = 0;

			// if we're decomposing, it's length vs length, so compare strictly greater.
			if(isPerformingDecomposition)
				res = cs->irb.ICmpGT(ind, max);

			else
				res = cs->irb.ICmpGEQ(ind, max);

			iceAssert(res);

			cs->irb.CondBranch(res, failb, checkneg);
			cs->irb.setCurrentBlock(failb);
			{
				if(isPerformingDecomposition)
					printError(cs, func->getArguments()[2], "Tried to decompose array with only '%ld' elements into '%ld' bindings\n", { max, ind });

				else
					printError(cs, func->getArguments()[2], "Tried to index array at index '%ld', but length is only '%ld'\n", { ind, max });
			}

			cs->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res2 = cs->irb.ICmpLT(ind, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(res2, failb, merge);
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.ReturnVoid();
			}

			fn = func;

			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}





	static void _callCloneFunctionInLoop(CodegenState* cs, fir::Function* curfunc, fir::Function* fn,
		fir::Value* ptr, fir::Value* len, fir::Value* newptr, fir::Value* startIndex)
	{
		fir::IRBlock* loopcond = cs->irb.addNewBlockInFunction("loopcond", curfunc);
		fir::IRBlock* loopbody = cs->irb.addNewBlockInFunction("loopbody", curfunc);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", curfunc);

		fir::Value* counter = cs->irb.StackAlloc(fir::Type::getInt64());
		cs->irb.Store(startIndex, counter);

		cs->irb.UnCondBranch(loopcond);
		cs->irb.setCurrentBlock(loopcond);
		{
			fir::Value* res = cs->irb.ICmpEQ(cs->irb.Load(counter), len);
			cs->irb.CondBranch(res, merge, loopbody);
		}

		cs->irb.setCurrentBlock(loopbody);
		{
			// make clone
			fir::Value* origElm = cs->irb.PointerAdd(ptr, cs->irb.Load(counter));
			fir::Value* clone = 0;

			//* note: the '0' argument specifies the offset to clone from -- since want the whole thing, the offset is 0.
			clone = cs->irb.Call(fn, cs->irb.Load(origElm), fir::ConstantInt::getInt64(0));

			// store clone
			fir::Value* newElm = cs->irb.PointerAdd(newptr, cs->irb.Load(counter));
			cs->irb.Store(clone, newElm);

			// increment counter
			cs->irb.Store(cs->irb.Add(cs->irb.Load(counter), fir::ConstantInt::getInt64(1)), counter);
			cs->irb.UnCondBranch(loopcond);
		}

		cs->irb.setCurrentBlock(merge);
	}


	static void _handleCallingAppropriateCloneFunction(CodegenState* cs, fir::Function* func, fir::Type* elmType, fir::Value* origptr,
		fir::Value* newptr, fir::Value* origlen, fir::Value* actuallen, fir::Value* startIndex)
	{
		if(elmType->isPrimitiveType() || elmType->isCharType() || elmType->isEnumType())
		{
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			cs->irb.Call(memcpyf, { newptr, cs->irb.PointerTypeCast(cs->irb.PointerAdd(origptr,
				startIndex), fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });
		}
		else if(elmType->isDynamicArrayType())
		{
			// yo dawg i heard you like arrays...
			fir::Function* clonef = getCloneFunction(cs, elmType->toDynamicArrayType());
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.PointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, origptr, origlen, cloneptr, startIndex);
		}
		else if(elmType->isArraySliceType())
		{
			// yo dawg i heard you like arrays...
			fir::Function* clonef = getCloneFunction(cs, elmType->toArraySliceType());
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.PointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, origptr, origlen, cloneptr, startIndex);
		}
		else if(elmType->isStringType())
		{
			fir::Function* clonef = glue::string::getCloneFunction(cs);
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.PointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, origptr, origlen, cloneptr, startIndex);
		}
		else if(elmType->isStructType() || elmType->isClassType() || elmType->isTupleType() || elmType->isArrayType())
		{
			// todo: call copy constructors and stuff

			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			cs->irb.Call(memcpyf, { newptr, cs->irb.PointerTypeCast(cs->irb.PointerAdd(origptr,
				startIndex), fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });
		}
		else
		{
			error("unsupported element type '%s' for array clone", elmType);
		}
	}








	fir::Function* getCloneFunction(CodegenState* cs, fir::Type* arrtype)
	{
		if(arrtype->isDynamicArrayType())		return getCloneFunction(cs, arrtype->toDynamicArrayType());
		else if(arrtype->isArraySliceType())	return getCloneFunction(cs, arrtype->toArraySliceType());
		else									error("unsupported type '%s'", arrtype);
	}

	// takes ptr, start index
	fir::Function* getCloneFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		auto name = BUILTIN_DYNARRAY_CLONE_FUNC_NAME + std::string("_") + arrtype->encodedStr();

		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, fir::Type::getInt64() }, arrtype),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* insane = cs->irb.addNewBlockInFunction("insane", func);
			fir::IRBlock* merge1 = cs->irb.addNewBlockInFunction("merge1", func);

			cs->irb.setCurrentBlock(entry);

			fir::Value* orig = func->getArguments()[0];
			fir::Value* startIndex = func->getArguments()[1];

			iceAssert(orig);
			iceAssert(startIndex);



			fir::Value* origptr = cs->irb.GetDynamicArrayData(orig, "origptr");
			fir::Value* origlen = cs->irb.GetDynamicArrayLength(orig, "origlen");

			fir::Value* origcap = cs->irb.GetDynamicArrayCapacity(orig);
			// cs->irb.Store(origcap, cap);


			// note: sanity check that len <= cap
			fir::Value* sane = cs->irb.ICmpLEQ(origlen, origcap);
			cs->irb.CondBranch(sane, merge1, insane);


			cs->irb.setCurrentBlock(insane);
			{
				// sanity check failed
				// *but* since we're using dyn arrays as vararg arrays,
				// then we just treat capacity = length.

				cs->irb.UnCondBranch(merge1);
			}


			// ok, back to normal
			cs->irb.setCurrentBlock(merge1);
			auto cap = cs->irb.CreatePHINode(fir::Type::getInt64());

			cap->addIncoming(origcap, entry);
			cap->addIncoming(/*origlen*/ origcap, insane);


			// ok, alloc a buffer with the original capacity
			// get size in bytes, since cap is in elements
			fir::Value* actuallen = cs->irb.Multiply(cap, cs->irb.Sizeof(arrtype->getElementType()));

			// space for refcount
			actuallen = cs->irb.Add(actuallen, fir::ConstantInt::getInt64(REFCOUNT_SIZE));


			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* newptr = cs->irb.Call(mallocf, actuallen);
			newptr = cs->irb.PointerAdd(newptr, fir::ConstantInt::getInt64(REFCOUNT_SIZE));


			fir::Type* elmType = arrtype->getElementType();
			_handleCallingAppropriateCloneFunction(cs, func, elmType, origptr, newptr, origlen, actuallen, startIndex);

			fir::Value* newarr = cs->irb.CreateValue(arrtype);
			newarr = cs->irb.SetDynamicArrayData(newarr, cs->irb.PointerTypeCast(newptr, arrtype->getElementType()->getPointerTo()));
			newarr = cs->irb.SetDynamicArrayLength(newarr, cs->irb.Subtract(origlen, startIndex));
			newarr = cs->irb.SetDynamicArrayCapacity(newarr, cap);
			cs->irb.SetDynamicArrayRefCount(newarr, fir::ConstantInt::getInt64(1));


			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("clone array: OLD :: (ptr: %p, len: %ld, cap: %ld) | NEW :: (ptr: %p, len: %ld, cap: %ld)\n");
				cs->irb.Call(printfn, { tmpstr, origptr, origlen, origcap, newptr, cs->irb.Sub(origlen, startIndex), cap });
			}
			#endif


			cs->irb.Return(newarr);

			fn = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}









	// takes a slice, but returns a dynamic array
	fir::Function* getCloneFunction(CodegenState* cs, fir::ArraySliceType* arrtype)
	{
		auto name = BUILTIN_SLICE_CLONE_FUNC_NAME + std::string("_") + arrtype->encodedStr();

		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, fir::Type::getInt64() },
					fir::DynamicArrayType::get(arrtype->getElementType())), fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* merge1 = cs->irb.addNewBlockInFunction("merge1", func);

			cs->irb.setCurrentBlock(entry);

			fir::Value* orig = func->getArguments()[0];
			fir::Value* startIndex = func->getArguments()[1];

			iceAssert(orig);
			iceAssert(startIndex);

			fir::Value* origptr = cs->irb.GetArraySliceData(orig);
			fir::Value* origlen = cs->irb.GetArraySliceLength(orig);

			cs->irb.UnCondBranch(merge1);

			// ok, back to normal
			cs->irb.setCurrentBlock(merge1);

			// ok, alloc a buffer with the original capacity
			// get size in bytes, since cap is in elements
			fir::Value* actuallen = cs->irb.Multiply(origlen, cs->irb.Sizeof(arrtype->getElementType()));

			// refcount space
			actuallen = cs->irb.Add(actuallen, fir::ConstantInt::getInt64(REFCOUNT_SIZE));


			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* newptr = cs->irb.Call(mallocf, actuallen);
			newptr = cs->irb.PointerAdd(newptr, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			fir::Type* elmType = arrtype->getElementType();
			_handleCallingAppropriateCloneFunction(cs, func, elmType, origptr, newptr, origlen, actuallen, startIndex);


			fir::Value* newlen = cs->irb.Subtract(origlen, startIndex);

			fir::Value* newarr = cs->irb.CreateValue(fir::DynamicArrayType::get(arrtype->getElementType()));
			newarr = cs->irb.SetDynamicArrayData(newarr, cs->irb.PointerTypeCast(newptr, arrtype->getElementType()->getPointerTo()));
			newarr = cs->irb.SetDynamicArrayLength(newarr, newlen);
			newarr = cs->irb.SetDynamicArrayCapacity(newarr, newlen);
			cs->irb.SetDynamicArrayRefCount(newarr, fir::ConstantInt::getInt64(1));


			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("clone slice: OLD :: (ptr: %p, len: %ld) | NEW :: (ptr: %p, len: %ld, cap: %ld)\n");
				cs->irb.Call(printfn, { tmpstr, origptr, origlen, newptr, newlen, newlen });
			}
			#endif


			cs->irb.Return(newarr);

			fn = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






	// static void _growCapacityBy(CodegenState* cs, fir::Function* func, fir::Value* arr, fir::Value* amt)
	// {
	// 	iceAssert(arr->getType()->isPointerType());
	// 	iceAssert(arr->getType()->getPointerElementType()->isDynamicArrayType());
	// 	iceAssert(amt->getType() == fir::Type::getInt64());
	// }


	//* required is how much *EXTRA* space we need.
	static fir::Value* _checkCapacityAndGrowIfNeeded(CodegenState* cs, fir::Function* func, fir::Value* arr, fir::Value* required)
	{
		iceAssert(arr->getType()->isDynamicArrayType());
		iceAssert(required->getType() == fir::Type::getInt64());

		auto elmtype = arr->getType()->getArrayElementType();

		fir::Value* ptr = cs->irb.GetDynamicArrayData(arr, "ptr");
		fir::Value* len = cs->irb.GetDynamicArrayLength(arr, "len");
		fir::Value* cap = cs->irb.GetDynamicArrayCapacity(arr, "cap");
		fir::Value* refCountSize = fir::ConstantInt::getInt64(REFCOUNT_SIZE);
		fir::Value* refcnt = 0;

		auto curblk = cs->irb.getCurrentBlock();
		fir::IRBlock* isnull = cs->irb.addNewBlockInFunction("isnull", func);
		fir::IRBlock* notnull = cs->irb.addNewBlockInFunction("notnull", func);
		// fir::IRBlock* trampoline = cs->irb.addNewBlockInFunction("trampoline", func);
		fir::IRBlock* newblk = cs->irb.addNewBlockInFunction("newblock", func);
		fir::IRBlock* freeold = cs->irb.addNewBlockInFunction("freeold", func);
		fir::IRBlock* mergeblk = cs->irb.addNewBlockInFunction("merge", func);

		fir::Value* needed = cs->irb.Add(len, required, "needed");
		fir::Value* elmsize = cs->irb.Sizeof(elmtype, "elmsize");

		fir::Value* nullPhi = 0;
		{
			auto cond = cs->irb.ICmpEQ(cs->irb.PointerToIntCast(ptr, fir::Type::getInt64()), fir::ConstantInt::getInt64(0));
			cs->irb.CondBranch(cond, isnull, notnull);

			cs->irb.setCurrentBlock(isnull);
			{
				auto mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
				iceAssert(mallocf);

				auto allocsz = cs->irb.Add(cs->irb.Multiply(needed, elmsize), refCountSize);
				auto rawdata = cs->irb.Call(mallocf, allocsz);

				auto dataptr = cs->irb.PointerAdd(cs->irb.PointerTypeCast(rawdata, fir::Type::getInt64Ptr()), fir::ConstantInt::getInt64(1));
				dataptr = cs->irb.PointerTypeCast(dataptr, elmtype->getPointerTo());

				auto setfn = cgn::glue::array::getSetElementsToDefaultValueFunction(cs, elmtype);
				cs->irb.Call(setfn, dataptr, needed);

				auto ret = cs->irb.SetDynamicArrayData(arr, dataptr);
				ret = cs->irb.SetDynamicArrayLength(ret, needed);
				ret = cs->irb.SetDynamicArrayCapacity(ret, needed);

				#if DEBUG_ARRAY_ALLOCATION
				{
					fir::Value* tmpstr = cs->module->createGlobalString("grow arr (from null): OLD :: (ptr: %p, len: %ld, cap: %ld) | NEW :: (ptr: %p, len: %ld, cap: %ld)\n");

					cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, ptr, len, cap, dataptr, needed, needed });
				}
				#endif


				cs->irb.SetDynamicArrayRefCount(ret, fir::ConstantInt::getInt64(1));

				nullPhi = ret;
			}



			cs->irb.UnCondBranch(mergeblk);

			cs->irb.setCurrentBlock(notnull);
			refcnt = cs->irb.GetDynamicArrayRefCount(arr);
		}


		// check if len + required > cap
		fir::IRBlock* cb = cs->irb.getCurrentBlock();
		fir::Value* cond = cs->irb.ICmpGT(needed, cap);


		// for when the 'dynamic' array came from a literal. same as the usual stuff
		// capacity will be -1, in this case.

		cs->irb.CondBranch(cond, newblk, mergeblk);

		// return a phi node.
		fir::Value* growPhi = 0;


		// grows to the nearest power of two from (len + required)
		cs->irb.setCurrentBlock(newblk);
		{
			fir::Function* p2func = cs->module->getIntrinsicFunction("roundup_pow2");
			iceAssert(p2func);

			fir::Value* nextpow2 = cs->irb.Call(p2func, needed, "nextpow2");

			fir::Value* newptr = 0;
			#if 0
			{
				fir::Function* refunc = cs->getOrDeclareLibCFunction(REALLOCATE_MEMORY_FUNC);
				iceAssert(refunc);

				auto dataptr = cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr());
				dataptr = cs->irb.PointerSub(dataptr, refCountSize);

				fir::Value* actuallen = cs->irb.Mul(nextpow2, elmsize);
				newptr = cs->irb.Call(refunc, dataptr, actuallen);
				newptr = cs->irb.PointerAdd(newptr, refCountSize);
				newptr = cs->irb.PointerTypeCast(newptr, ptr->getType());
			}
			#else
			{
				/*
					* Because this shit has caused me a lot of trouble, here's some documentation on the semantics of this allocation
					* job:

					1. 'ptr' points to the start of the array, which is 8 bytes ahead of the actual allocated address.
					2. We allocate nextpow2 + 8 bytes of memory
					3. We set the returned pointer (mallocptr) to be 8 bytes ahead.
					4. Memcpy only sees numElements * sizeof(element)
				*/


				fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
				iceAssert(mallocf);

				auto malloclen = cs->irb.Multiply(nextpow2, elmsize);
				auto mallocptr = cs->irb.Call(mallocf, cs->irb.Add(malloclen, refCountSize));
				mallocptr = cs->irb.PointerAdd(mallocptr, refCountSize);


				fir::Value* oldptr = cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr());

				// do a memcopy
				fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
				auto copylen = cs->irb.Multiply(len, elmsize);

				cs->irb.Call(memcpyf, { mallocptr, oldptr, copylen,
					fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

				iceAssert(mallocptr->getType() == fir::Type::getInt8Ptr());
				newptr = cs->irb.PointerTypeCast(mallocptr, ptr->getType());
			}
			#endif

			iceAssert(newptr);


			fir::Value* ret = cs->irb.SetDynamicArrayData(arr, newptr);
			ret = cs->irb.SetDynamicArrayCapacity(ret, nextpow2);

			iceAssert(refcnt);
			cs->irb.SetDynamicArrayRefCount(ret, refcnt);

			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Value* tmpstr = cs->module->createGlobalString("grow arr: OLD :: (ptr: %p, len: %ld, cap: %ld) | NEW :: (ptr: %p, len: %ld, cap: %ld)\n");
				cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, ptr, len, cap, newptr, len, nextpow2 });
			}
			#endif


			growPhi = ret;

			// set the new space to valid things
			if((false))
			{
				auto setfn = getSetElementsToDefaultValueFunction(cs, elmtype);
				auto ofsptr = cs->irb.PointerAdd(cs->irb.GetDynamicArrayData(ret), len);
				cs->irb.Call(setfn, ofsptr, cs->irb.Subtract(nextpow2, len));
			}


			{
				auto cond = cs->irb.ICmpEQ(cap, fir::ConstantInt::getInt64(-1));
				cs->irb.CondBranch(cond, mergeblk, freeold);
			}
		}


		// makes a new memory piece, to the nearest power of two from (len + required)
		cs->irb.setCurrentBlock(freeold);
		{
			// free the old memory
			fir::Function* freef = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
			iceAssert(freef);

			cs->irb.Call(freef, cs->irb.PointerSub(cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr()), refCountSize));

			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Value* tmpstr = cs->module->createGlobalString("free arr: (ptr: %p)\n");
				cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, ptr });
			}
			#endif


			cs->irb.UnCondBranch(mergeblk);
		}



		cs->irb.setCurrentBlock(mergeblk);
		{
			auto phi = cs->irb.CreatePHINode(arr->getType());
			phi->addIncoming(arr, cb);
			phi->addIncoming(nullPhi, isnull);
			phi->addIncoming(growPhi, newblk);
			phi->addIncoming(growPhi, freeold);

			return phi;
		}
	}



	fir::Function* getAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_APPEND_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* appendf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!appendf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, arrtype }, arrtype), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			auto elmType = arrtype->getElementType();



			// get the second one
			{
				fir::Value* origlen = cs->irb.GetDynamicArrayLength(s1);
				fir::Value* applen = cs->irb.GetDynamicArrayLength(s2);

				// grow if needed
				s1 = _checkCapacityAndGrowIfNeeded(cs, func, s1, applen);

				// we should be ok, now copy.
				fir::Value* ptr = cs->irb.GetDynamicArrayData(s1);
				ptr = cs->irb.PointerAdd(ptr, origlen);

				fir::Value* s2ptr = cs->irb.GetDynamicArrayData(s2);

				fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

				fir::Value* actuallen = cs->irb.Multiply(applen, cs->irb.Sizeof(arrtype->getElementType()));

				cs->irb.Call(memcpyf, { cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr()),
					cs->irb.PointerTypeCast(s2ptr, fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0),
					fir::ConstantBool::get(false) });

				// increase the length
				s1 = cs->irb.SetDynamicArrayLength(s1, cs->irb.Add(origlen, applen));


				if(cs->isRefCountedType(elmType))
				{
					// loop through the source array (Y in X + Y)

					fir::IRBlock* cond = cs->irb.addNewBlockInFunction("loopCond", func);
					fir::IRBlock* body = cs->irb.addNewBlockInFunction("loopBody", func);
					fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

					fir::Value* ctrPtr = cs->irb.StackAlloc(fir::Type::getInt64());

					// already set to 0 internally
					// cs->irb.Store(fir::ConstantInt::getInt64(0), ctrPtr);

					fir::Value* s2len = cs->irb.GetDynamicArrayLength(s2);
					cs->irb.UnCondBranch(cond);

					cs->irb.setCurrentBlock(cond);
					{
						// check the condition
						fir::Value* ctr = cs->irb.Load(ctrPtr);
						fir::Value* res = cs->irb.ICmpLT(ctr, s2len);

						cs->irb.CondBranch(res, body, merge);
					}

					cs->irb.setCurrentBlock(body);
					{
						// increment refcount
						fir::Value* val = cs->irb.Load(cs->irb.PointerAdd(s2ptr, cs->irb.Load(ctrPtr)));

						cs->incrementRefCount(val);

						// increment counter
						cs->irb.Store(cs->irb.Add(fir::ConstantInt::getInt64(1), cs->irb.Load(ctrPtr)), ctrPtr);
						cs->irb.UnCondBranch(cond);
					}

					cs->irb.setCurrentBlock(merge);
				}


				// ok done.
				cs->irb.Return(s1);
			}


			cs->irb.setCurrentBlock(restore);
			appendf = func;
		}

		iceAssert(appendf);
		return appendf;
	}






	fir::Function* getElementAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* appendf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!appendf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, arrtype->getElementType() }, arrtype), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			auto elmType = arrtype->getElementType();

			// get the second one
			{
				fir::Value* origlen = cs->irb.GetDynamicArrayLength(s1);
				fir::Value* applen = fir::ConstantInt::getInt64(1);

				// grow if needed
				s1 = _checkCapacityAndGrowIfNeeded(cs, func, s1, applen);


				// we should be ok, now copy.
				fir::Value* ptr = cs->irb.GetDynamicArrayData(s1);
				auto origptr = ptr;
				ptr = cs->irb.PointerAdd(ptr, origlen);

				cs->irb.Store(s2, ptr);
				if(cs->isRefCountedType(elmType))
					cs->incrementRefCount(s2);

				// increase the length
				s1 = cs->irb.SetDynamicArrayLength(s1, cs->irb.Add(origlen, applen));

				// ok done.
				cs->irb.Return(s1);
			}


			cs->irb.setCurrentBlock(restore);
			appendf = func;
		}

		iceAssert(appendf);
		return appendf;
	}


	fir::Function* getCallClassConstructorOnElementsFunction(CodegenState* cs, fir::ClassType* cls, sst::FunctionDefn* constr,
		const std::vector<FnCallArgument>& args)
	{
		iceAssert(cls);

		auto name = BUILTIN_ARRAY_CALL_CLASS_CONSTRUCTOR + std::string("_") + cls->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ cls->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			// ok: the real difference with the one below is that we need to call the constructor function on every element.

			fir::Value* arrdata = func->getArguments()[0];
			fir::Value* len = func->getArguments()[1];


			fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto ctrptr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), ctrptr);

			cs->irb.UnCondBranch(check);
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLT(cs->irb.Load(ctrptr), len);
				cs->irb.CondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				auto ctr = cs->irb.Load(ctrptr);
				auto ptr = cs->irb.PointerAdd(arrdata, ctr);

				cs->constructClassWithArguments(cls, constr, ptr, args);

				cs->irb.Store(cs->irb.Add(ctr, fir::ConstantInt::getInt64(1)), ctrptr);

				cs->irb.UnCondBranch(check);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();




			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}


	fir::Function* getSetElementsToValueFunction(CodegenState* cs, fir::Type* elmType)
	{
		iceAssert(elmType);

		auto name = BUILTIN_ARRAY_SET_ELEMENTS_NAME + std::string("_") + elmType->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ elmType->getPointerTo(), fir::Type::getInt64(), elmType }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arrdata = func->getArguments()[0];
			fir::Value* len = func->getArguments()[1];
			fir::Value* value = func->getArguments()[2];

			iceAssert(value);
			fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto ctrptr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), ctrptr);

			cs->irb.UnCondBranch(check);
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLT(cs->irb.Load(ctrptr), len);
				cs->irb.CondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				auto ctr = cs->irb.Load(ctrptr);
				auto ptr = cs->irb.PointerAdd(arrdata, ctr);

				cs->autoAssignRefCountedValue(CGResult(0, ptr), CGResult(value, 0, CGResult::VK::LitRValue), true, true);

				cs->irb.Store(cs->irb.Add(ctr, fir::ConstantInt::getInt64(1)), ctrptr);

				cs->irb.UnCondBranch(check);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}


	fir::Function* getSetElementsToDefaultValueFunction(CodegenState* cs, fir::Type* elmType)
	{
		iceAssert(elmType);

		auto name = BUILTIN_ARRAY_SET_ELEMENTS_DEFAULT_NAME + std::string("_") + elmType->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ elmType->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* value = 0;

			if(elmType->isClassType())
				value = cs->irb.CreateValue(elmType);

			else
				value = cs->getDefaultValue(elmType);

			iceAssert(value);

			auto setfn = getSetElementsToValueFunction(cs, elmType);
			iceAssert(setfn);

			cs->irb.Call(setfn, func->getArguments()[0], func->getArguments()[1], value);

			cs->irb.ReturnVoid();


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}


	//* On the other hand, this *always* reserves an *extra* N elements worth of space
	//* in the array.
	//! broken
	fir::Function* getReserveExtraSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_RESERVE_EXTRA_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* reservef = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!reservef)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, fir::Type::getInt64() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arr = func->getArguments()[0];
			fir::Value* cnt = func->getArguments()[1];

			auto elmType = arrtype->getElementType();

			fir::Value* origlen = cs->irb.GetDynamicArrayLength(arr);

			arr = _checkCapacityAndGrowIfNeeded(cs, func, arr, cnt);

			auto setfn = getSetElementsToDefaultValueFunction(cs, elmType);
			auto ofsptr = cs->irb.PointerAdd(cs->irb.GetDynamicArrayData(arr), origlen);
			cs->irb.Call(setfn, ofsptr, cnt);

			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);
			reservef = func;
		}

		return reservef;
	}






	static void _compareFunctionUsingBuiltinCompare(CodegenState* cs, fir::Type* arrtype, fir::Function* func,
		fir::Value* arg1, fir::Value* arg2)
	{
		// ok, ez.
		fir::Value* zeroval = fir::ConstantInt::getInt64(0);
		fir::Value* oneval = fir::ConstantInt::getInt64(1);

		fir::IRBlock* cond = cs->irb.addNewBlockInFunction("cond", func);
		fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
		fir::IRBlock* incr = cs->irb.addNewBlockInFunction("incr", func);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

		fir::Value* ptr1 = 0; fir::Value* ptr2 = 0;

		if(arrtype->isDynamicArrayType())
		{
			ptr1 = cs->irb.GetDynamicArrayData(arg1);
			ptr2 = cs->irb.GetDynamicArrayData(arg2);
		}
		else if(arrtype->isArraySliceType())
		{
			ptr1 = cs->irb.GetArraySliceData(arg1);
			ptr2 = cs->irb.GetArraySliceData(arg2);
		}
		else if(arrtype->isArrayType())
		{
			ptr1 = cs->irb.ConstGEP2(arg1, 0, 0);
			ptr2 = cs->irb.ConstGEP2(arg2, 0, 0);
		}
		else
		{
			error("invalid type '%s'", arrtype);
		}

		fir::Value* len1 = 0; fir::Value* len2 = 0;

		if(arrtype->isDynamicArrayType())
		{
			len1 = cs->irb.GetDynamicArrayLength(arg1);
			len2 = cs->irb.GetDynamicArrayLength(arg2);
		}
		else if(arrtype->isArraySliceType())
		{
			len1 = cs->irb.GetArraySliceLength(arg1);
			len2 = cs->irb.GetArraySliceLength(arg2);
		}
		else if(arrtype->isArrayType())
		{
			len1 = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());
			len2 = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());
		}
		else
		{
			error("invalid type '%s'", arrtype);
		}

		// we compare to this to break
		fir::Value* counter = cs->irb.StackAlloc(fir::Type::getInt64());
		cs->irb.Store(zeroval, counter);

		fir::Value* res = cs->irb.StackAlloc(fir::Type::getInt64());
		cs->irb.Store(zeroval, res);


		cs->irb.UnCondBranch(cond);
		cs->irb.setCurrentBlock(cond);
		{
			fir::IRBlock* retlt = cs->irb.addNewBlockInFunction("retlt", func);
			fir::IRBlock* reteq = cs->irb.addNewBlockInFunction("reteq", func);
			fir::IRBlock* retgt = cs->irb.addNewBlockInFunction("retgt", func);

			fir::IRBlock* tmp1 = cs->irb.addNewBlockInFunction("tmp1", func);
			fir::IRBlock* tmp2 = cs->irb.addNewBlockInFunction("tmp2", func);

			// if we got here, the arrays were equal *up to this point*
			// if ptr1 exceeds or ptr2 exceeds, return len1 - len2

			fir::Value* t1 = cs->irb.ICmpEQ(cs->irb.Load(counter), len1);
			fir::Value* t2 = cs->irb.ICmpEQ(cs->irb.Load(counter), len2);

			// if t1 is over, goto tmp1, if not goto t2
			cs->irb.CondBranch(t1, tmp1, tmp2);
			cs->irb.setCurrentBlock(tmp1);
			{
				// t1 is over
				// check if t2 is over
				// if so, return 0 (b == a)
				// if not, return -1 (b > a)

				cs->irb.CondBranch(t2, reteq, retlt);
			}

			cs->irb.setCurrentBlock(tmp2);
			{
				// t1 is not over
				// check if t2 is over
				// if so, return 1 (a > b)
				// if not, goto body

				cs->irb.CondBranch(t2, retgt, body);
			}


			cs->irb.setCurrentBlock(retlt);
			cs->irb.Return(fir::ConstantInt::getInt64(-1));

			cs->irb.setCurrentBlock(reteq);
			cs->irb.Return(fir::ConstantInt::getInt64(0));

			cs->irb.setCurrentBlock(retgt);
			cs->irb.Return(fir::ConstantInt::getInt64(+1));
		}


		cs->irb.setCurrentBlock(body);
		{
			fir::Value* v1 = cs->irb.Load(cs->irb.PointerAdd(ptr1, cs->irb.Load(counter)));
			fir::Value* v2 = cs->irb.Load(cs->irb.PointerAdd(ptr2, cs->irb.Load(counter)));

			fir::Value* c = cs->performBinaryOperation(cs->loc(), { cs->loc(), CGResult(v1) }, { cs->loc(), CGResult(v2) }, "==").value;

			// c is a bool, because it's very generic in nature
			// so we just take !c and convert to i64 to get our result.
			// if c == true, then lhs == rhs, and so we should have 0.

			c = cs->irb.LogicalNot(c);
			c = cs->irb.IntSizeCast(c, fir::Type::getInt64());

			cs->irb.Store(c, res);

			// compare to 0.
			fir::Value* cmpres = cs->irb.ICmpEQ(cs->irb.Load(res), zeroval);

			// if equal, go to incr, if not return directly
			cs->irb.CondBranch(cmpres, incr, merge);
		}


		cs->irb.setCurrentBlock(incr);
		{
			cs->irb.Store(cs->irb.Add(cs->irb.Load(counter), oneval), counter);
			cs->irb.UnCondBranch(cond);
		}



		cs->irb.setCurrentBlock(merge);
		{
			// load and return
			cs->irb.Return(cs->irb.Load(res));
		}
	}


	static void _compareFunctionUsingOperatorFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* curfunc,
		fir::Value* arg1, fir::Value* arg2, fir::Function* opf)
	{
		// fir::Value* zeroval = fir::ConstantInt::getInt64(0);
		error("notsup");
	}



	fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_ARRAY_CMP_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, arrtype }, fir::Type::getInt64()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			{
				// check our situation.
				if(opf == 0)
				{
					_compareFunctionUsingBuiltinCompare(cs, arrtype, func, s1, s2);
				}
				else
				{
					_compareFunctionUsingOperatorFunction(cs, arrtype, func, s1, s2, opf);
				}

				// functions above do their own return
			}


			cs->irb.setCurrentBlock(restore);
			cmpf = func;
		}

		iceAssert(cmpf);
		return cmpf;
	}








	/*
		How this works is simple, but at the same time not really. 'make-recursive-dealloc' will return a function that simply frees the memory
		pointed to by the first argument.

		Additionally, if the 'nest' parameter is greater than 0, it will call itself with nest - 1, to create a function that frees each *element*
		of the array (ptr, len) using the same, recursive method.

		After calling itself if necessary, it then simply does free() on the memory pointer.

		It's quite clever, I know.

		One thing to note is that, because this deals exclusively with dynamic arrays, the 'ptr' passed to it is not the true memory pointer,
		but rather the pointer to the first element -- so it must pass (ptr - 8) to free().

		Oops, a last-minute addition and i'm too lazy to rewrite the above;
		We now handle both incrementing and decrementing, to greatly simplify all the code. Incrementing simply ignores the part where we free things.
	*/
	static fir::Function* makeRecursiveRefCountingFunction(CodegenState* cs, fir::DynamicArrayType* arrtype, int nest, bool incr)
	{
		auto name = BUITLIN_DYNARRAY_RECURSIVE_REFCOUNT_NAME + ("_" + std::to_string(nest) + (incr ? "_incr_" : "_decr_")) + arrtype->encodedStr();
		fir::Function* retf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!retf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arr = func->getArguments()[0];

			auto ptr = cs->irb.GetDynamicArrayData(arr);
			auto len = cs->irb.GetDynamicArrayLength(arr);
			auto cap = cs->irb.GetDynamicArrayCapacity(arr);
			auto refc = cs->irb.GetDynamicArrayRefCount(arr);


			// we combine these a little bit more than maybe you're expecting, but that's because fundamentally we're really
			// doing the same thing, whether nest is > 1 or whether we're refcounting -- still a loop through the elements,
			// the only thing that changes is what we do with each element.
			{
				auto elmtype = arrtype->getElementType();

				if(nest > 1)	iceAssert(elmtype->isDynamicArrayType());
				else			iceAssert(!elmtype->isDynamicArrayType());

				// handle the refcount if we're nested, or if we're the final layer but our element type is refcounted (eg. strings)
				if(nest > 1 || cs->isRefCountedType(elmtype))
				{
					fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
					fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
					fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

					auto idxptr = cs->irb.StackAlloc(fir::Type::getInt64());

					// already set to 0 internally
					// cs->irb.Store(fir::ConstantInt::getInt64(0), idxptr);

					cs->irb.UnCondBranch(check);
					cs->irb.setCurrentBlock(check);
					{
						auto cond = cs->irb.ICmpLT(cs->irb.Load(idxptr), len);
						cs->irb.CondBranch(cond, body, merge);
					}

					cs->irb.setCurrentBlock(body);
					{
						auto val = cs->irb.Load(cs->irb.PointerAdd(ptr, cs->irb.Load(idxptr)));

						// this is where things differ a bit -- for nest > 1, it's a dynamic array.
						// note that cs->(incr|decr)ementRefCount() eventually calls back to us, leading
						// to a recursion fail, so we must do it ourselves, basically.

						if(nest > 1)
						{
							auto fn = makeRecursiveRefCountingFunction(cs, elmtype->toDynamicArrayType(), nest - 1, incr);
							iceAssert(fn);

							cs->irb.Call(fn, val);
						}
						else
						{
							if(incr)	cs->incrementRefCount(val);
							else		cs->decrementRefCount(val);
						}

						cs->irb.Store(cs->irb.Add(cs->irb.Load(idxptr), fir::ConstantInt::getInt64(1)), idxptr);

						cs->irb.UnCondBranch(check);
					}

					cs->irb.setCurrentBlock(merge);
				}

				// here it's the same thing regardless of nest.
				if(incr)	cs->irb.SetDynamicArrayRefCount(arr, cs->irb.Add(refc, fir::ConstantInt::getInt64(1)));
				else		cs->irb.SetDynamicArrayRefCount(arr, cs->irb.Subtract(refc, fir::ConstantInt::getInt64(1)));

				#if DEBUG_ARRAY_REFCOUNTING
				{
					std::string x = increment ? "(incr)" : "(decr)";
					fir::Value* tmpstr = cs->module->createGlobalString(x + " new rc of arr: (ptr: %p, len: %ld) = %d\n");

					cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, cs->irb.GetDynamicArrayData(arr), cs->irb.GetDynamicArrayLength(arr), refc });
				}
				#endif

				// ok. if we're incrementing, then we're done -- but if we're decrementing, we may need to free the memory.
				if(!incr)
				{
					auto mem = cs->irb.PointerAdd(cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr()),
						fir::ConstantInt::getInt64(REFCOUNT_SIZE));

					fir::IRBlock* dealloc = cs->irb.addNewBlockInFunction("dealloc", func);
					fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

					//! NOTE: what we want to happen here is for us to free the memory, but only if refcnt == 0 && capacity >= 0
					//* so our condition is (REFCOUNT == 0) & (CAP >= 0)

					auto zv = fir::ConstantInt::getInt64(0);
					auto dofree = cs->irb.BitwiseAND(cs->irb.ICmpEQ(refc, zv), cs->irb.ICmpGEQ(cap, zv));
					cs->irb.CondBranch(dofree, dealloc, merge);

					cs->irb.setCurrentBlock(dealloc);
					{
						ptr = cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr());
						ptr = cs->irb.PointerSub(ptr, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

						auto freefn = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
						iceAssert(freefn);

						cs->irb.Call(freefn, ptr);

						#if DEBUG_ARRAY_ALLOCATION
						{
							fir::Value* tmpstr = cs->module->createGlobalString("freed arr: (ptr: %p)\n");
							cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr,
								cs->irb.PointerAdd(ptr, fir::ConstantInt::getInt64(REFCOUNT_SIZE)) });
						}
						#endif


						cs->irb.UnCondBranch(merge);
					}

					cs->irb.setCurrentBlock(merge);
				}
			}



			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);
			retf = func;
		}

		iceAssert(retf);
		return retf;
	}






	static fir::Function* _getDoRefCountFunctionForDynamicArray(CodegenState* cs, fir::DynamicArrayType* arrtype, bool increment)
	{
		auto name = (increment ? BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME : BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME)
			+ std::string("_") + arrtype->encodedStr();

		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype }, arrtype), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);


			fir::Value* arr = func->getArguments()[0];

			int nest = 0;
			auto ty = arr->getType();
			while(ty->isDynamicArrayType())
				ty = ty->getArrayElementType(), nest++;

			auto fn = makeRecursiveRefCountingFunction(cs, arr->getType()->toDynamicArrayType(), nest, increment);
			iceAssert(fn);

			cs->irb.Call(fn, arr);

			cs->irb.Return(arr);

			cs->irb.setCurrentBlock(restore);
			cmpf = func;
		}

		iceAssert(cmpf);
		return cmpf;
	}

	static fir::Function* _getDoRefCountFunctionForArray(CodegenState* cs, fir::ArrayType* arrtype, bool incr)
	{
		auto elmtype = arrtype->getElementType();

		//* note that we don't need a separate name for it, since the type of the array is added to the name itself
		auto name = (incr ? BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME : BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME)
			+ std::string("_") + arrtype->encodedStr();

		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype->getPointerTo() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arrptr = func->getArguments()[0];
			fir::Value* ptr = cs->irb.ConstGEP2(arrptr, 0, 0);
			fir::Value* len = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());

			fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto idxptr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), idxptr);

			cs->irb.UnCondBranch(check);
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLT(cs->irb.Load(idxptr), len);
				cs->irb.CondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				auto valptr = cs->irb.PointerAdd(ptr, cs->irb.Load(idxptr));
				auto val = cs->irb.Load(valptr);

				// if we're a dynamic array, then call the dynamic array version.
				// if it's an array again, then call ourselves.
				if(elmtype->isDynamicArrayType())
				{
					auto fn = (incr ? getIncrementArrayRefCountFunction(cs, elmtype) : getDecrementArrayRefCountFunction(cs, elmtype));
					iceAssert(fn);

					cs->irb.Call(fn, val);
				}
				else if(elmtype->isArrayType())
				{
					// call ourselves. we already declared and everything, so it should be fine.
					cs->irb.Call(func, valptr);
				}
				else
				{
					if(incr)	cs->incrementRefCount(val);
					else		cs->decrementRefCount(val);
				}

				cs->irb.Store(cs->irb.Add(cs->irb.Load(idxptr), fir::ConstantInt::getInt64(1)), idxptr);

				cs->irb.UnCondBranch(check);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);
			cmpf = func;
		}

		iceAssert(cmpf);
		return cmpf;
	}


	fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype)
	{
		if(arrtype->isDynamicArrayType())	return _getDoRefCountFunctionForDynamicArray(cs, arrtype->toDynamicArrayType(), true);
		else								return _getDoRefCountFunctionForArray(cs, arrtype->toArrayType(), true);
	}

	fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype)
	{
		if(arrtype->isDynamicArrayType())	return _getDoRefCountFunctionForDynamicArray(cs, arrtype->toDynamicArrayType(), false);
		else								return _getDoRefCountFunctionForArray(cs, arrtype->toArrayType(), false);
	}












	fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, arrtype }, arrtype), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);


			fir::Value* a1 = func->getArguments()[0];
			fir::Value* a2 = func->getArguments()[1];

			fir::Value* ret = cs->irb.CreateValue(arrtype);

			auto clonef = getCloneFunction(cs, arrtype);
			ret = cs->irb.Call(clonef, a1, fir::ConstantInt::getInt64(0));

			auto appendf = getAppendFunction(cs, arrtype);
			ret = cs->irb.Call(appendf, ret, a2);

			// ok, then

			cs->irb.Return(ret);


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		iceAssert(fn);
		return fn;
	}

	fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::Type* arrtype)
	{
		iceAssert(arrtype);
		iceAssert(arrtype->isDynamicArrayType() || arrtype->isArraySliceType());

		auto name = BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			bool isslice = arrtype->isArraySliceType();

			auto restore = cs->irb.getCurrentBlock();
			auto retTy = fir::TupleType::get({ arrtype, arrtype->getArrayElementType() });

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, fir::Type::getString() }, retTy), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);


			fir::Value* arr = func->getArguments()[0];
			fir::Value* loc = func->getArguments()[1];

			fir::Value* origlen = (isslice ? cs->irb.GetArraySliceLength(arr) : cs->irb.GetDynamicArrayLength(arr));

			fir::IRBlock* fail = cs->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto cond = cs->irb.ICmpLT(origlen, fir::ConstantInt::getInt64(1));

			cs->irb.CondBranch(cond, fail, merge);
			cs->irb.setCurrentBlock(fail);
			{
				printError(cs, loc, "Calling pop() on an empty array\n", { });
			}


			cs->irb.setCurrentBlock(merge);
			{
				auto newlen = cs->irb.Subtract(origlen, fir::ConstantInt::getInt64(1));
				fir::Value* ret = 0;

				// first, load the last value
				if(isslice)
				{
					auto ptr = cs->irb.GetArraySliceData(arr);
					auto val = cs->irb.Load(cs->irb.PointerAdd(ptr, newlen));

					auto newarr = cs->irb.SetArraySliceLength(arr, newlen);
					ret = cs->irb.CreateValue(retTy);
					ret = cs->irb.InsertValue(ret, { 0 }, newarr);
					ret = cs->irb.InsertValue(ret, { 1 }, val);
				}
				else
				{
					auto ptr = cs->irb.GetDynamicArrayData(arr);
					auto val = cs->irb.Load(cs->irb.PointerAdd(ptr, newlen));

					auto newarr = cs->irb.SetDynamicArrayLength(arr, newlen);
					ret = cs->irb.CreateValue(retTy);
					ret = cs->irb.InsertValue(ret, { 0 }, newarr);
					ret = cs->irb.InsertValue(ret, { 1 }, val);
				}

				iceAssert(ret);
				cs->irb.Return(ret);
			}


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}


	//* This makes sure that there is *enough space* for N elements in the array
	//* If there is already enough space, it does nothing.
	fir::Function* getReserveSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		iceAssert(0);
		return 0;
	}





}
}
}

