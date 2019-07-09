// strings.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

// generate runtime glue code

namespace cgn {
namespace glue {
namespace string
{
	fir::Function* getCloneFunction(CodegenState* cs)
	{
		return saa_common::generateCloneFunction(cs, fir::Type::getString());
	}

	fir::Function* getAppendFunction(CodegenState* cs)
	{
		return saa_common::generateAppendFunction(cs, fir::Type::getString());
	}

	fir::Function* getCharAppendFunction(CodegenState* cs)
	{
		return saa_common::generateElementAppendFunction(cs, fir::Type::getString());
	}

	fir::Function* getConstructFromTwoFunction(CodegenState* cs)
	{
		return saa_common::generateConstructFromTwoFunction(cs, fir::Type::getString());
	}

	fir::Function* getConstructWithCharFunction(CodegenState* cs)
	{
		return saa_common::generateConstructWithElementFunction(cs, fir::Type::getString());
	}

	fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isDecomp)
	{
		return saa_common::generateBoundsCheckFunction(cs, /* isString: */ true, isDecomp);
	}

	fir::Function* getCompareFunction(CodegenState* cs)
	{
		auto fname = misc::getCompare_FName(fir::Type::getString());
		fir::Function* cmpf = cs->module->getFunction(fname);

		if(!cmpf)
		{
			// great.
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getString(), fir::Type::getString() },
				fir::Type::getNativeWord()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			/*
				int strcmp(const char* s1, const char* s2)
				{
					while(*s1 && (*s1 == *s2))
						s1++, s2++;

					return *(const unsigned char*) s1 - *(const unsigned char*) s2;
				}
			*/

			{
				fir::Value* str1p = cs->irb.StackAlloc(fir::Type::getMutInt8Ptr());
				cs->irb.WritePtr(cs->irb.GetSAAData(s1, "s1"), str1p);

				fir::Value* str2p = cs->irb.StackAlloc(fir::Type::getMutInt8Ptr());
				cs->irb.WritePtr(cs->irb.GetSAAData(s2, "s2"), str2p);


				fir::IRBlock* loopcond = cs->irb.addNewBlockInFunction("cond1", func);
				fir::IRBlock* loopincr = cs->irb.addNewBlockInFunction("loopincr", func);
				fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

				cs->irb.UnCondBranch(loopcond);
				cs->irb.setCurrentBlock(loopcond);
				{
					fir::IRBlock* cond2 = cs->irb.addNewBlockInFunction("cond2", func);

					fir::Value* str1 = cs->irb.ReadPtr(str1p);
					fir::Value* str2 = cs->irb.ReadPtr(str2p);

					// make sure ptr1 is not null
					fir::Value* cnd = cs->irb.ICmpNEQ(cs->irb.ReadPtr(str1), fir::ConstantInt::getInt8(0));
					cs->irb.CondBranch(cnd, cond2, merge);

					cs->irb.setCurrentBlock(cond2);
					{
						// check that they are equal
						fir::Value* iseq = cs->irb.ICmpEQ(cs->irb.ReadPtr(str1), cs->irb.ReadPtr(str2));
						cs->irb.CondBranch(iseq, loopincr, merge);
					}


					cs->irb.setCurrentBlock(loopincr);
					{
						// increment str1 and str2
						fir::Value* v1 = cs->irb.GetPointer(str1, fir::ConstantInt::getNative(1));
						fir::Value* v2 = cs->irb.GetPointer(str2, fir::ConstantInt::getNative(1));

						cs->irb.WritePtr(v1, str1p);
						cs->irb.WritePtr(v2, str2p);

						cs->irb.UnCondBranch(loopcond);
					}
				}

				cs->irb.setCurrentBlock(merge);
				fir::Value* ret = cs->irb.Subtract(cs->irb.ReadPtr(cs->irb.ReadPtr(str1p)),
					cs->irb.ReadPtr(cs->irb.ReadPtr(str2p)));

				ret = cs->irb.IntSizeCast(ret, func->getReturnType());

				cs->irb.Return(ret);
			}

			cmpf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}


	static void _doRefCount(CodegenState* cs, fir::Function* func, bool decrement)
	{
		auto str = func->getArguments()[0];
		auto rcp = cs->irb.GetSAARefCountPointer(str, "rcp");

		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);
		fir::IRBlock* dorc = cs->irb.addNewBlockInFunction("dorc", func);

		cs->irb.CondBranch(cs->irb.ICmpEQ(rcp, fir::ConstantValue::getZeroValue(fir::Type::getNativeWordPtr())),
			merge, dorc);

