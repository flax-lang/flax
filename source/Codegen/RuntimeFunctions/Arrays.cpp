// Arrays.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

using namespace Codegen;
using namespace Ast;

#define BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME		"__.array_boundscheck"

#define BUILTIN_DYNARRAY_CLONE_FUNC_NAME			"__.darray_clone"
#define BUILTIN_DYNARRAY_APPEND_FUNC_NAME			"__.darray_append"
#define BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME	"__.darray_appendelement"
#define BUILTIN_DYNARRAY_CMP_FUNC_NAME				"__.darray_compare"
#define BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME			"__.darray_popback"
#define BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME	"__.darray_combinetwo"

namespace Codegen {
namespace RuntimeFuncs {
namespace Array
{
	fir::Function* getBoundsCheckFunction(CodegenInstance* cgi)
	{
		fir::Function* fn = cgi->module->getFunction(Identifier(BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cgi->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* checkneg = cgi->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

			cgi->irb.setCurrentBlock(entry);

			fir::Value* max = func->getArguments()[0];
			fir::Value* ind = func->getArguments()[1];

			fir::Value* res = cgi->irb.CreateICmpGEQ(ind, max);

			cgi->irb.CreateCondBranch(res, failb, checkneg);
			cgi->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cgi->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = cgi->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::ConstantValue* tmpstr = cgi->module->createGlobalString("w");
				fir::ConstantValue* fmtstr = cgi->module->createGlobalString("Tried to index array at index '%zd'; length is only '%zd'\n");

				fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cgi->irb.CreateCall(fprintfn, { err, fmtstr, ind, max });

				cgi->irb.CreateCall0(cgi->getOrDeclareLibCFunc("abort"));
				cgi->irb.CreateUnreachable();
			}

			cgi->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res2 = cgi->irb.CreateICmpLT(ind, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(res2, failb, merge);
			}

			cgi->irb.setCurrentBlock(merge);
			{
				cgi->irb.CreateReturnVoid();
			}

			fn = func;

			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}





	static void _callCloneFunctionInLoop(CodegenInstance* cgi, fir::Function* curfunc, fir::Function* fn,
		fir::Value* ptr, fir::Value* len, fir::Value* newptr)
	{
		fir::IRBlock* loopcond = cgi->irb.addNewBlockInFunction("loopcond", curfunc);
		fir::IRBlock* loopbody = cgi->irb.addNewBlockInFunction("loopbody", curfunc);
		fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", curfunc);

		fir::Value* counter = cgi->irb.CreateStackAlloc(fir::Type::getInt64());
		cgi->irb.CreateStore(fir::ConstantInt::getInt64(0), counter);

		cgi->irb.CreateUnCondBranch(loopcond);
		cgi->irb.setCurrentBlock(loopcond);
		{
			fir::Value* res = cgi->irb.CreateICmpEQ(cgi->irb.CreateLoad(counter), len);
			cgi->irb.CreateCondBranch(res, merge, loopbody);
		}

		cgi->irb.setCurrentBlock(loopbody);
		{
			// make clone
			fir::Value* origElm = cgi->irb.CreatePointerAdd(ptr, cgi->irb.CreateLoad(counter));
			fir::Value* clone = cgi->irb.CreateCall1(fn, origElm);

			// store clone
			fir::Value* newElm = cgi->irb.CreatePointerAdd(newptr, cgi->irb.CreateLoad(counter));
			cgi->irb.CreateStore(clone, newElm);

			// increment counter
			cgi->irb.CreateStore(cgi->irb.CreateAdd(cgi->irb.CreateLoad(counter), fir::ConstantInt::getInt64(1)), counter);
			cgi->irb.CreateUnCondBranch(loopcond);
		}

		cgi->irb.setCurrentBlock(merge);
	}

	fir::Function* getCloneFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		auto name = BUILTIN_DYNARRAY_CLONE_FUNC_NAME + std::string("_") + arrtype->getElementType()->encodedStr();

		fir::Function* fn = cgi->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype->getPointerTo() }, arrtype, false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* insane = cgi->irb.addNewBlockInFunction("insane", func);
			fir::IRBlock* merge1 = cgi->irb.addNewBlockInFunction("merge1", func);

