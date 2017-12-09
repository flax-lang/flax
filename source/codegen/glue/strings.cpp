// strings.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

// generate runtime glue code

#define BUILTIN_STRINGREF_INCR_FUNC_NAME			"__stringref_incr"
#define BUILTIN_STRINGREF_DECR_FUNC_NAME			"__stringref_decr"

#define BUILTIN_STRING_CLONE_FUNC_NAME				"__string_clone"
#define BUILTIN_STRING_APPEND_FUNC_NAME				"__string_append"
#define BUILTIN_STRING_APPEND_CHAR_FUNC_NAME		"__string_appendchar"
#define BUILTIN_STRING_CMP_FUNC_NAME				"__string_compare"
#define BUILTIN_STRING_UNICODE_LENGTH_FUNC_NAME		"__string_ulength"

#define BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME		"__string_checkliteralmodify"
#define BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME		"__string_boundscheck"


// namespace cgn::glue::string
namespace cgn {
namespace glue {
namespace string
{
	fir::Function* getCloneFunction(CodegenState* cs)
	{
		fir::Function* clonef = cs->module->getFunction(Identifier(BUILTIN_STRING_CLONE_FUNC_NAME, IdKind::Name));

		if(!clonef)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CLONE_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString() }, fir::Type::getString()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			iceAssert(s1);

			// get an empty string
			fir::Value* lhslen = cs->irb.GetStringLength(s1, "l1");
			fir::Value* lhsbuf = cs->irb.GetStringData(s1, "d1");


			// space for null + refcount

			fir::Value* malloclen = cs->irb.Add(lhslen, fir::ConstantInt::getInt64(1 + REFCOUNT_SIZE));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.Call(mallocf, malloclen);


			#if DEBUG_STRING_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("clone string: OLD :: (ptr: %p, len: %ld) | NEW :: (ptr: %p, len: %ld)\n");
				cs->irb.Call(printfn, { tmpstr, lhsbuf, lhslen, buf, lhslen });
			}
			#endif


			// move it forward (skip the refcount)
			buf = cs->irb.PointerAdd(buf, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			// now memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.Call(memcpyf, { buf, lhsbuf, cs->irb.IntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			fir::Value* offsetbuf = cs->irb.PointerAdd(buf, lhslen);

			// null terminator
			cs->irb.Store(fir::ConstantInt::getInt8(0), offsetbuf);

			// ok, now fix it

			fir::Value* str = cs->irb.CreateValue(fir::Type::getString());

			str = cs->irb.SetStringData(str, buf);
			str = cs->irb.SetStringLength(str, lhslen);
			cs->irb.SetStringRefCount(str, fir::ConstantInt::getInt64(1));


			cs->irb.Return(str);

			clonef = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(clonef);
		return clonef;
	}





	fir::Function* getAppendFunction(CodegenState* cs)
	{
		fir::Function* appendf = cs->module->getFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString(), fir::Type::getString() },
				fir::Type::getString()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];



			// add two strings
			// steps:
			//
			// 1. get the size of the left string
			// 2. get the size of the right string
			// 3. add them together
			// 4. malloc a string of that size + 1
			// 5. make a new string
			// 6. set the buffer to the malloced buffer
			// 7. set the length to a + b
			// 8. return.


			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = cs->irb.GetStringLength(s1, "l1");
			fir::Value* rhslen = cs->irb.GetStringLength(s2, "l2");

			fir::Value* lhsbuf = cs->irb.GetStringData(s1, "d1");
			fir::Value* rhsbuf = cs->irb.GetStringData(s2, "d2");

			// ok. combine the lengths
			fir::Value* newlen = cs->irb.Add(lhslen, rhslen);

