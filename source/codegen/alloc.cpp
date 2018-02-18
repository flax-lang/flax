// alloc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

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
		auto isNeg = cs->irb.ICmpLT(s1, fir::ConstantInt::getInt64(0));
		cs->irb.CondBranch(isNeg, failb, merge);


		cs->irb.setCurrentBlock(failb);
		{
			cgn::glue::printError(cs, s2, "Tried to allocate a negative ('%ld') amount of memory\n", { s1 });
		}

		cs->irb.setCurrentBlock(merge);
		cs->irb.ReturnVoid();

		cs->irb.setCurrentBlock(restore);
		checkf = func;
	}

	iceAssert(checkf);
	return checkf;
}


static fir::Value* performAllocation(cgn::CodegenState* cs, sst::AllocOp* alloc, fir::Type* type, std::vector<sst::Expr*> counts, bool isRaw)
{
	auto mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
	iceAssert(mallocf);

	auto callSetFunction = [&cs](fir::Type* type, sst::AllocOp* alloc, fir::Value* ptr, fir::Value* count) -> void {

		if(type->isClassType())
		{
			auto constr = dcast(sst::FunctionDefn, alloc->constructor);
			iceAssert(constr);

			auto setfn = cgn::glue::array::getCallClassConstructorOnElementsFunction(cs, type->toClassType(), constr, alloc->arguments);
			iceAssert(setfn);

			cs->irb.Call(setfn, ptr, count);
		}
		else if(type->isStructType())
		{
			auto value = cs->getConstructedStructValue(type->toStructType(), alloc->arguments);

			auto setfn = cgn::glue::array::getSetElementsToValueFunction(cs, type);
			iceAssert(setfn);

			cs->irb.Call(setfn, ptr, count, value);
		}
	};



	if(counts.empty() || isRaw)
	{
		fir::Value* cnt = (counts.empty() ? fir::ConstantInt::getInt64(1)
			: cs->oneWayAutocast(counts[0]->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value);

		//* if we don't have a count, then we just return a T* -- no arrays, nothing.

		auto sz = cs->irb.Multiply(cs->irb.Sizeof(type), cnt);
		auto mem = cs->irb.Call(mallocf, sz);
		mem = cs->irb.PointerTypeCast(mem, type->getPointerTo());

		callSetFunction(type, alloc, mem, cnt);

		return mem;
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
		cs->irb.Call(checkf, count, fir::ConstantString::get(ecount->loc.toString()));

		// ok, now we have a length -- allocate enough memory for length * sizeof(elm) + refcount size
		auto alloclen = cs->irb.Add(cs->irb.Multiply(count, cs->irb.Sizeof(type)), fir::ConstantInt::getInt64(REFCOUNT_SIZE));
		auto mem = cs->irb.PointerAdd(cs->irb.Call(mallocf, alloclen), fir::ConstantInt::getInt64(REFCOUNT_SIZE));
		mem = cs->irb.PointerTypeCast(mem, type->getPointerTo());

		// make them valid things
		callSetFunction(type, alloc, mem, count);

		// ok, now return the array we created.
		{
			auto ret = cs->irb.CreateValue(fir::DynamicArrayType::get(type));
			ret = cs->irb.SetDynamicArrayData(ret, mem);
			ret = cs->irb.SetDynamicArrayLength(ret, count);
			ret = cs->irb.SetDynamicArrayCapacity(ret, count);

			cs->irb.SetDynamicArrayRefCount(ret, fir::ConstantInt::getInt64(1));

			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Value* tmpstr = cs->module->createGlobalString("alloc new array: (ptr: %p, len: %ld, cap: %ld)\n");
				cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, mem, count, count });
			}
			#endif

			return ret;
		}
	}
}












CGResult sst::AllocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->counts.size() > 1)
		error(this, "Multi-dimensional arrays are not supported yet.");

	auto result = performAllocation(cs, this, this->elmType, this->counts, this->isRaw);
	return CGResult(result, 0, CGResult::VK::LitRValue);
}



CGResult sst::DeallocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());


	auto freef = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
	iceAssert(freef);

	auto value = this->expr->codegen(cs).value;
	auto ty = value->getType();

	iceAssert(ty->isPointerType());

	fir::Value* ptr = cs->irb.PointerTypeCast(value, fir::Type::getInt8Ptr());

	cs->irb.Call(freef, ptr);
	return CGResult(0);
}
























