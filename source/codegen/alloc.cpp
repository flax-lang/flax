// alloc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

#define BUILTIN_ALLOC_CHECK_NEGATIVE_LENGTH_NAME	"__alloc_checkneg"


static fir::Function* getCheckNegativeLengthFunction(cgn::CodegenState* cs)
{
	fir::Function* checkf = cs->module->getFunction(Identifier(BUILTIN_ALLOC_CHECK_NEGATIVE_LENGTH_NAME, IdKind::Name));

	if(!checkf)
	{
		auto restore = cs->irb.getCurrentBlock();

		fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_ALLOC_CHECK_NEGATIVE_LENGTH_NAME, IdKind::Name),
			fir::FunctionType::get({ fir::Type::getInt64(), fir::Type::getString() }, fir::Type::getVoid()), fir::LinkageType::Internal);

		func->setAlwaysInline();

		fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
		cs->irb.setCurrentBlock(entry);

		fir::Value* s1 = func->getArguments()[0];
		fir::Value* s2 = func->getArguments()[1];

		fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", func);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

		// make the thing
		auto isNeg = cs->irb.CreateICmpLT(s1, fir::ConstantInt::getInt64(0));
		cs->irb.CreateCondBranch(isNeg, failb, merge);


		cs->irb.setCurrentBlock(failb);
		{
			fir::Function* fprintfn = cs->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
				fir::Type::getInt32()), fir::LinkageType::External);

			fir::Function* fdopenf = cs->module->getOrCreateFunction(Identifier(CRT_FDOPEN, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()),
				fir::LinkageType::External);

			// basically:
			// void* stderr = fdopen(2, "w")
			// fprintf(stderr, "", bla bla)

			fir::Value* tmpstr = cs->module->createGlobalString("w");

			fir::Value* fmtstr = cs->module->createGlobalString("%s: Tried to allocate memory with a negative (%d) count\n");
			fir::Value* posstr = cs->irb.CreateGetStringData(s2);

			fir::Value* err = cs->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

			cs->irb.CreateCall(fprintfn, { err, fmtstr, posstr, s1 });

			cs->irb.CreateCall0(cs->getOrDeclareLibCFunction("abort"));
			cs->irb.CreateUnreachable();
		}

		cs->irb.setCurrentBlock(merge);
		cs->irb.CreateReturnVoid();

		cs->irb.setCurrentBlock(restore);
		checkf = func;
	}

	iceAssert(checkf);
	return checkf;
}


static fir::Value* performAllocation(cgn::CodegenState* cs, fir::Type* type, std::vector<sst::Expr*> counts, bool isRaw)
{
	auto mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
	iceAssert(mallocf);

	if(counts.empty() || isRaw)
	{
		fir::Value* cnt = (counts.empty() ? fir::ConstantInt::getInt64(1)
			: cs->oneWayAutocast(counts[0]->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value);

		//* if we don't have a count, then we just return a T* -- no arrays, nothing.

		auto sz = cs->irb.CreateMul(cs->irb.CreateSizeof(type), cnt);
		auto mem = cs->irb.CreateCall1(mallocf, sz);

		return cs->irb.CreatePointerTypeCast(mem, type->getPointerTo());
	}
	else
	{
		auto ecount = counts[0];

		auto count = cs->oneWayAutocast(ecount->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value;
		if(!count || !count->getType()->isIntegerType())
			error(ecount, "Expected integer type for length, found '%s' instead", (count ? count->getType()->str() : "null"));

		// make sure the length isn't negative
		auto checkf = getCheckNegativeLengthFunction(cs);
		iceAssert(checkf);
		cs->irb.CreateCall2(checkf, count, fir::ConstantString::get(ecount->loc.toString()));

		// ok, now we have a length -- allocate enough memory for length * sizeof(elm) + refcount size
		auto alloclen = cs->irb.CreateAdd(cs->irb.CreateMul(count, cs->irb.CreateSizeof(type)), fir::ConstantInt::getInt64(REFCOUNT_SIZE));
		auto mem = cs->irb.CreatePointerAdd(cs->irb.CreateCall1(mallocf, alloclen), fir::ConstantInt::getInt64(REFCOUNT_SIZE));
		mem = cs->irb.CreatePointerTypeCast(mem, type->getPointerTo());


		// make them valid things
		auto setf = cgn::glue::array::getSetElementsToDefaultValueFunction(cs, type);
		iceAssert(setf);

		cs->irb.CreateCall2(setf, mem, count);

		// ok, now return the array we created.
		{
			auto ret = cs->irb.CreateValue(fir::DynamicArrayType::get(type));
			ret = cs->irb.CreateSetDynamicArrayData(ret, mem);
			ret = cs->irb.CreateSetDynamicArrayLength(ret, count);
			ret = cs->irb.CreateSetDynamicArrayCapacity(ret, count);

			cs->irb.CreateSetDynamicArrayRefCount(ret, fir::ConstantInt::getInt64(1));

			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Value* tmpstr = cs->module->createGlobalString("alloc new array: (ptr: %p, len: %ld, cap: %ld)\n");
				cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { tmpstr, mem, count, count });
			}
			#endif

			return ret;
		}
	}
}












