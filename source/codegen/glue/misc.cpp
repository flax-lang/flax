// misc.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"
#include "frontend.h"


namespace cgn {
namespace glue {

void printRuntimeError(cgn::CodegenState* cs, fir::Value* pos, const std::string& message, const std::vector<fir::Value*>& args)
{
	//! on windows, apparently fprintf doesn't like to work.
	//! so we just use normal printf.

	if(!frontend::getIsNoRuntimeErrorStrings())
	{
		iceAssert(pos->getType()->isCharSliceType());

		fir::Value* fmtstr = cs->module->createGlobalString(("\nRuntime error at %s:\n" + message + "\n").c_str());
		fir::Value* posstr = cs->irb.GetArraySliceData(pos);

		std::vector<fir::Value*> as = { fmtstr, posstr };
		as.insert(as.end(), args.begin(), args.end());

		cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), as);
	}

	cs->irb.Call(cs->getOrDeclareLibCFunction("abort"));
	cs->irb.Unreachable();

}

namespace misc
{
	fir::Function* getMallocWrapperFunction(CodegenState* cs)
	{
		auto fname = getMallocWrapper_FName();
		fir::Function* fn = cs->module->getFunction(fname);

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getNativeWord(), fir::Type::getCharSlice(false) }, fir::Type::getMutInt8Ptr()),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			auto sz = func->getArguments()[0];
			auto locstr = func->getArguments()[1];

			auto entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			// do the alloc.
			auto mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			auto mem = cs->irb.Call(mallocf, sz);
			auto cond = cs->irb.ICmpEQ(mem, fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));

			auto alloc_succ = cs->irb.addNewBlockAfter("success", cs->irb.getCurrentBlock());
			auto alloc_fail = cs->irb.addNewBlockAfter("failure", cs->irb.getCurrentBlock());

			cs->irb.CondBranch(cond, alloc_fail, alloc_succ);
			cs->irb.setCurrentBlock(alloc_succ);
			{
				cs->irb.Return(mem);
			}

			cs->irb.setCurrentBlock(alloc_fail);
			{
				printRuntimeError(cs, locstr, "allocation failed (returned null) (tried to allocate %d bytes)", { sz });

				// it emits an unreachable for us.
			}

			fn = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}

	fir::Function* getRangeSanityCheckFunction(CodegenState* cs)
	{
		if(frontend::getIsNoRuntimeChecks())
			return 0;

		auto fname = getRangeSanityCheck_FName();
		fir::Function* fn = cs->module->getFunction(fname);

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getRange(), fir::Type::getCharSlice(false) }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			/*
				Valid scenarios:

				1. start <= end, step > 0
				2. start >= end, step < 0
			*/
			auto entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());

			auto checkstepneg = cs->irb.addNewBlockAfter("checkstepneg", cs->irb.getCurrentBlock());
			auto checksteppos = cs->irb.addNewBlockAfter("checksteppos", cs->irb.getCurrentBlock());

			auto stepnotneg = cs->irb.addNewBlockAfter("fail_stepnotneg", cs->irb.getCurrentBlock());
			auto stepnotpos = cs->irb.addNewBlockAfter("fail_stepnotpos", cs->irb.getCurrentBlock());
			auto stepzero = cs->irb.addNewBlockAfter("fail_stepzero", cs->irb.getCurrentBlock());

			auto merge = cs->irb.addNewBlockAfter("merge", cs->irb.getCurrentBlock());


			auto lower = cs->irb.GetRangeLower(func->getArguments()[0]);
			auto upper = cs->irb.GetRangeUpper(func->getArguments()[0]);
			auto step = cs->irb.GetRangeStep(func->getArguments()[0]);

			auto zero = fir::ConstantInt::getNative(0);
			// first of all check if step is zero.
			{
				auto cond = cs->irb.ICmpEQ(step, zero);
				cs->irb.CondBranch(cond, stepzero, check);
			}


			// first, check if start <= end.
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLEQ(lower, upper);

				// if start < end, check step > 0. else check step < 0.
				cs->irb.CondBranch(cond, checksteppos, checkstepneg);
			}

			cs->irb.setCurrentBlock(checksteppos);
			{
				auto cond = cs->irb.ICmpGT(step, zero);
				cs->irb.CondBranch(cond, merge, stepnotpos);
			}

			cs->irb.setCurrentBlock(checkstepneg);
			{
				auto cond = cs->irb.ICmpLT(step, zero);
				cs->irb.CondBranch(cond, merge, stepnotneg);
			}


			// ok, now the failure messages.
			{
				cs->irb.setCurrentBlock(stepzero);
				{
					printRuntimeError(cs, func->getArguments()[1], "range step had value of zero\n", { });
				}

				cs->irb.setCurrentBlock(stepnotpos);
				{
					printRuntimeError(cs, func->getArguments()[1], "range had negative step value ('%ld'); invalid when start < end\n", { step });
				}

				cs->irb.setCurrentBlock(stepnotneg);
				{
					printRuntimeError(cs, func->getArguments()[1], "range had positive step value ('%ld'); invalid when start > end\n", { step });
				}
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();

			fn = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}



	using Idt = Identifier;
	Idt getOI(const std::string& name, fir::Type* t = 0)
	{
		if(t) return util::obfuscateIdentifier(name, t->encodedStr());
		else  return util::obfuscateIdentifier(name);
	}

	Idt getCompare_FName(fir::Type* t)              { return getOI("compare", t); }
	Idt getSetElements_FName(fir::Type* t)          { return getOI("setelements", t); }
	Idt getCallClassConstructor_FName(fir::Type* t) { return getOI("callclassinit", t); }
	Idt getSetElementsDefault_FName(fir::Type* t)   { return getOI("setelementsdefault", t); }

	Idt getClone_FName(fir::Type* t)         { return getOI("clone", t); }
	Idt getAppend_FName(fir::Type* t)        { return getOI("append", t); }
	Idt getPopBack_FName(fir::Type* t)       { return getOI("popback", t); }
	Idt getMakeFromOne_FName(fir::Type* t)   { return getOI("makefromone", t); }
	Idt getMakeFromTwo_FName(fir::Type* t)   { return getOI("makefromtwo", t); }
	Idt getReserveExtra_FName(fir::Type* t)  { return getOI("reserveextra", t); }
	Idt getAppendElement_FName(fir::Type* t) { return getOI("appendelement", t); }
	Idt getReserveEnough_FName(fir::Type* t) { return getOI("reservesufficient", t); }
	Idt getRecursiveRefcount_FName(fir::Type* t, bool incr)
	{
		return getOI(strprintf("rrc_%s", incr ? "incr" : "decr"), t);
	}

	Idt getIncrRefcount_FName(fir::Type* t)         { return getOI("incr_rc", t); }
	Idt getDecrRefcount_FName(fir::Type* t)         { return getOI("decr_rc", t); }
	Idt getLoopIncrRefcount_FName(fir::Type* t)     { return getOI("loop_incr_rc", t); }
	Idt getLoopDecrRefcount_FName(fir::Type* t)     { return getOI("loop_decr_rc", t); }

	Idt getCreateAnyOf_FName(fir::Type* t)          { return getOI("create_any_of", t); }
	Idt getGetValueFromAny_FName(fir::Type* t)      { return getOI("get_value_from_any", t); }

	Idt getUtf8Length_FName()           { return getOI("utf8_length"); }
	Idt getRangeSanityCheck_FName()     { return getOI("range_sanity"); }
	Idt getMallocWrapper_FName()        { return getOI("malloc_wrapper"); }
	Idt getBoundsCheck_FName()          { return getOI("boundscheck"); }
	Idt getDecompBoundsCheck_FName()    { return getOI("boundscheck_decomp"); }

}
}
}



















