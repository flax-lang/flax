// arrays.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"


namespace cgn {
namespace glue {
namespace array
{
	static void _compareFunctionUsingBuiltinCompare(CodegenState* cs, fir::Type* arrtype, fir::Function* func,
		fir::Value* arg1, fir::Value* arg2)
	{
		// ok, ez.
		fir::Value* zeroval = fir::ConstantInt::getNative(0);
		fir::Value* oneval = fir::ConstantInt::getNative(1);

		fir::IRBlock* cond = cs->irb.addNewBlockInFunction("cond", func);
		fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
		fir::IRBlock* incr = cs->irb.addNewBlockInFunction("incr", func);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

		fir::Value* ptr1 = 0; fir::Value* ptr2 = 0;

		if(arrtype->isArraySliceType())
		{
			ptr1 = cs->irb.GetArraySliceData(arg1);
			ptr2 = cs->irb.GetArraySliceData(arg2);
		}
		else if(arrtype->isArrayType())
		{
			ptr1 = cs->irb.ConstGEP2(arg1, 0, 0);
			ptr2 = cs->irb.ConstGEP2(arg2, 0, 0);
		}
		else
		{
			error("invalid type '%s'", arrtype);
		}

		fir::Value* len1 = 0; fir::Value* len2 = 0;

		if(arrtype->isArraySliceType())
		{
			len1 = cs->irb.GetArraySliceLength(arg1);
			len2 = cs->irb.GetArraySliceLength(arg2);
		}
		else if(arrtype->isArrayType())
		{
			len1 = fir::ConstantInt::getNative(arrtype->toArrayType()->getArraySize());
			len2 = fir::ConstantInt::getNative(arrtype->toArrayType()->getArraySize());
		}
		else
		{
			error("invalid type '%s'", arrtype);
		}

		// we compare to this to break
		fir::Value* counter = cs->irb.StackAlloc(fir::Type::getNativeWord());
		cs->irb.WritePtr(zeroval, counter);

		fir::Value* res = cs->irb.StackAlloc(fir::Type::getNativeWord());
		cs->irb.WritePtr(zeroval, res);


		cs->irb.UnCondBranch(cond);
		cs->irb.setCurrentBlock(cond);
		{
			fir::IRBlock* retlt = cs->irb.addNewBlockInFunction("retlt", func);
			fir::IRBlock* reteq = cs->irb.addNewBlockInFunction("reteq", func);
			fir::IRBlock* retgt = cs->irb.addNewBlockInFunction("retgt", func);

			fir::IRBlock* tmp1 = cs->irb.addNewBlockInFunction("tmp1", func);
			fir::IRBlock* tmp2 = cs->irb.addNewBlockInFunction("tmp2", func);

			// if we got here, the arrays were equal *up to this point*
			// if ptr1 exceeds or ptr2 exceeds, return len1 - len2

			fir::Value* t1 = cs->irb.ICmpEQ(cs->irb.ReadPtr(counter), len1);
			fir::Value* t2 = cs->irb.ICmpEQ(cs->irb.ReadPtr(counter), len2);

			// if t1 is over, goto tmp1, if not goto t2
			cs->irb.CondBranch(t1, tmp1, tmp2);
			cs->irb.setCurrentBlock(tmp1);
			{
				// t1 is over
				// check if t2 is over
				// if so, return 0 (b == a)
				// if not, return -1 (b > a)

				cs->irb.CondBranch(t2, reteq, retlt);
			}

			cs->irb.setCurrentBlock(tmp2);
			{
				// t1 is not over
				// check if t2 is over
				// if so, return 1 (a > b)
				// if not, goto body

				cs->irb.CondBranch(t2, retgt, body);
			}


			cs->irb.setCurrentBlock(retlt);
			cs->irb.Return(fir::ConstantInt::getNative(-1));

			cs->irb.setCurrentBlock(reteq);
			cs->irb.Return(fir::ConstantInt::getNative(0));

			cs->irb.setCurrentBlock(retgt);
			cs->irb.Return(fir::ConstantInt::getNative(+1));
		}


		cs->irb.setCurrentBlock(body);
		{
			fir::Value* v1 = cs->irb.ReadPtr(cs->irb.GetPointer(ptr1, cs->irb.ReadPtr(counter)));
			fir::Value* v2 = cs->irb.ReadPtr(cs->irb.GetPointer(ptr2, cs->irb.ReadPtr(counter)));

			fir::Value* c = cs->performBinaryOperation(cs->loc(), { cs->loc(), v1 }, { cs->loc(), v2 }, "==").value;

			// c is a bool, because it's very generic in nature
			// so we just take !c and convert to i64 to get our result.
			// if c == true, then lhs == rhs, and so we should have 0.

			c = cs->irb.LogicalNot(c);
			c = cs->irb.IntSizeCast(c, fir::Type::getNativeWord());

			cs->irb.WritePtr(c, res);

			// compare to 0.
			fir::Value* cmpres = cs->irb.ICmpEQ(cs->irb.ReadPtr(res), zeroval);

			// if equal, go to incr, if not return directly
			cs->irb.CondBranch(cmpres, incr, merge);
		}


		cs->irb.setCurrentBlock(incr);
		{
			cs->irb.WritePtr(cs->irb.Add(cs->irb.ReadPtr(counter), oneval), counter);
			cs->irb.UnCondBranch(cond);
		}



		cs->irb.setCurrentBlock(merge);
		{
			// load and return
			cs->irb.Return(cs->irb.ReadPtr(res));
		}
	}


	static void _compareFunctionUsingOperatorFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* curfunc,
		fir::Value* arg1, fir::Value* arg2, fir::Function* opf)
	{
		error("notsup");
	}

	fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf)
	{
		iceAssert(arrtype);

		auto fname = misc::getCompare_FName(arrtype);
		fir::Function* cmpf = cs->module->getFunction(fname);

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ arrtype, arrtype }, fir::Type::getNativeWord()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			{
				// check our situation.
				if(opf == 0)
				{
					_compareFunctionUsingBuiltinCompare(cs, arrtype, func, s1, s2);
				}
				else
				{
					_compareFunctionUsingOperatorFunction(cs, arrtype, func, s1, s2, opf);
				}

				// functions above do their own return
			}


			cs->irb.setCurrentBlock(restore);
			cmpf = func;
		}

		iceAssert(cmpf);
		return cmpf;
	}
}
}
}

