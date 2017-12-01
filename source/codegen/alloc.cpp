// alloc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::AllocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());

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
			allocsz = cs->irb.CreateAdd(allocsz, fir::ConstantInt::getInt64(8));

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

		return CGResult(arr, 0, CGResult::VK::LitRValue);
	}
}



CGResult sst::DeallocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());


	auto freef = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
	iceAssert(freef);

	auto value = this->expr->codegen(cs).value;
	auto ty = value->getType();

	fir::Value* ptr = 0;
	if(ty->isPointerType())
	{
		// good, just free this nonsensical thing.
		ptr = cs->irb.CreatePointerTypeCast(value, fir::Type::getInt8Ptr());
	}
	else if(ty->isDynamicArrayType())
	{
		ptr = cs->irb.CreateGetDynamicArrayData(value);

		// do the refcount
		ptr = cs->irb.CreatePointerSub(cs->irb.CreatePointerTypeCast(ptr, fir::Type::getInt64Ptr()), fir::ConstantInt::getInt64(1));

		ptr = cs->irb.CreatePointerTypeCast(value, fir::Type::getInt8Ptr());
	}
	else
	{
		error(this->expr, "Unsupported type '%s' for deallocation", ty);
	}

	cs->irb.CreateCall1(freef, ptr);
	return CGResult(0);
}
