			cgi->irb.setCurrentBlock(entry);

			fir::Value* orig = func->getArguments()[0];
			iceAssert(orig);

			fir::Value* origptr = cgi->irb.CreateGetDynamicArrayData(orig);
			fir::Value* origlen = cgi->irb.CreateGetDynamicArrayLength(orig);
			fir::Value* origcap = cgi->irb.CreateGetDynamicArrayCapacity(orig);

			// note: sanity check that len <= cap
			fir::Value* sane = cgi->irb.CreateICmpLEQ(origlen, origcap);
			cgi->irb.CreateCondBranch(sane, merge1, insane);


			cgi->irb.setCurrentBlock(insane);
			{
				// sanity check failed

				fir::Function* fprintfn = cgi->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = cgi->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				fir::ConstantValue* tmpstr = cgi->module->createGlobalString("w");
				fir::ConstantValue* fmtstr = cgi->module->createGlobalString("Sanity check failed (length '%zd' somehow > capacity '%zd') for array\n");

				fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cgi->irb.CreateCall(fprintfn, { err, fmtstr, origlen, origcap });

				cgi->irb.CreateCall0(cgi->getOrDeclareLibCFunc("abort"));
				cgi->irb.CreateUnreachable();
			}

			// ok, back to normal
			cgi->irb.setCurrentBlock(merge1);

			// ok, alloc a buffer with the original capacity
			// get size in bytes, since cap is in elements
			fir::Value* actuallen = cgi->irb.CreateMul(origcap, cgi->irb.CreateSizeof(arrtype->getElementType()));


			// fir::ConstantInt::getInt64(cgi->execTarget->getTypeSizeInBytes(arrtype->getElementType())));