			// space for null + refcount
			fir::Value* malloclen = cs->irb.Add(newlen, fir::ConstantInt::getInt64(1 + REFCOUNT_SIZE));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.Call(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = cs->irb.PointerAdd(buf, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			// now memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.Call(memcpyf, { buf, lhsbuf, cs->irb.IntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			fir::Value* offsetbuf = cs->irb.PointerAdd(buf, lhslen);
			cs->irb.Call(memcpyf, { offsetbuf, rhsbuf, cs->irb.IntSizeCast(rhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			// null terminator
			fir::Value* nt = cs->irb.GetPointer(offsetbuf, rhslen);
			cs->irb.Store(fir::ConstantInt::getInt8(0), nt);

			#if DEBUG_STRING_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("append string: malloc(%d): (ptr: %p)\n");
				cs->irb.Call(printfn, { tmpstr, malloclen, buf });
			}
			#endif

			// ok, now fix it
			fir::Value* str = cs->irb.CreateValue(fir::Type::getString());

			str = cs->irb.SetStringData(str, buf);
			str = cs->irb.SetStringLength(str, newlen);
			cs->irb.SetStringRefCount(str, fir::ConstantInt::getInt64(1));

			cs->irb.Return(str);

			appendf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}

	fir::Function* getCharAppendFunction(CodegenState* cs)
	{
		fir::Function* appendf = cs->module->getFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString(), fir::Type::getChar() },
				fir::Type::getString()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			// add a char to a string
			// steps:
			//
			// 1. get the size of the left string
			// 2. malloc a string of that size + 1
			// 3. make a new string
			// 4. set the buffer to the malloced buffer
			// 5. memcpy.
			// 6. set the length to a + b
			// 7. return.


			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhsbuf = cs->irb.GetStringData(s1, "d1");
			fir::Value* lhslen = cs->irb.GetStringLength(s1, "l1");


			// space for null (1) + refcount (i64size) + the char (another 1)
			fir::Value* malloclen = cs->irb.Add(lhslen, fir::ConstantInt::getInt64(2 + REFCOUNT_SIZE));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.Call(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = cs->irb.PointerAdd(buf, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			// now memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.Call(memcpyf, { buf, lhsbuf, lhslen,
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			fir::Value* offsetbuf = cs->irb.PointerAdd(buf, lhslen);

			// store the char.
			fir::Value* ch = cs->irb.Bitcast(s2, fir::Type::getInt8());
			cs->irb.Store(ch, offsetbuf);

			// null terminator
			fir::Value* nt = cs->irb.PointerAdd(offsetbuf, fir::ConstantInt::getInt64(1));
			cs->irb.Store(fir::ConstantInt::getInt8(0), nt);

			#if DEBUG_STRING_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("append char: malloc(%d): (ptr: %p)\n");
				cs->irb.Call(printfn, { tmpstr, malloclen, buf });
			}
			#endif


			// ok, now fix it
			// get an empty string
			fir::Value* str = cs->irb.CreateValue(fir::Type::getString());

			str = cs->irb.SetStringData(str, buf);
			str = cs->irb.SetStringLength(str, cs->irb.Add(lhslen, fir::ConstantInt::getInt64(1)));
			cs->irb.SetStringRefCount(str, fir::ConstantInt::getInt64(1));

			cs->irb.Return(str);

			appendf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}







	fir::Function* getCompareFunction(CodegenState* cs)
	{
		fir::Function* cmpf = cs->module->getFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name));

		if(!cmpf)
		{
			// great.
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString(), fir::Type::getString() },
				fir::Type::getInt64()), fir::LinkageType::Internal);

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
				fir::Value* str1p = cs->irb.StackAlloc(fir::Type::getInt8Ptr());
				cs->irb.Store(cs->irb.GetStringData(s1, "s1"), str1p);

				fir::Value* str2p = cs->irb.StackAlloc(fir::Type::getInt8Ptr());
				cs->irb.Store(cs->irb.GetStringData(s2, "s2"), str2p);


				fir::IRBlock* loopcond = cs->irb.addNewBlockInFunction("cond1", func);
				fir::IRBlock* loopincr = cs->irb.addNewBlockInFunction("loopincr", func);
				fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

				cs->irb.UnCondBranch(loopcond);
				cs->irb.setCurrentBlock(loopcond);
				{
					fir::IRBlock* cond2 = cs->irb.addNewBlockInFunction("cond2", func);

					fir::Value* str1 = cs->irb.Load(str1p);
					fir::Value* str2 = cs->irb.Load(str2p);

					// make sure ptr1 is not null
					fir::Value* cnd = cs->irb.ICmpNEQ(cs->irb.Load(str1), fir::ConstantInt::getInt8(0));
					cs->irb.CondBranch(cnd, cond2, merge);

					cs->irb.setCurrentBlock(cond2);
					{
						// check that they are equal
						fir::Value* iseq = cs->irb.ICmpEQ(cs->irb.Load(str1), cs->irb.Load(str2));
						cs->irb.CondBranch(iseq, loopincr, merge);
					}


					cs->irb.setCurrentBlock(loopincr);
					{
						// increment str1 and str2
						fir::Value* v1 = cs->irb.PointerAdd(str1, fir::ConstantInt::getInt64(1));
						fir::Value* v2 = cs->irb.PointerAdd(str2, fir::ConstantInt::getInt64(1));

						cs->irb.Store(v1, str1p);
						cs->irb.Store(v2, str2p);

						cs->irb.UnCondBranch(loopcond);
					}
				}

				cs->irb.setCurrentBlock(merge);
				fir::Value* ret = cs->irb.Sub(cs->irb.Load(cs->irb.Load(str1p)),
					cs->irb.Load(cs->irb.Load(str2p)));

