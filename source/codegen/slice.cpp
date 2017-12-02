// slice.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"

#define dcast(t, v)		dynamic_cast<t*>(v)


static void _complainAboutSliceIndices(cgn::CodegenState* cs, std::string fmt, fir::Value* complaintValue, Location loc)
{
	#if 1
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

		fir::ConstantValue* tmpstr = cs->module->createGlobalString("w");
		fir::ConstantValue* fmtstr = cs->module->createGlobalString(fmt);

		iceAssert(fmtstr);

		auto pstr = fir::ConstantString::get(loc.toString());
		fir::Value* posstr = cs->irb.CreateGetStringData(pstr);
		fir::Value* err = cs->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

		cs->irb.CreateCall(fprintfn, { err, fmtstr, posstr, complaintValue });
	}
	#else
	{
		fir::ConstantValue* fmtstr = cs->module->createGlobalString(fmt);

		auto pstr = fir::ConstantString::get(loc.toString());
		fir::Value* posstr = cs->irb.CreateGetStringData(pstr);

		cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { fmtstr, posstr, complaintValue });
	}
	#endif

	cs->irb.CreateCall0(cs->getOrDeclareLibCFunction("abort"));
	cs->irb.CreateUnreachable();
}


static void checkSliceOperation(cgn::CodegenState* cs, sst::Expr* user, fir::Value* maxlen, fir::Value* beginIndex, fir::Value* endIndex,
	sst::Expr* bexpr, sst::Expr* eexpr)
{
	Location apos = (bexpr ? bexpr->loc : user->loc);
	Location bpos = (eexpr ? eexpr->loc : user->loc);

	iceAssert(beginIndex);
	iceAssert(endIndex);

	if(!beginIndex->getType()->isIntegerType())
		error(bexpr, "Expected integer type for array slice; got '%s'", beginIndex->getType());

	if(!endIndex->getType()->isIntegerType())
		error(eexpr, "Expected integer type for array slice; got '%s'", endIndex->getType());


	fir::Value* length = cs->irb.CreateSub(endIndex, beginIndex);

	// do a check
	auto neg_begin = cs->irb.addNewBlockInFunction("neg_begin", cs->irb.getCurrentFunction());
	auto neg_end = cs->irb.addNewBlockInFunction("neg_end", cs->irb.getCurrentFunction());
	auto neg_len = cs->irb.addNewBlockInFunction("neg_len", cs->irb.getCurrentFunction());
	auto check1 = cs->irb.addNewBlockInFunction("check1", cs->irb.getCurrentFunction());
	auto check2 = cs->irb.addNewBlockInFunction("check2", cs->irb.getCurrentFunction());
	auto merge = cs->irb.addNewBlockInFunction("merge", cs->irb.getCurrentFunction());

	{
		fir::Value* neg = cs->irb.CreateICmpLT(beginIndex, fir::ConstantInt::getInt64(0));
		cs->irb.CreateCondBranch(neg, neg_begin, check1);
	}

	cs->irb.setCurrentBlock(check1);
	{
		fir::Value* neg = cs->irb.CreateICmpLT(endIndex, fir::ConstantInt::getInt64(0));
		cs->irb.CreateCondBranch(neg, neg_end, check2);
	}

	cs->irb.setCurrentBlock(check2);
	{
		fir::Value* neg = cs->irb.CreateICmpLT(length, fir::ConstantInt::getInt64(0));
		cs->irb.CreateCondBranch(neg, neg_len, merge);
	}


	cs->irb.setCurrentBlock(neg_begin);
	_complainAboutSliceIndices(cs, "%s: Start index for array slice was negative (%zd)\n", beginIndex, apos);

	cs->irb.setCurrentBlock(neg_end);
	_complainAboutSliceIndices(cs, "%s: Ending index for array slice was negative (%zd)\n", endIndex, bpos);

	cs->irb.setCurrentBlock(neg_len);
	_complainAboutSliceIndices(cs, "%s: Length for array slice was negative (%zd)\n", length, bpos);


	cs->irb.setCurrentBlock(merge);

	// bounds check.
	{
		// endindex is non-inclusive, so do the len vs len check
		fir::Function* checkf = cgn::glue::array::getBoundsCheckFunction(cs, true);
		iceAssert(checkf);

		cs->irb.CreateCall3(checkf, maxlen, endIndex, fir::ConstantString::get(apos.toString()));
	}
}