			fir::Function* mallocf = cgi->getOrDeclareLibCFunc(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* newptr = cgi->irb.CreateCall1(mallocf, actuallen);


			fir::Type* elmType = arrtype->getElementType();

			if(elmType->isPrimitiveType() || elmType->isCharType() || elmType->isEnumType())
			{
				fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");

				cgi->irb.CreateCall(memcpyf, { newptr, cgi->irb.CreatePointerTypeCast(origptr, fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });
			}
			else if(elmType->isDynamicArrayType())
			{
				// yo dawg i heard you like arrays...
				fir::Function* clonef = getCloneFunction(cgi, elmType->toDynamicArrayType());
				iceAssert(clonef);

				// loop
				fir::Value* cloneptr = cgi->irb.CreatePointerTypeCast(newptr, elmType->getPointerTo());
				_callCloneFunctionInLoop(cgi, func, clonef, origptr, origlen, cloneptr);
			}
			else if(elmType->isStringType())
			{
				fir::Function* clonef = String::getCloneFunction(cgi);
				iceAssert(clonef);

				// loop
				fir::Value* cloneptr = cgi->irb.CreatePointerTypeCast(newptr, elmType->getPointerTo());
				_callCloneFunctionInLoop(cgi, func, clonef, origptr, origlen, cloneptr);
			}
			else if(elmType->isStructType() || elmType->isClassType() || elmType->isTupleType() || elmType->isArrayType())
			{
				// todo: call copy constructors and stuff

				fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");

				cgi->irb.CreateCall(memcpyf, { newptr, cgi->irb.CreatePointerTypeCast(origptr, fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });
			}

			fir::Value* newarr = cgi->irb.CreateStackAlloc(arrtype);
			cgi->irb.CreateSetDynamicArrayData(newarr, cgi->irb.CreatePointerTypeCast(newptr, arrtype->getElementType()->getPointerTo()));
			cgi->irb.CreateSetDynamicArrayLength(newarr, origlen);
			cgi->irb.CreateSetDynamicArrayCapacity(newarr, origcap);

			fir::Value* ret = cgi->irb.CreateLoad(newarr);
			cgi->irb.CreateReturn(ret);

			fn = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






	// static void _growCapacityBy(CodegenInstance* cgi, fir::Function* func, fir::Value* arr, fir::Value* amt)
	// {
	// 	iceAssert(arr->getType()->isPointerType());
	// 	iceAssert(arr->getType()->getPointerElementType()->isDynamicArrayType());
	// 	iceAssert(amt->getType() == fir::Type::getInt64());
	// }


	// required is how much *EXTRA* space we need.
	static void _checkCapacityAndGrowIfNeeded(CodegenInstance* cgi, fir::Function* func, fir::Value* arr, fir::Value* required)
	{
		iceAssert(arr->getType()->isPointerType());
		iceAssert(arr->getType()->getPointerElementType()->isDynamicArrayType());
		iceAssert(required->getType() == fir::Type::getInt64());

		auto elmtype = arr->getType()->getPointerElementType()->toDynamicArrayType()->getElementType();

		fir::Value* ptr = cgi->irb.CreateGetDynamicArrayData(arr, "ptr");
		fir::Value* len = cgi->irb.CreateGetDynamicArrayLength(arr, "len");
		fir::Value* cap = cgi->irb.CreateGetDynamicArrayCapacity(arr, "cap");

		// check if len + required > cap
		fir::Value* needed = cgi->irb.CreateAdd(len, required, "needed");
		fir::Value* cond = cgi->irb.CreateICmpGT(needed, cap);

		fir::IRBlock* growblk = cgi->irb.addNewBlockInFunction("grow", func);
		fir::IRBlock* mergeblk = cgi->irb.addNewBlockInFunction("merge", func);

		cgi->irb.CreateCondBranch(cond, growblk, mergeblk);


		// grows to the nearest power of two from (len + required)
		cgi->irb.setCurrentBlock(growblk);
		{
			fir::Function* p2func = cgi->module->getIntrinsicFunction("roundup_pow2");
			iceAssert(p2func);

			fir::Value* nextpow2 = cgi->irb.CreateCall1(p2func, needed, "nextpow2");


			fir::Function* refunc = cgi->getOrDeclareLibCFunc(REALLOCATE_MEMORY_FUNC);
			iceAssert(refunc);


			// fir::Value* actuallen = cgi->irb.CreateMul(nextpow2, fir::ConstantInt::getInt64(cgi->execTarget->getTypeSizeInBytes(elmtype)));
			fir::Value* actuallen = cgi->irb.CreateMul(nextpow2, cgi->irb.CreateSizeof(elmtype));
			fir::Value* newptr = cgi->irb.CreateCall2(refunc, cgi->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()), actuallen);

			cgi->irb.CreateSetDynamicArrayData(arr, cgi->irb.CreatePointerTypeCast(newptr, ptr->getType()));
			cgi->irb.CreateSetDynamicArrayCapacity(arr, nextpow2);

			cgi->irb.CreateUnCondBranch(mergeblk);
		}

		cgi->irb.setCurrentBlock(mergeblk);
	}




	fir::Function* getAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_APPEND_FUNC_NAME + std::string("_") + arrtype->getElementType()->encodedStr();
		fir::Function* appendf = cgi->module->getFunction(Identifier(name, IdKind::Name));

		if(!appendf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype->getPointerTo(), arrtype->getPointerTo() },
					fir::Type::getVoid(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			// get the second one
			{
				fir::Value* origlen = cgi->irb.CreateGetDynamicArrayLength(s1);
				fir::Value* applen = cgi->irb.CreateGetDynamicArrayLength(s2);

				// grow if needed
				_checkCapacityAndGrowIfNeeded(cgi, func, s1, applen);


				// // we should be ok, now copy.
				fir::Value* ptr = cgi->irb.CreateGetDynamicArrayData(s1);
				ptr = cgi->irb.CreatePointerAdd(ptr, origlen);

				fir::Value* s2ptr = cgi->irb.CreateGetDynamicArrayData(s2);

				fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");

				// fir::Value* actuallen = cgi->irb.CreateMul(applen,
				// 	fir::ConstantInt::getInt64(cgi->execTarget->getTypeSizeInBytes(arrtype->getElementType())));

				fir::Value* actuallen = cgi->irb.CreateMul(applen, cgi->irb.CreateSizeof(arrtype->getElementType()));

				cgi->irb.CreateCall(memcpyf, { cgi->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()),
					cgi->irb.CreatePointerTypeCast(s2ptr, fir::Type::getInt8Ptr()), actuallen, fir::ConstantInt::getInt32(0),
					fir::ConstantInt::getBool(0) });

				// // increase the length
				cgi->irb.CreateSetDynamicArrayLength(s1, cgi->irb.CreateAdd(origlen, applen));

				// ok done.
				cgi->irb.CreateReturnVoid();
			}


			appendf = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}

	fir::Function* getElementAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME + std::string("_") + arrtype->getElementType()->encodedStr();
		fir::Function* appendf = cgi->module->getFunction(Identifier(name, IdKind::Name));

		if(!appendf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype->getPointerTo(), arrtype->getElementType() },
					fir::Type::getVoid(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			// get the second one
			{
				fir::Value* origlen = cgi->irb.CreateGetDynamicArrayLength(s1);
				fir::Value* applen = fir::ConstantInt::getInt64(1);

				// grow if needed
				_checkCapacityAndGrowIfNeeded(cgi, func, s1, applen);


				// we should be ok, now copy.
				fir::Value* ptr = cgi->irb.CreateGetDynamicArrayData(s1);
				ptr = cgi->irb.CreatePointerAdd(ptr, origlen);

				cgi->irb.CreateStore(s2, ptr);

				// increase the length
				cgi->irb.CreateSetDynamicArrayLength(s1, cgi->irb.CreateAdd(origlen, applen));

				// ok done.
				cgi->irb.CreateReturnVoid();
			}


			appendf = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}







	static void _compareFunctionUsingBuiltinCompare(CodegenInstance* cgi, fir::DynamicArrayType* arrtype, fir::Function* func,
		fir::Value* arg1, fir::Value* arg2)
	{
		// ok, ez.
		fir::Value* zeroval = fir::ConstantInt::getInt64(0);
		fir::Value* oneval = fir::ConstantInt::getInt64(1);

		fir::IRBlock* cond = cgi->irb.addNewBlockInFunction("cond", func);
		fir::IRBlock* body = cgi->irb.addNewBlockInFunction("body", func);
		fir::IRBlock* incr = cgi->irb.addNewBlockInFunction("incr", func);
		fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

		fir::Value* ptr1 = cgi->irb.CreateGetDynamicArrayData(arg1);
		fir::Value* ptr2 = cgi->irb.CreateGetDynamicArrayData(arg2);

		fir::Value* len1 = cgi->irb.CreateGetDynamicArrayLength(arg1);
		fir::Value* len2 = cgi->irb.CreateGetDynamicArrayLength(arg2);

		// we compare to this to break
		fir::Value* counter = cgi->irb.CreateStackAlloc(fir::Type::getInt64());
		cgi->irb.CreateStore(zeroval, counter);

		fir::Value* res = cgi->irb.CreateStackAlloc(fir::Type::getInt64());
		cgi->irb.CreateStore(zeroval, res);


		cgi->irb.CreateUnCondBranch(cond);
		cgi->irb.setCurrentBlock(cond);
		{
			fir::IRBlock* retlt = cgi->irb.addNewBlockInFunction("retlt", func);
			fir::IRBlock* reteq = cgi->irb.addNewBlockInFunction("reteq", func);
			fir::IRBlock* retgt = cgi->irb.addNewBlockInFunction("retgt", func);

			fir::IRBlock* tmp1 = cgi->irb.addNewBlockInFunction("tmp1", func);
			fir::IRBlock* tmp2 = cgi->irb.addNewBlockInFunction("tmp2", func);

			// if we got here, the arrays were equal *up to this point*
			// if ptr1 exceeds or ptr2 exceeds, return len1 - len2

			fir::Value* t1 = cgi->irb.CreateICmpEQ(cgi->irb.CreateLoad(counter), len1);
			fir::Value* t2 = cgi->irb.CreateICmpEQ(cgi->irb.CreateLoad(counter), len2);

			// if t1 is over, goto tmp1, if not goto t2
			cgi->irb.CreateCondBranch(t1, tmp1, tmp2);
			cgi->irb.setCurrentBlock(tmp1);
			{
				// t1 is over
				// check if t2 is over
				// if so, return 0 (b == a)
				// if not, return -1 (b > a)

				cgi->irb.CreateCondBranch(t2, reteq, retlt);
			}

			cgi->irb.setCurrentBlock(tmp2);
			{
				// t1 is not over
				// check if t2 is over
				// if so, return 1 (a > b)
				// if not, goto body

				cgi->irb.CreateCondBranch(t2, retgt, body);
			}


			cgi->irb.setCurrentBlock(retlt);
			cgi->irb.CreateReturn(fir::ConstantInt::getInt64(-1));

			cgi->irb.setCurrentBlock(reteq);
			cgi->irb.CreateReturn(fir::ConstantInt::getInt64(0));

			cgi->irb.setCurrentBlock(retgt);
			cgi->irb.CreateReturn(fir::ConstantInt::getInt64(+1));
		}


		cgi->irb.setCurrentBlock(body);
		{
			if(arrtype->getElementType()->isStringType())
			{
				fir::Function* strf = RuntimeFuncs::String::getCompareFunction(cgi);
				iceAssert(strf);

				fir::Value* c = cgi->irb.CreateCall2(strf, ptr1, ptr2);
				cgi->irb.CreateStore(c, res);
			}
			else
			{
				fir::Value* v1 = cgi->irb.CreateLoad(ptr1);
				fir::Value* v2 = cgi->irb.CreateLoad(ptr2);

				cgi->irb.CreateStore(cgi->irb.CreateICmpMulti(v1, v2), res);
			}

			// compare to 0.
			fir::Value* cmpres = cgi->irb.CreateICmpEQ(cgi->irb.CreateLoad(res), zeroval);

			// if equal, go to incr, if not return directly
			cgi->irb.CreateCondBranch(cmpres, incr, merge);
		}


		cgi->irb.setCurrentBlock(incr);
		{
			cgi->irb.CreateStore(cgi->irb.CreateAdd(cgi->irb.CreateLoad(counter), oneval), counter);
			cgi->irb.CreateUnCondBranch(cond);
		}



		cgi->irb.setCurrentBlock(merge);
		{
			// load and return
			cgi->irb.CreateReturn(cgi->irb.CreateLoad(res));
		}
	}


	static void _compareFunctionUsingOperatorFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype, fir::Function* curfunc,
		fir::Value* arg1, fir::Value* arg2, fir::Function* opf)
	{
		// fir::Value* zeroval = fir::ConstantInt::getInt64(0);
		error("notsup");
	}



	fir::Function* getCompareFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype, fir::Function* opf)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_DYNARRAY_CMP_FUNC_NAME + std::string("_") + arrtype->getElementType()->encodedStr();
		fir::Function* cmpf = cgi->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype->getPointerTo(), arrtype->getPointerTo() },
					fir::Type::getInt64(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			{
				// check our situation.
				if(opf == 0)
				{
					_compareFunctionUsingBuiltinCompare(cgi, arrtype, func, s1, s2);
				}
				else
				{
					_compareFunctionUsingOperatorFunction(cgi, arrtype, func, s1, s2, opf);
				}

				// functions above do their own return
			}


			cmpf = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}

	fir::Function* getConstructFromTwoFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getPopElementFromBackFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getReserveSpaceForElementsFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getReserveExtraSpaceForElementsFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}
}
}
}



















