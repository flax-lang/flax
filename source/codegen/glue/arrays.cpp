// arrays.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"

#define BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME		"__array_boundscheck"
#define BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME	"__array_boundscheckdecomp"

#define BUILTIN_ARRAY_CMP_FUNC_NAME					"__array_compare"

#define BUILTIN_DYNARRAY_CLONE_FUNC_NAME			"__darray_clone"
#define BUILTIN_DYNARRAY_APPEND_FUNC_NAME			"__darray_append"
#define BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME	"__darray_appendelement"
#define BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME			"__darray_popback"
#define BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME	"__darray_combinetwo"

#define BUILTIN_SLICE_CLONE_FUNC_NAME				"__slice_clone"
#define BUILTIN_SLICE_APPEND_FUNC_NAME				"__slice_append"
#define BUILTIN_SLICE_APPEND_ELEMENT_FUNC_NAME		"__slice_appendelement"

#define BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME		"__loop_incr_refcount"
#define BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME		"__loop_decr_refcount"


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
				res = cs->irb.CreateICmpGT(ind, max);

			else
				res = cs->irb.CreateICmpGEQ(ind, max);

			iceAssert(res);

			cs->irb.CreateCondBranch(res, failb, checkneg);
			cs->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cs->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = cs->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::ConstantValue* tmpstr = cs->module->createGlobalString("w");
				fir::ConstantValue* fmtstr = 0;

				if(isPerformingDecomposition)
					fmtstr = cs->module->createGlobalString("%s: Tried to decompose array into '%zd' elements; length is only '%zd'\n");

				else
					fmtstr = cs->module->createGlobalString("%s: Tried to index array at index '%zd'; length is only '%zd'\n");

				iceAssert(fmtstr);

				fir::Value* posstr = cs->irb.CreateGetStringData(func->getArguments()[2]);
				fir::Value* err = cs->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cs->irb.CreateCall(fprintfn, { err, fmtstr, posstr, ind, max });

				cs->irb.CreateCall0(cs->getOrDeclareLibCFunction("abort"));
				cs->irb.CreateUnreachable();
			}

			cs->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res2 = cs->irb.CreateICmpLT(ind, fir::ConstantInt::getInt64(0));
				cs->irb.CreateCondBranch(res2, failb, merge);
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.CreateReturnVoid();
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

		fir::Value* counter = cs->irb.CreateStackAlloc(fir::Type::getInt64());
		cs->irb.CreateStore(startIndex, counter);

		cs->irb.CreateUnCondBranch(loopcond);
		cs->irb.setCurrentBlock(loopcond);
		{
			fir::Value* res = cs->irb.CreateICmpEQ(cs->irb.CreateLoad(counter), len);
			cs->irb.CreateCondBranch(res, merge, loopbody);
		}

		cs->irb.setCurrentBlock(loopbody);
		{
			// make clone
			fir::Value* origElm = cs->irb.CreatePointerAdd(ptr, cs->irb.CreateLoad(counter));
			fir::Value* clone = 0;

			if(fn->getArgumentCount() == 1)
				clone = cs->irb.CreateCall1(fn, cs->irb.CreateLoad(origElm));
			else
				clone = cs->irb.CreateCall2(fn, cs->irb.CreateLoad(origElm), fir::ConstantInt::getInt64(0));

			// store clone
			fir::Value* newElm = cs->irb.CreatePointerAdd(newptr, cs->irb.CreateLoad(counter));
			cs->irb.CreateStore(clone, newElm);

			// increment counter
			cs->irb.CreateStore(cs->irb.CreateAdd(cs->irb.CreateLoad(counter), fir::ConstantInt::getInt64(1)), counter);
			cs->irb.CreateUnCondBranch(loopcond);
		}

		cs->irb.setCurrentBlock(merge);
	}


	static void _handleCallingAppropriateCloneFunction(CodegenState* cs, fir::Function* func, fir::Type* elmType, fir::Value* origptr,
		fir::Value* newptr, fir::Value* origlen, fir::Value* actuallen, fir::Value* startIndex)
	{
		if(elmType->isPrimitiveType() || elmType->isCharType() || elmType->isEnumType())
		{
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			cs->irb.CreateCall(memcpyf, { newptr, cs->irb.CreatePointerTypeCast(cs->irb.CreatePointerAdd(origptr,
				startIndex), fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });
		}
		else if(elmType->isDynamicArrayType())
		{
			// yo dawg i heard you like arrays...
			fir::Function* clonef = getCloneFunction(cs, elmType->toDynamicArrayType());
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.CreatePointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, origptr, origlen, cloneptr, startIndex);
		}
		else if(elmType->isArraySliceType())
		{
			// yo dawg i heard you like arrays...
			fir::Function* clonef = getCloneFunction(cs, elmType->toArraySliceType());
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.CreatePointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, origptr, origlen, cloneptr, startIndex);
		}
		else if(elmType->isStringType())
		{
			fir::Function* clonef = glue::string::getCloneFunction(cs);
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.CreatePointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, origptr, origlen, cloneptr, startIndex);
		}
		else if(elmType->isStructType() || elmType->isClassType() || elmType->isTupleType() || elmType->isArrayType())
		{
			// todo: call copy constructors and stuff

			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			cs->irb.CreateCall(memcpyf, { newptr, cs->irb.CreatePointerTypeCast(cs->irb.CreatePointerAdd(origptr,
				startIndex), fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });
		}
		else
		{
			error("unsupported element type '%s' for array clone", elmType->str());
		}
	}








	fir::Function* getCloneFunction(CodegenState* cs, fir::Type* arrtype)
	{
		if(arrtype->isDynamicArrayType())		return getCloneFunction(cs, arrtype->toDynamicArrayType());
		else if(arrtype->isArraySliceType())	return getCloneFunction(cs, arrtype->toArraySliceType());
		else									error("unsupported type '%s'", arrtype->str());
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

			fir::Value* origptr = cs->irb.CreateGetDynamicArrayData(orig);
			fir::Value* origlen = cs->irb.CreateGetDynamicArrayLength(orig);

			fir::Value* cap = cs->irb.CreateStackAlloc(fir::Type::getInt64());
			cs->irb.CreateStore(cs->irb.CreateGetDynamicArrayCapacity(orig), cap);

			// note: sanity check that len <= cap
			fir::Value* sane = cs->irb.CreateICmpLEQ(origlen, cs->irb.CreateLoad(cap));
			cs->irb.CreateCondBranch(sane, merge1, insane);


			cs->irb.setCurrentBlock(insane);
			{
				// sanity check failed
				// *but* since we're using dyn arrays as vararg arrays,
				// then we just treat capacity = length.

				cs->irb.CreateStore(origlen, cap);
				cs->irb.CreateUnCondBranch(merge1);
			}

			// ok, back to normal
			cs->irb.setCurrentBlock(merge1);

			// ok, alloc a buffer with the original capacity
			// get size in bytes, since cap is in elements
			fir::Value* actuallen = cs->irb.CreateMul(cs->irb.CreateLoad(cap), cs->irb.CreateSizeof(arrtype->getElementType()));


			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* newptr = cs->irb.CreateCall1(mallocf, actuallen);

			fir::Type* elmType = arrtype->getElementType();
			_handleCallingAppropriateCloneFunction(cs, func, elmType, origptr, newptr, origlen, actuallen, startIndex);

			fir::Value* newarr = cs->irb.CreateValue(arrtype);
			newarr = cs->irb.CreateSetDynamicArrayData(newarr, cs->irb.CreatePointerTypeCast(newptr, arrtype->getElementType()->getPointerTo()));
			newarr = cs->irb.CreateSetDynamicArrayLength(newarr, cs->irb.CreateSub(origlen, startIndex));
			newarr = cs->irb.CreateSetDynamicArrayCapacity(newarr, cs->irb.CreateLoad(cap));

			cs->irb.CreateReturn(newarr);

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

			fir::Value* origptr = cs->irb.CreateGetArraySliceData(orig);
			fir::Value* origlen = cs->irb.CreateGetArraySliceLength(orig);

			cs->irb.CreateUnCondBranch(merge1);

			// ok, back to normal
			cs->irb.setCurrentBlock(merge1);

			// ok, alloc a buffer with the original capacity
			// get size in bytes, since cap is in elements
			fir::Value* actuallen = cs->irb.CreateMul(origlen, cs->irb.CreateSizeof(arrtype->getElementType()));

			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* newptr = cs->irb.CreateCall1(mallocf, actuallen);

			fir::Type* elmType = arrtype->getElementType();
			_handleCallingAppropriateCloneFunction(cs, func, elmType, origptr, newptr, origlen, actuallen, startIndex);

			fir::Value* newarr = cs->irb.CreateValue(fir::DynamicArrayType::get(arrtype->getElementType()));
			newarr = cs->irb.CreateSetDynamicArrayData(newarr, cs->irb.CreatePointerTypeCast(newptr, arrtype->getElementType()->getPointerTo()));
			newarr = cs->irb.CreateSetDynamicArrayLength(newarr, cs->irb.CreateSub(origlen, startIndex));
			newarr = cs->irb.CreateSetDynamicArrayCapacity(newarr, cs->irb.CreateSub(origlen, startIndex));

			cs->irb.CreateReturn(newarr);

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


	// required is how much *EXTRA* space we need.
	static fir::Value* _checkCapacityAndGrowIfNeeded(CodegenState* cs, fir::Function* func, fir::Value* arr, fir::Value* required)
	{
		iceAssert(arr->getType()->isDynamicArrayType());
		iceAssert(required->getType() == fir::Type::getInt64());

		auto elmtype = arr->getType()->getArrayElementType();

		fir::Value* ptr = cs->irb.CreateGetDynamicArrayData(arr, "ptr");
		fir::Value* len = cs->irb.CreateGetDynamicArrayLength(arr, "len");
		fir::Value* cap = cs->irb.CreateGetDynamicArrayCapacity(arr, "cap");

		// check if len + required > cap
		fir::Value* needed = cs->irb.CreateAdd(len, required, "needed");
		fir::Value* cond = cs->irb.CreateICmpGT(needed, cap);



		// TODO: ugly hack to force visiting the incoming values before we visit ourselves.
		// THE ORDER OF THESE BLOCKS IS VERY IMPORTANT (FOR NOW)

		fir::IRBlock* trampoline = cs->irb.addNewBlockInFunction("trampoline", func);

		fir::IRBlock* growblk = cs->irb.addNewBlockInFunction("grow", func);

		// for when the 'dynamic' array came from a literal. same as the usual stuff
		// capacity will be -1, in this case.
		fir::IRBlock* growNewblk = cs->irb.addNewBlockInFunction("growFromScratch", func);

		fir::IRBlock* forceblk = cs->irb.addNewBlockInFunction("make_phi", func);

		fir::IRBlock* mergeblk = cs->irb.addNewBlockInFunction("merge", func);

		cs->irb.CreateUnCondBranch(forceblk);


		// return a phi node.
		cs->irb.setCurrentBlock(forceblk);
		auto phi = cs->irb.CreatePHINode(arr->getType());
		{
			phi->addIncoming(arr, cs->irb.getCurrentBlock());
			cs->irb.CreateCondBranch(cond, trampoline, mergeblk);
		}

		cs->irb.setCurrentBlock(trampoline);
		{
			cond = cs->irb.CreateICmpEQ(cap, fir::ConstantInt::getInt64(-1));
			cs->irb.CreateCondBranch(cond, growNewblk, growblk);
		}

		// grows to the nearest power of two from (len + required)
		cs->irb.setCurrentBlock(growblk);
		{
			fir::Function* p2func = cs->module->getIntrinsicFunction("roundup_pow2");
			iceAssert(p2func);

			fir::Value* nextpow2 = cs->irb.CreateCall1(p2func, needed, "nextpow2");

			fir::Function* refunc = cs->getOrDeclareLibCFunction(REALLOCATE_MEMORY_FUNC);
			iceAssert(refunc);

			fir::Value* actuallen = cs->irb.CreateMul(nextpow2, cs->irb.CreateSizeof(elmtype));
			fir::Value* newptr = cs->irb.CreateCall2(refunc, cs->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()), actuallen);

			fir::Value* ret = cs->irb.CreateSetDynamicArrayData(arr, cs->irb.CreatePointerTypeCast(newptr, ptr->getType()));
			ret = cs->irb.CreateSetDynamicArrayCapacity(ret, nextpow2);

			cs->irb.CreateUnCondBranch(mergeblk);

			phi->addIncoming(ret, growblk);
		}


		// makes a new memory piece, to the nearest power of two from (len + required)
		cs->irb.setCurrentBlock(growNewblk);
		{
			fir::Function* p2func = cs->module->getIntrinsicFunction("roundup_pow2");
			iceAssert(p2func);

			fir::Value* nextpow2 = cs->irb.CreateCall1(p2func, needed, "nextpow2");

			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* actuallen = cs->irb.CreateMul(nextpow2, cs->irb.CreateSizeof(elmtype));
			fir::Value* newptr = cs->irb.CreateCall1(mallocf, actuallen);

			// memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			cs->irb.CreateCall(memcpyf, { cs->irb.CreatePointerTypeCast(newptr, fir::Type::getInt8Ptr()),
				cs->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0),
				fir::ConstantBool::get(false) });


			fir::Value* ret = cs->irb.CreateSetDynamicArrayData(arr, cs->irb.CreatePointerTypeCast(newptr, ptr->getType()));
			ret = cs->irb.CreateSetDynamicArrayCapacity(ret, nextpow2);

			cs->irb.CreateUnCondBranch(mergeblk);

			phi->addIncoming(ret, growNewblk);
		}

		cs->irb.setCurrentBlock(mergeblk);
		return phi;
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
				fir::Value* origlen = cs->irb.CreateGetDynamicArrayLength(s1);
				fir::Value* applen = cs->irb.CreateGetDynamicArrayLength(s2);

				// grow if needed
				s1 = _checkCapacityAndGrowIfNeeded(cs, func, s1, applen);

				// // we should be ok, now copy.
				fir::Value* ptr = cs->irb.CreateGetDynamicArrayData(s1);
				ptr = cs->irb.CreatePointerAdd(ptr, origlen);

				fir::Value* s2ptr = cs->irb.CreateGetDynamicArrayData(s2);

				fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

				fir::Value* actuallen = cs->irb.CreateMul(applen, cs->irb.CreateSizeof(arrtype->getElementType()));

				cs->irb.CreateCall(memcpyf, { cs->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()),
					cs->irb.CreatePointerTypeCast(s2ptr, fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0),
					fir::ConstantBool::get(false) });

				// increase the length
				s1 = cs->irb.CreateSetDynamicArrayLength(s1, cs->irb.CreateAdd(origlen, applen));


				if(cs->isRefCountedType(elmType))
				{
					// loop through the source array (Y in X + Y)

					fir::IRBlock* cond = cs->irb.addNewBlockInFunction("loopCond", func);
					fir::IRBlock* body = cs->irb.addNewBlockInFunction("loopBody", func);
					fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

					fir::Value* ctrPtr = cs->irb.CreateStackAlloc(fir::Type::getInt64());
					cs->irb.CreateStore(fir::ConstantInt::getInt64(0), ctrPtr);

					fir::Value* s2len = cs->irb.CreateGetDynamicArrayLength(s2);
					cs->irb.CreateUnCondBranch(cond);

					cs->irb.setCurrentBlock(cond);
					{
						// check the condition
						fir::Value* ctr = cs->irb.CreateLoad(ctrPtr);
						fir::Value* res = cs->irb.CreateICmpLT(ctr, s2len);

						cs->irb.CreateCondBranch(res, body, merge);
					}

					cs->irb.setCurrentBlock(body);
					{
						// increment refcount
						fir::Value* val = cs->irb.CreateLoad(cs->irb.CreatePointerAdd(s2ptr, cs->irb.CreateLoad(ctrPtr)));

						cs->incrementRefCount(val);

						// increment counter
						cs->irb.CreateStore(cs->irb.CreateAdd(fir::ConstantInt::getInt64(1), cs->irb.CreateLoad(ctrPtr)), ctrPtr);
						cs->irb.CreateUnCondBranch(cond);
					}

					cs->irb.setCurrentBlock(merge);
				}


				// ok done.
				cs->irb.CreateReturn(s1);
			}


			appendf = func;
			cs->irb.setCurrentBlock(restore);
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
				fir::Value* origlen = cs->irb.CreateGetDynamicArrayLength(s1);
				fir::Value* applen = fir::ConstantInt::getInt64(1);

				// grow if needed
				s1 = _checkCapacityAndGrowIfNeeded(cs, func, s1, applen);


				// we should be ok, now copy.
				fir::Value* ptr = cs->irb.CreateGetDynamicArrayData(s1);
				ptr = cs->irb.CreatePointerAdd(ptr, origlen);

				cs->irb.CreateStore(s2, ptr);

				if(cs->isRefCountedType(elmType))
					cs->incrementRefCount(s2);

				// increase the length
				s1 = cs->irb.CreateSetDynamicArrayLength(s1, cs->irb.CreateAdd(origlen, applen));

				// ok done.
				cs->irb.CreateReturn(s1);
			}


			appendf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
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
			ptr1 = cs->irb.CreateGetDynamicArrayData(arg1);
			ptr2 = cs->irb.CreateGetDynamicArrayData(arg2);
		}
		else if(arrtype->isArraySliceType())
		{
			ptr1 = cs->irb.CreateGetArraySliceData(arg1);
			ptr2 = cs->irb.CreateGetArraySliceData(arg2);
		}
		else if(arrtype->isArrayType())
		{
			ptr1 = cs->irb.CreateConstGEP2(arg1, 0, 0);
			ptr2 = cs->irb.CreateConstGEP2(arg2, 0, 0);
		}
		else
		{
			error("invalid type '%s'", arrtype->str());
		}

		fir::Value* len1 = 0; fir::Value* len2 = 0;

		if(arrtype->isDynamicArrayType())
		{
			len1 = cs->irb.CreateGetDynamicArrayLength(arg1);
			len2 = cs->irb.CreateGetDynamicArrayLength(arg2);
		}
		else if(arrtype->isArraySliceType())
		{
			len1 = cs->irb.CreateGetArraySliceLength(arg1);
			len2 = cs->irb.CreateGetArraySliceLength(arg2);
		}
		else if(arrtype->isArrayType())
		{
			len1 = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());
			len2 = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());
		}
		else
		{
			error("invalid type '%s'", arrtype->str());
		}

		// we compare to this to break
		fir::Value* counter = cs->irb.CreateStackAlloc(fir::Type::getInt64());
		cs->irb.CreateStore(zeroval, counter);

		fir::Value* res = cs->irb.CreateStackAlloc(fir::Type::getInt64());
		cs->irb.CreateStore(zeroval, res);


		cs->irb.CreateUnCondBranch(cond);
		cs->irb.setCurrentBlock(cond);
		{
			fir::IRBlock* retlt = cs->irb.addNewBlockInFunction("retlt", func);
			fir::IRBlock* reteq = cs->irb.addNewBlockInFunction("reteq", func);
			fir::IRBlock* retgt = cs->irb.addNewBlockInFunction("retgt", func);

			fir::IRBlock* tmp1 = cs->irb.addNewBlockInFunction("tmp1", func);
			fir::IRBlock* tmp2 = cs->irb.addNewBlockInFunction("tmp2", func);

			// if we got here, the arrays were equal *up to this point*
			// if ptr1 exceeds or ptr2 exceeds, return len1 - len2

			fir::Value* t1 = cs->irb.CreateICmpEQ(cs->irb.CreateLoad(counter), len1);
			fir::Value* t2 = cs->irb.CreateICmpEQ(cs->irb.CreateLoad(counter), len2);

			// if t1 is over, goto tmp1, if not goto t2
			cs->irb.CreateCondBranch(t1, tmp1, tmp2);
			cs->irb.setCurrentBlock(tmp1);
			{
				// t1 is over
				// check if t2 is over
				// if so, return 0 (b == a)
				// if not, return -1 (b > a)

				cs->irb.CreateCondBranch(t2, reteq, retlt);
			}

			cs->irb.setCurrentBlock(tmp2);
			{
				// t1 is not over
				// check if t2 is over
				// if so, return 1 (a > b)
				// if not, goto body

				cs->irb.CreateCondBranch(t2, retgt, body);
			}


			cs->irb.setCurrentBlock(retlt);
			cs->irb.CreateReturn(fir::ConstantInt::getInt64(-1));

			cs->irb.setCurrentBlock(reteq);
			cs->irb.CreateReturn(fir::ConstantInt::getInt64(0));

			cs->irb.setCurrentBlock(retgt);
			cs->irb.CreateReturn(fir::ConstantInt::getInt64(+1));
		}


		cs->irb.setCurrentBlock(body);
		{
			// fir::Value* v1 = cs->irb.CreateLoad(cs->irb.CreatePointerAdd(ptr1, cs->irb.CreateLoad(counter)));
			// fir::Value* v2 = cs->irb.CreateLoad(cs->irb.CreatePointerAdd(ptr2, cs->irb.CreateLoad(counter)));

			// fir::Value* c = Operators::performGeneralArithmeticOperator(cs, ArithmeticOp::CmpEq, 0, v1, cs->irb.CreatePointerAdd(ptr1, cs->irb.CreateLoad(counter)), v2, cs->irb.CreatePointerAdd(ptr2, cs->irb.CreateLoad(counter)), { }).value;

			fir::Value* c = 0;

			// c is a bool, because it's very generic in nature
			// so we just take !c and convert to i64 to get our result.
			// if c == true, then lhs == rhs, and so we should have 0.

			c = cs->irb.CreateLogicalNot(c);
			c = cs->irb.CreateIntSizeCast(c, fir::Type::getInt64());

			cs->irb.CreateStore(c, res);

			// compare to 0.
			fir::Value* cmpres = cs->irb.CreateICmpEQ(cs->irb.CreateLoad(res), zeroval);

			// if equal, go to incr, if not return directly
			cs->irb.CreateCondBranch(cmpres, incr, merge);
		}


		cs->irb.setCurrentBlock(incr);
		{
			cs->irb.CreateStore(cs->irb.CreateAdd(cs->irb.CreateLoad(counter), oneval), counter);
			cs->irb.CreateUnCondBranch(cond);
		}



		cs->irb.setCurrentBlock(merge);
		{
			// load and return
			cs->irb.CreateReturn(cs->irb.CreateLoad(res));
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


			cmpf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}











	static fir::Function* _getDoRefCountFunction(CodegenState* cs, fir::Type* elmtype, bool increment)
	{
		iceAssert(elmtype);
		iceAssert(cs->isRefCountedType(elmtype) && "not refcounted type");

		auto name = (increment ? BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME : BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME)
			+ std::string("_") + elmtype->encodedStr();

		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ elmtype->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid()),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* ptr = func->getArguments()[0];
			fir::Value* len = func->getArguments()[1];



			// ok, we need to decrement the refcount of *ALL* THE FUCKING STRINGS
			// in this shit

			// loop from 0 to len
			fir::IRBlock* cond = cs->irb.addNewBlockInFunction("cond", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			fir::Value* counter = cs->irb.CreateStackAlloc(fir::Type::getInt64());
			cs->irb.CreateStore(fir::ConstantInt::getInt64(0), counter);

			cs->irb.CreateUnCondBranch(cond);
			cs->irb.setCurrentBlock(cond);
			{
				// check
				fir::Value* cond = cs->irb.CreateICmpLT(cs->irb.CreateLoad(counter), len);
				cs->irb.CreateCondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				// ok. first, do pointer arithmetic to get the current array
				fir::Value* strp = cs->irb.CreatePointerAdd(ptr, cs->irb.CreateLoad(counter));

				if(increment)	cs->incrementRefCount(cs->irb.CreateLoad(strp));
				else			cs->decrementRefCount(cs->irb.CreateLoad(strp));

				// increment counter
				cs->irb.CreateStore(cs->irb.CreateAdd(cs->irb.CreateLoad(counter), fir::ConstantInt::getInt64(1)), counter);

				// branch to top
				cs->irb.CreateUnCondBranch(cond);
			}

			// merge:
			cs->irb.setCurrentBlock(merge);
			cs->irb.CreateReturnVoid();




			cmpf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}

	fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* elmtype)
	{
		return _getDoRefCountFunction(cs, elmtype, true);
	}

	fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* elmtype)
	{
		return _getDoRefCountFunction(cs, elmtype, false);
	}












	fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
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
			ret = cs->irb.CreateCall2(clonef, a1, fir::ConstantInt::getInt64(0));

			auto appendf = getAppendFunction(cs, arrtype);
			ret = cs->irb.CreateCall2(appendf, ret, a2);

			// ok, then

			cs->irb.CreateReturn(ret);


			cmpf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}

	fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getReserveSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getReserveExtraSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}
}
}
}