CGResult sst::AllocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());

	if(this->counts.size() > 1)
		error(this, "Multi-dimensional arrays are not supported yet.");

	auto result = performAllocation(cs, this->elmType, this->counts, this->isRaw);
	return CGResult(result, 0, CGResult::VK::LitRValue);

	#if 0

	auto cntres = (this->count ?
		this->count->codegen(cs, fir::Type::getInt64()) : CGResult(fir::ConstantInt::getInt64(1)));

	cntres = cs->oneWayAutocast(cntres, fir::Type::getInt64());
	auto cntval = cntres.value;

	if(!cntval->getType()->isIntegerType())
		error(this->count, "Expected integer type in count for alloc, found '%s' instead", cntval->getType()->str());



	// ok, we do a runtime check for the value here.
	if(this->count)
	{
		auto fn = cs->irb.getCurrentFunction();
		fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", fn);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", fn);

		// make the thing
		auto isNeg = cs->irb.CreateICmpLT(cntval, fir::ConstantInt::getInt64(0));
		cs->irb.CreateCondBranch(isNeg, failb, merge);


		cs->irb.setCurrentBlock(failb);
		{
			fir::Function* fprintfn = cs->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
				fir::Type::getInt32()), fir::LinkageType::External);

			fir::Function* fdopenf = cs->module->getOrCreateFunction(Identifier(CRT_FDOPEN, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()),
				fir::LinkageType::External);

			// basically:
			// void* stderr = fdopen(2, "w")
			// fprintf(stderr, "", bla bla)

			fir::Value* tmpstr = cs->module->createGlobalString("w");

			fir::Value* fmtstr = cs->module->createGlobalString("%s: Tried to allocate memory with a negative (%d) count\n");
			fir::Value* posstr = cs->irb.CreateGetStringData(fir::ConstantString::get(this->loc.toString()));

			fir::Value* err = cs->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

			cs->irb.CreateCall(fprintfn, { err, fmtstr, posstr, cntval });

			cs->irb.CreateCall0(cs->getOrDeclareLibCFunction("abort"));
			cs->irb.CreateUnreachable();
		}

		cs->irb.setCurrentBlock(merge);
	}


	fir::Value* dataptr = 0;
	{
		auto mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
		iceAssert(mallocf);

		auto allocsz = cs->irb.CreateMul(cntval, cs->irb.CreateSizeof(elmType));
		if(!this->isRaw)
			allocsz = cs->irb.CreateAdd(allocsz, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

		auto rawdata = cs->irb.CreateCall1(mallocf, allocsz);


		if(!this->isRaw)
		{
			dataptr = cs->irb.CreatePointerAdd(cs->irb.CreatePointerTypeCast(rawdata, fir::Type::getInt64Ptr()), fir::ConstantInt::getInt64(1));
			dataptr = cs->irb.CreatePointerTypeCast(dataptr, elmType->getPointerTo());
		}
		else
		{
			dataptr = cs->irb.CreatePointerTypeCast(rawdata, elmType->getPointerTo());
		}

		auto setfn = cgn::glue::array::getSetElementsToDefaultValueFunction(cs, elmType);
		cs->irb.CreateCall2(setfn, dataptr, cntval);
	}
	iceAssert(dataptr);

	// ok, resume
	if(this->isRaw)
	{
		return CGResult(dataptr);
	}
	else
	{
		// use the same pointer but wrap it in a dynamic array, I guess.
		iceAssert(this->type->isDynamicArrayType());
		auto arr = cs->irb.CreateValue(this->type);

		dataptr = cs->irb.CreatePointerTypeCast(dataptr, elmType->getPointerTo());

		arr = cs->irb.CreateSetDynamicArrayData(arr, dataptr);
		arr = cs->irb.CreateSetDynamicArrayLength(arr, cntval);
		arr = cs->irb.CreateSetDynamicArrayCapacity(arr, cntval);
		cs->irb.CreateSetDynamicArrayRefCount(arr, fir::ConstantInt::getInt64(1));


		#if DEBUG_ARRAY_ALLOCATION
		{
			fir::Value* tmpstr = cs->module->createGlobalString("alloc new array: (ptr: %p, len: %ld, cap: %ld)\n");
			cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { tmpstr, dataptr, cntval, cntval });
		}
		#endif


		return CGResult(arr, 0, CGResult::VK::LitRValue);
	}

	#endif
}



CGResult sst::DeallocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());


	auto freef = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
	iceAssert(freef);

	auto value = this->expr->codegen(cs).value;
	auto ty = value->getType();

	iceAssert(ty->isPointerType());

	fir::Value* ptr = cs->irb.CreatePointerTypeCast(value, fir::Type::getInt8Ptr());

	cs->irb.CreateCall1(freef, ptr);
	return CGResult(0);
}
























