// alloc.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

static fir::Function* getCheckNegativeLengthFunction(cgn::CodegenState* cs)
{
	auto fname = util::obfuscateIdentifier("alloc_checkneg");
	fir::Function* checkf = cs->module->getFunction(fname);

	if(!checkf)
	{
		auto restore = cs->irb.getCurrentBlock();

		fir::Function* func = cs->module->getOrCreateFunction(fname,
			fir::FunctionType::get({ fir::Type::getNativeWord(), fir::Type::getCharSlice(false) }, fir::Type::getVoid()),
			fir::LinkageType::Internal);

		func->setAlwaysInline();

		fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
		cs->irb.setCurrentBlock(entry);

		fir::Value* s1 = func->getArguments()[0];
		fir::Value* s2 = func->getArguments()[1];

		fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", func);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

		// make the thing
		auto isNeg = cs->irb.ICmpLT(s1, fir::ConstantInt::getNative(0));
		cs->irb.CondBranch(isNeg, failb, merge);


		cs->irb.setCurrentBlock(failb);
		{
			cgn::glue::printRuntimeError(cs, s2, "tried to allocate a negative ('%ld') amount of memory\n", { s1 });
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
	auto callSetFunction = [cs](fir::Type* type, sst::AllocOp* alloc, fir::Value* ptr, fir::Value* count) -> void {

		auto callUserCode = [cs, alloc](fir::Value* elmp, fir::Value* idxp) {
			iceAssert(alloc->initBlockIdx);
			iceAssert(alloc->initBlockVar);

			// ok, then. create the variables:
			cs->addVariableUsingStorage(alloc->initBlockIdx, idxp);
			cs->addVariableUsingStorage(alloc->initBlockVar, elmp);

			alloc->initBlock->codegen(cs);
		};




		auto arrp = ptr;
		auto ctrp = cs->irb.CreateLValue(fir::Type::getNativeWord());

		auto actuallyStore = [cs, type, alloc](fir::Value* ptr) -> void {

			if(type->isClassType())
			{
				auto constr = dcast(sst::FunctionDefn, alloc->constructor);
				iceAssert(constr);

				//! here, the arguments are called once per element.
				auto value = cs->constructClassWithArguments(type->toClassType(), constr, alloc->arguments);
				cs->autoAssignRefCountedValue(cs->irb.Dereference(ptr), value, true);
			}
			else if(type->isStructType())
			{
				auto value = cs->getConstructedStructValue(type->toStructType(), alloc->arguments);
				cs->autoAssignRefCountedValue(cs->irb.Dereference(ptr), value, true);
			}
			else
			{
				auto value = cs->getDefaultValue(type);
				cs->autoAssignRefCountedValue(cs->irb.Dereference(ptr), value, true);
			}
		};


		if(alloc->counts.empty())
		{
			actuallyStore(arrp);
		}
		else
		{
			cs->createWhileLoop([cs, ctrp, count](auto pass, auto fail) {
				auto cond = cs->irb.ICmpLT(ctrp, count);
				cs->irb.CondBranch(cond, pass, fail);
			},
			[cs, callUserCode, actuallyStore, alloc, ctrp, arrp]() {

				auto ptr = cs->irb.GetPointer(arrp, ctrp);

				actuallyStore(ptr);

				if(alloc->initBlock)
					callUserCode(cs->irb.Dereference(ptr), ctrp);

				cs->irb.Store(cs->irb.Add(ctrp, fir::ConstantInt::getNative(1)), ctrp);
			});
		}
	};



	if(counts.empty() || isRaw)
	{
		fir::Value* cnt = (counts.empty() ? fir::ConstantInt::getNative(1)
			: cs->oneWayAutocast(counts[0]->codegen(cs, fir::Type::getNativeWord()).value, fir::Type::getNativeWord()));

		//* if we don't have a count, then we just return a T* -- no arrays, nothing.

		auto sz = cs->irb.Multiply(cs->irb.Sizeof(type), cnt);
		auto mem = cs->irb.Call(cgn::glue::misc::getMallocWrapperFunction(cs), sz, fir::ConstantString::get(alloc->loc.shortString()));
		mem = cs->irb.PointerTypeCast(mem, type->getMutablePointerTo());

		callSetFunction(type, alloc, mem, cnt);

		// check if we were supposed to be immutable
		if(!alloc->isMutable)
			mem = cs->irb.PointerTypeCast(mem, mem->getType()->getImmutablePointerVersion());

		return mem;
	}
	else
	{
		auto ecount = counts[0];

		auto count = cs->oneWayAutocast(ecount->codegen(cs, fir::Type::getNativeWord()).value, fir::Type::getNativeWord());
		if(!count || !count->getType()->isIntegerType())
			error(ecount, "expected integer type for length, found '%s' instead", (count ? count->getType()->str() : "null"));

		// make sure the length isn't negative
		auto checkf = getCheckNegativeLengthFunction(cs);
		iceAssert(checkf);
		cs->irb.Call(checkf, count, fir::ConstantString::get(ecount->loc.toString()));

		auto arr = cs->irb.CreateValue(fir::DynamicArrayType::get(type));
		auto expandfn = cgn::glue::saa_common::generateReserveAtLeastFunction(cs, arr->getType());
		iceAssert(expandfn);

		arr = cs->irb.Call(expandfn, arr, count);
		arr = cs->irb.SetSAALength(arr, count);

		callSetFunction(type, alloc, cs->irb.GetSAAData(arr), count);
		cs->addRefCountedValue(arr);

		return arr;
	}
}












CGResult sst::AllocOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->counts.size() > 1)
		error(this, "multi-dimensional arrays are not supported yet.");

	return CGResult(performAllocation(cs, this, this->elmType, this->counts, this->attrs.has(attr::RAW)));
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

	fir::Value* ptr = cs->irb.PointerTypeCast(value, fir::Type::getMutInt8Ptr());

	cs->irb.Call(freef, ptr);
	return CGResult(0);
}
























