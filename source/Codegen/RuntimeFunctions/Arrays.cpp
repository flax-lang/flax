// Arrays.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

using namespace Codegen;
using namespace Ast;

#define BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME		"__.array_boundscheck"

#define BUILTIN_DYNARRAY_APPEND_FUNC_NAME			"__.darray_append"
#define BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME	"__.darray_appendelement"
#define BUILTIN_DYNARRAY_CMP_FUNC_NAME				"__.darray_compare"
#define BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME			"__.darray_popback"
#define BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME	"__.darray_combinetwo"

namespace RuntimeFuncs
{
	fir::Function* getArrayBoundsCheckFunction(CodegenInstance* cgi)
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

				fir::Value* tmpstr = cgi->module->createGlobalString("w");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = cgi->module->createGlobalString("Tried to index array at index '%zd'; length is only '%zd'! (max index is thus '%zu')\n");
				fmtstr = cgi->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cgi->irb.CreateCall(fprintfn, { err, fmtstr, ind, max, cgi->irb.CreateSub(max, fir::ConstantInt::getInt64(1)) });

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

			fir::Function* refunc = cgi->getOrDeclareLibCFunc("realloc");
			iceAssert(refunc);


			fir::Value* actuallen = cgi->irb.CreateMul(nextpow2, fir::ConstantInt::getInt64(cgi->execTarget->getTypeSizeInBytes(elmtype)));
			fir::Value* newptr = cgi->irb.CreateCall2(refunc, cgi->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()), actuallen);

			cgi->irb.CreateSetDynamicArrayData(arr, cgi->irb.CreatePointerTypeCast(newptr, ptr->getType()));
			cgi->irb.CreateSetDynamicArrayCapacity(arr, nextpow2);

			cgi->irb.CreateUnCondBranch(mergeblk);
		}

		cgi->irb.setCurrentBlock(mergeblk);
	}




	fir::Function* getDynamicArrayAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
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

				fir::Value* actuallen = cgi->irb.CreateMul(applen,
					fir::ConstantInt::getInt64(cgi->execTarget->getTypeSizeInBytes(arrtype->getElementType())));

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

	fir::Function* getDynamicArrayElementAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
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










	fir::Function* getDynamicArrayCompareFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getDynamicArrayConstructFromTwoFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getDynamicArrayPopElementFromBackFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getDynamicArrayReserveSpaceForElementsFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}

	fir::Function* getDynamicArrayReserveExtraSpaceForElementsFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype)
	{
		return 0;
	}
}




















