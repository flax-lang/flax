// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

// generate runtime glue code
#define BUILTIN_RANGE_SANITY_CHECK_FUNC_NAME		"__range_sanitycheck"


namespace cgn {
namespace glue {

void printRuntimeError(cgn::CodegenState* cs, fir::Value* pos, std::string message, std::vector<fir::Value*> args)
{
	//! on windows, apparently fprintf doesn't like to work.
	//! so we just use normal printf.

	iceAssert(pos->getType()->isCharSliceType());

	#ifdef _WIN32
	{
		fir::Value* fmtstr = cs->module->createGlobalString(("\nRuntime error at %s: " + message).c_str());
		fir::Value* posstr = cs->irb.GetArraySliceData(pos);

		std::vector<fir::Value*> as = { fmtstr, posstr };
		as.insert(as.end(), args.begin(), args.end());

		cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), as);
	}
	#else
	{
		fir::Value* tmpstr = cs->module->createGlobalString("r");
		fir::Value* fmtstr = cs->module->createGlobalString(("\nRuntime error at %s: " + message).c_str());
		fir::Value* posstr = cs->irb.GetArraySliceData(pos);

		fir::Value* err = cs->irb.Call(cs->getOrDeclareLibCFunction(CRT_FDOPEN), fir::ConstantInt::getInt32(2), tmpstr);

		std::vector<fir::Value*> as = { err, fmtstr, posstr };
		as.insert(as.end(), args.begin(), args.end());

		cs->irb.Call(cs->getOrDeclareLibCFunction("fprintf"), as);
	}
	#endif


	cs->irb.Call(cs->getOrDeclareLibCFunction("abort"));
	cs->irb.Unreachable();

}

namespace misc
{
	fir::Function* getRangeSanityCheckFunction(CodegenState* cs)
	{
		fir::Function* fn = cs->module->getFunction(Identifier(BUILTIN_RANGE_SANITY_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_RANGE_SANITY_CHECK_FUNC_NAME, IdKind::Name),
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

			auto zero = fir::ConstantInt::getInt64(0);
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
					printRuntimeError(cs, func->getArguments()[1], "Range step had value of zero\n", { });
				}

				cs->irb.setCurrentBlock(stepnotpos);
				{
					printRuntimeError(cs, func->getArguments()[1], "Range had negative step value ('%ld'); invalid when start < end\n", { step });
				}

				cs->irb.setCurrentBlock(stepnotneg);
				{
					printRuntimeError(cs, func->getArguments()[1], "Range had positive step value ('%ld'); invalid when start > end\n", { step });
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

}
}
}



