				ret = cs->irb.IntSizeCast(ret, func->getReturnType());

				cs->irb.Return(ret);
			}

			cmpf = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}






	fir::Function* getRefCountIncrementFunction(CodegenState* cs)
	{
		fir::Function* incrf = cs->module->getFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name));

		if(!incrf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString() }, fir::Type::getVoid()),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* getref = cs->irb.addNewBlockInFunction("getref", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);
			cs->irb.setCurrentBlock(entry);

			// if ptr is 0, we exit early.
			{
				fir::Value* ptr = cs->irb.GetStringData(func->getArguments()[0]);
				fir::Value* cond = cs->irb.ICmpEQ(ptr, fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));

				cs->irb.CondBranch(cond, merge, getref);
			}


			cs->irb.setCurrentBlock(getref);
			fir::Value* curRc = cs->irb.GetStringRefCount(func->getArguments()[0]);

			// never increment the refcount if this is a string literal
			// how do we know? the refcount was -1 to begin with.

			// check.
			fir::IRBlock* doadd = cs->irb.addNewBlockInFunction("doref", func);
			{
				fir::Value* cond = cs->irb.ICmpLT(curRc, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(cond, merge, doadd);
			}

			cs->irb.setCurrentBlock(doadd);
			fir::Value* newRc = cs->irb.Add(curRc, fir::ConstantInt::getInt64(1));
			cs->irb.SetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_STRING_REFCOUNTING
			{
				fir::Value* tmpstr = cs->module->createGlobalString("(incr) new rc of %p ('%s') = %d\n");

				auto bufp = cs->irb.GetStringData(func->getArguments()[0]);
				cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, bufp, bufp, newRc });
			}
			#endif

			cs->irb.UnCondBranch(merge);
			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);

			incrf = func;
		}

		iceAssert(incrf);


		return incrf;
	}


	fir::Function* getRefCountDecrementFunction(CodegenState* cs)
	{
		fir::Function* decrf = cs->module->getFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name));

		if(!decrf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString() }, fir::Type::getVoid()),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* checkneg = cs->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* dotest = cs->irb.addNewBlockInFunction("dotest", func);
			fir::IRBlock* dealloc = cs->irb.addNewBlockInFunction("deallocate", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);


			// note:
			// if the ptr is 0, we exit immediately
			// if the refcount is -1, we exit as well.


			cs->irb.setCurrentBlock(entry);
			{
				fir::Value* ptr = cs->irb.GetStringData(func->getArguments()[0]);
				fir::Value* cond = cs->irb.ICmpEQ(ptr, fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));

				cs->irb.CondBranch(cond, merge, checkneg);
			}


			// needs to handle freeing the thing.
			cs->irb.setCurrentBlock(checkneg);
			fir::Value* curRc = cs->irb.GetStringRefCount(func->getArguments()[0]);

			// check.
			{
				fir::Value* cond = cs->irb.ICmpLT(curRc, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(cond, merge, dotest);
			}


			cs->irb.setCurrentBlock(dotest);
			fir::Value* newRc = cs->irb.Sub(curRc, fir::ConstantInt::getInt64(1));
			cs->irb.SetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_STRING_REFCOUNTING
			{
				fir::Value* tmpstr = cs->module->createGlobalString("(decr) new rc of %p ('%s') = %d\n");


				auto bufp = cs->irb.GetStringData(func->getArguments()[0]);
				cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, bufp, bufp, newRc });
			}
			#endif

			{
				fir::Value* cond = cs->irb.ICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(cond, dealloc, merge);

				cs->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = cs->irb.GetStringData(func->getArguments()[0]);


				#if DEBUG_STRING_ALLOCATION
				{
					fir::Value* tmpstr = cs->module->createGlobalString("free %p ('%s') (%d)\n");
					cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), { tmpstr, bufp, bufp, newRc });
				}
				#endif

				fir::Function* freefn = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
				iceAssert(freefn);

				cs->irb.Call(freefn, cs->irb.PointerSub(bufp, fir::ConstantInt::getInt64(REFCOUNT_SIZE)));

				// cs->irb.SetStringData(func->getArguments()[0], fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));
				cs->irb.UnCondBranch(merge);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);

			decrf = func;
		}

		iceAssert(decrf);
		return decrf;
	}




	fir::Function* getBoundsCheckFunction(CodegenState* cs)
	{
		fir::Function* fn = cs->module->getFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString(), fir::Type::getInt64(), fir::Type::getString() },
					fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* checkneg = cs->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			cs->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* len = cs->irb.GetStringLength(arg1);
			fir::Value* res = cs->irb.ICmpGEQ(arg2, len);

			cs->irb.CondBranch(res, failb, checkneg);
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
				fir::Value* fmtstr = cs->module->createGlobalString("%s: Tried to index string at index '%zd'; length is only '%zd'! (max index is thus '%zu')\n");
				fir::Value* posstr = cs->irb.GetStringData(func->getArguments()[2]);

				fir::Value* err = cs->irb.Call(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cs->irb.Call(fprintfn, { err, fmtstr, posstr, arg2, len, cs->irb.Sub(len, fir::ConstantInt::getInt64(1)) });

				cs->irb.Call(cs->getOrDeclareLibCFunction("abort"));
				cs->irb.Unreachable();
			}

			cs->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res = cs->irb.ICmpLT(arg2, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(res, failb, merge);
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.ReturnVoid();
			}

			fn = func;


			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






	fir::Function* getCheckLiteralWriteFunction(CodegenState* cs)
	{
		fir::Function* fn = cs->module->getFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();


			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getString(), fir::Type::getInt64(), fir::Type::getString() },
					fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			cs->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* rc = cs->irb.GetStringRefCount(arg1);
			fir::Value* res = cs->irb.ICmpLT(rc, fir::ConstantInt::getInt64(0));

			cs->irb.CondBranch(res, failb, merge);
			cs->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cs->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()),
					fir::LinkageType::External);

				fir::Function* fdopenf = cs->module->getOrCreateFunction(Identifier(CRT_FDOPEN, IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = cs->module->createGlobalString("w");
				fir::Value* fmtstr = cs->module->createGlobalString("%s: Tried to write to immutable string literal '%s' at index '%zd'!\n");
				fir::Value* posstr = cs->irb.GetStringData(func->getArguments()[2]);

				fir::Value* err = cs->irb.Call(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				fir::Value* dp = cs->irb.GetStringData(arg1);
				cs->irb.Call(fprintfn, { err, fmtstr, posstr, dp, arg2 });

				cs->irb.Call(cs->getOrDeclareLibCFunction("abort"));
				cs->irb.Unreachable();
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.ReturnVoid();
			}

			fn = func;


			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}


	fir::Function* getUnicodeLengthFunction(CodegenState* cs)
	{
		fir::Function* lenf = cs->module->getFunction(Identifier(BUILTIN_STRING_UNICODE_LENGTH_FUNC_NAME, IdKind::Name));

		if(!lenf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(BUILTIN_STRING_UNICODE_LENGTH_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getInt64()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* _ptr = func->getArguments()[0];
			iceAssert(_ptr);

			fir::Value* ptrp = cs->irb.StackAlloc(fir::Type::getInt8Ptr());
			cs->irb.Store(_ptr, ptrp);

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
			auto i0 = fir::ConstantInt::getInt64(0);
			auto i1 = fir::ConstantInt::getInt64(1);
			auto c0 = fir::ConstantInt::getInt8(0);

			fir::Value* lenp = cs->irb.StackAlloc(fir::Type::getInt64());
			cs->irb.Store(i0, lenp);


			fir::IRBlock* cond = cs->irb.addNewBlockInFunction("cond", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);


			fir::Value* isnull = cs->irb.ICmpEQ(fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()), _ptr);
			cs->irb.CondBranch(isnull, merge, cond);

			cs->irb.setCurrentBlock(cond);
			{
				auto ch = cs->irb.Load(cs->irb.Load(ptrp));
				auto isnotzero = cs->irb.ICmpNEQ(ch, c0);

				cs->irb.CondBranch(isnotzero, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				// if statement
				auto ch = cs->irb.Load(cs->irb.Load(ptrp));

				auto mask = cs->irb.BitwiseAND(ch, fir::ConstantInt::getInt8(0xC0));
				auto isch = cs->irb.ICmpNEQ(mask, fir::ConstantInt::getInt8(0x80));

				fir::IRBlock* incr = cs->irb.addNewBlockInFunction("incr", func);
				fir::IRBlock* skip = cs->irb.addNewBlockInFunction("skip", func);

				cs->irb.CondBranch(isch, incr, skip);
				cs->irb.setCurrentBlock(incr);
				{
					cs->irb.Store(cs->irb.Add(cs->irb.Load(lenp), i1), lenp);

					cs->irb.UnCondBranch(skip);
				}

				cs->irb.setCurrentBlock(skip);
				{
					auto newptr = cs->irb.PointerAdd(cs->irb.Load(ptrp), i1);
					cs->irb.Store(newptr, ptrp);

					cs->irb.UnCondBranch(cond);
				}
			}

			cs->irb.setCurrentBlock(merge);
			{
				auto len = cs->irb.Load(lenp);
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

