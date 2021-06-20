// alloc.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

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
				cs->performAssignment(cs->irb.Dereference(ptr), value, true);
			}
			else if(type->isStructType())
			{
				auto value = cs->getConstructedStructValue(type->toStructType(), alloc->arguments);
				cs->performAssignment(cs->irb.Dereference(ptr), value, true);
			}
			else
			{
				auto value = cs->getDefaultValue(type);
				cs->performAssignment(cs->irb.Dereference(ptr), value, true);
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
		auto mem = cs->irb.Call(cgn::glue::misc::getMallocWrapperFunction(cs), sz, fir::ConstantCharSlice::get(alloc->loc.shortString()));
		mem = cs->irb.PointerTypeCast(mem, type->getMutablePointerTo());

		callSetFunction(type, alloc, mem, cnt);

		// check if we were supposed to be immutable
		if(!alloc->isMutable)
			mem = cs->irb.PointerTypeCast(mem, mem->getType()->getImmutablePointerVersion());

		return mem;
	}
	else
	{
		iceAssert(false);
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
