static CGResult performSliceOperation(cgn::CodegenState* cs, sst::Expr* user, fir::Type* elmType, fir::Value* data, fir::Value* maxlen,
	fir::Value* beginIndex, fir::Value* endIndex, sst::Expr* bexpr, sst::Expr* eexpr)
{
	checkSliceOperation(cs, user, maxlen, beginIndex, endIndex, bexpr, eexpr);

	// ok, make the slice
	fir::Type* slct = fir::ArraySliceType::get(elmType);
	fir::Value* slice = cs->irb.CreateValue(slct, "slice");

	// FINALLY.
	// increment ptr
	fir::Value* newptr = cs->irb.CreatePointerAdd(data, beginIndex, "newptr");
	fir::Value* newlen = cs->irb.CreateSub(endIndex, beginIndex, "newlen");

	slice = cs->irb.CreateSetArraySliceData(slice, newptr);
	slice = cs->irb.CreateSetArraySliceLength(slice, newlen);

	if(cs->isRefCountedType(elmType))
	{
		// increment the refcounts for the strings
		fir::Function* incrfn = cgn::glue::array::getIncrementArrayRefCountFunction(cs, fir::DynamicArrayType::get(elmType));
		iceAssert(incrfn);

		cs->irb.CreateCall2(incrfn, newptr, newlen);
	}

	// slices are rvalues
	return CGResult(slice);
}





CGResult sst::SliceOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto ty = this->expr->type;
	auto res = this->expr->codegen(cs);

	this->cgSubscripteePtr = res.pointer;
	this->cgSubscriptee = res.value;

	auto lhs = res.value;
	// auto lhsptr = res.pointer;

	iceAssert(ty == lhs->getType());

	fir::Value* length = 0;
	if(ty->isDynamicArrayType())	length = cs->irb.CreateGetDynamicArrayLength(lhs, "orig_len");
	else if(ty->isArraySliceType())	length = cs->irb.CreateGetArraySliceLength(lhs, "orig_len");
	else if(ty->isStringType())		length = cs->irb.CreateGetStringLength(lhs, "orig_len");
	else if(ty->isArrayType())		length = fir::ConstantInt::getInt64(ty->toArrayType()->getArraySize());
	else							error(this, "unsupported type '%s'", ty);

	iceAssert(length);
	{
		if(this->begin)	this->cgBegin = this->begin->codegen(cs).value;
		else			this->cgBegin = fir::ConstantInt::getInt64(0);

		if(this->end)	this->cgEnd = this->end->codegen(cs).value;
		else			this->cgEnd = length;

		this->cgBegin = cs->oneWayAutocast(CGResult(this->cgBegin), fir::Type::getInt64()).value;
		this->cgEnd = cs->oneWayAutocast(CGResult(this->cgEnd), fir::Type::getInt64()).value;
	}

	this->cgBegin->setName("begin");
	this->cgEnd->setName("end");

	if(ty->isDynamicArrayType())
	{
		// make that shit happen

		return performSliceOperation(cs, this, ty->getArrayElementType(), cs->irb.CreateGetDynamicArrayData(lhs),
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isArrayType())
	{
		auto lhsptr = res.pointer;

		if(!lhsptr) lhsptr = cs->irb.CreateImmutStackAlloc(lhs->getType(), lhs);
		iceAssert(lhsptr);

		fir::Value* data = cs->irb.CreateConstGEP2(lhsptr, 0, 0);

		return performSliceOperation(cs, this, ty->getArrayElementType(), data,
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isArraySliceType())
	{
		return performSliceOperation(cs, this, ty->getArrayElementType(), cs->irb.CreateGetArraySliceData(lhs),
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isStringType())
	{
		// do it manually, since we want to get a string instead of char[]
		// and also we don't want to be stupid, so slices make copies!!
		// todo: might want to change
		checkSliceOperation(cs, this, length, this->cgBegin, this->cgEnd, this->begin, this->end);


		// ok
		fir::Value* srcptr = cs->irb.CreatePointerAdd(cs->irb.CreateGetStringData(lhs), this->cgBegin);
		fir::Value* newlen = cs->irb.CreateSub(this->cgEnd, this->cgBegin);

		fir::Value* data = 0;
		{
			// space for null + refcount
			size_t i64Size = 8;
			fir::Value* malloclen = cs->irb.CreateAdd(newlen, fir::ConstantInt::getInt64(1 + i64Size));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			data = cs->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));


			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.CreateCall(memcpyf, { data, srcptr, cs->irb.CreateIntSizeCast(newlen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			// null terminator
			cs->irb.CreateStore(fir::ConstantInt::getInt8(0), cs->irb.CreatePointerAdd(data, newlen));
		}

		// ok, now fix it
		fir::Value* str = cs->irb.CreateValue(fir::StringType::get());
		str = cs->irb.CreateSetStringData(str, data);
		str = cs->irb.CreateSetStringLength(str, newlen);

		cs->irb.CreateSetStringRefCount(str, fir::ConstantInt::getInt64(1));

		cs->addRefCountedValue(str);
		return CGResult(str);
	}
	else
	{
		error(this, "Cannot slice unsupported type '%s'", ty);
	}
}