		cs->irb.setCurrentBlock(dorc);
		{
			auto oldrc = cs->irb.ReadPtr(rcp, "oldrc");
			auto newrc = cs->irb.Add(oldrc, fir::ConstantInt::getNative(decrement ? -1 : 1));

			cs->irb.SetSAARefCount(str, newrc);

			#if DEBUG_STRING_REFCOUNTING
			{
				std::string x = decrement ? "(decr)" : "(incr)";

				cs->printIRDebugMessage("* STRING: " + x + " - new rc of: (ptr: %p, len: %ld, cap: %ld) = %d",
					{ cs->irb.GetSAAData(str), cs->irb.GetSAALength(str), cs->irb.GetSAACapacity(str), cs->irb.GetSAARefCount(str) });
			}
			#endif

			if(decrement)
			{
				fir::IRBlock* dofree = cs->irb.addNewBlockInFunction("dofree", func);
				cs->irb.CondBranch(cs->irb.ICmpEQ(newrc, fir::ConstantInt::getNative(0)),
					dofree, merge);

				cs->irb.setCurrentBlock(dofree);
				{
					auto freefn = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
					iceAssert(freefn);

					auto buf = cs->irb.GetSAAData(str, "buf");
					buf = cs->irb.PointerTypeCast(buf, fir::Type::getMutInt8Ptr());

					cs->irb.Call(freefn, buf);
					cs->irb.Call(freefn, cs->irb.PointerTypeCast(cs->irb.GetSAARefCountPointer(str), fir::Type::getMutInt8Ptr()));

					#if DEBUG_STRING_ALLOCATION
					{
						cs->printIRDebugMessage("* STRING: free(): (ptr: %p / rcp: %p)", {
							buf, cs->irb.GetSAARefCountPointer(str) });
					}
					#endif
				}
			}

			cs->irb.UnCondBranch(merge);
		}

		cs->irb.setCurrentBlock(merge);
		{
			cs->irb.ReturnVoid();
		}
	}




	fir::Function* getRefCountIncrementFunction(CodegenState* cs)
	{
		auto fname = misc::getIncrRefcount_FName(fir::Type::getString());
		fir::Function* retfn = cs->module->getFunction(fname);

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getString() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			_doRefCount(cs, func, false);

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}

	fir::Function* getRefCountDecrementFunction(CodegenState* cs)
	{
		auto fname = misc::getDecrRefcount_FName(fir::Type::getString());
		fir::Function* retfn = cs->module->getFunction(fname);

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getString() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			_doRefCount(cs, func, true);

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}





	fir::Function* getUnicodeLengthFunction(CodegenState* cs)
	{
		auto fname = misc::getUtf8Length_FName();
		fir::Function* lenf = cs->module->getFunction(fname);

		if(!lenf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getNativeWord()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* _ptr = func->getArguments()[0];
			iceAssert(_ptr);

			fir::Value* ptrp = cs->irb.StackAlloc(fir::Type::getInt8Ptr());
			cs->irb.WritePtr(_ptr, ptrp);

			/*

				public fn Utf8Length(_text: i8*) -> i64
				{
					var len = 0
					var text = _text


					while *text != 0
					{
						if (*text & 0xC0) != 0x80
						{
							len += 1
						}

						text = text + 1
					}

					return len
				}
			*/
			auto i0 = fir::ConstantInt::getNative(0);
			auto i1 = fir::ConstantInt::getNative(1);
			auto c0 = fir::ConstantInt::getInt8(0);

			fir::Value* lenp = cs->irb.StackAlloc(fir::Type::getNativeWord());
			cs->irb.WritePtr(i0, lenp);


			fir::IRBlock* cond = cs->irb.addNewBlockInFunction("cond", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);


			fir::Value* isnull = cs->irb.ICmpEQ(fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()), _ptr);
			cs->irb.CondBranch(isnull, merge, cond);

			cs->irb.setCurrentBlock(cond);
			{
				auto ch = cs->irb.ReadPtr(cs->irb.ReadPtr(ptrp));
				auto isnotzero = cs->irb.ICmpNEQ(ch, c0);

				cs->irb.CondBranch(isnotzero, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				// if statement
				auto ch = cs->irb.ReadPtr(cs->irb.ReadPtr(ptrp));

				auto mask = cs->irb.BitwiseAND(ch, cs->irb.IntSizeCast(fir::ConstantInt::getUint8(0xC0), fir::Type::getInt8()));
				auto isch = cs->irb.ICmpNEQ(mask, cs->irb.IntSizeCast(fir::ConstantInt::getUint8(0x80), fir::Type::getInt8()));

				fir::IRBlock* incr = cs->irb.addNewBlockInFunction("incr", func);
				fir::IRBlock* skip = cs->irb.addNewBlockInFunction("skip", func);

				cs->irb.CondBranch(isch, incr, skip);
				cs->irb.setCurrentBlock(incr);
				{
					cs->irb.WritePtr(cs->irb.Add(cs->irb.ReadPtr(lenp), i1), lenp);

					cs->irb.UnCondBranch(skip);
				}

				cs->irb.setCurrentBlock(skip);
				{
					auto newptr = cs->irb.GetPointer(cs->irb.ReadPtr(ptrp), i1);
					cs->irb.WritePtr(newptr, ptrp);

					cs->irb.UnCondBranch(cond);
				}
			}

			cs->irb.setCurrentBlock(merge);
			{
				auto len = cs->irb.ReadPtr(lenp);
				cs->irb.Return(len);
			}


			lenf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(lenf);
		return lenf;
	}
}
}
}

