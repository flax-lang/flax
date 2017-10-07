// strings.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"

// generate runtime glue code

#define BUILTIN_STRINGREF_INCR_FUNC_NAME			"__stringref_incr"
#define BUILTIN_STRINGREF_DECR_FUNC_NAME			"__stringref_decr"

#define BUILTIN_STRING_CLONE_FUNC_NAME				"__string_clone"
#define BUILTIN_STRING_APPEND_FUNC_NAME				"__string_append"
#define BUILTIN_STRING_APPEND_CHAR_FUNC_NAME		"__string_appendchar"
#define BUILTIN_STRING_CMP_FUNC_NAME				"__string_compare"

#define BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME		"__string_checkliteralmodify"
#define BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME		"__string_boundscheck"

#define DEBUG_MASTER		1
#define DEBUG_ALLOCATION	(1 & DEBUG_MASTER)
#define DEBUG_REFCOUNTING	(1 & DEBUG_MASTER)

#define REFCOUNT_SIZE		8


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
			fir::Value* lhslen = cs->irb.CreateGetStringLength(s1, "l1");
			fir::Value* lhsbuf = cs->irb.CreateGetStringData(s1, "d1");


			// space for null + refcount

			fir::Value* malloclen = cs->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(1 + REFCOUNT_SIZE));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.CreateCall1(mallocf, malloclen);



			// move it forward (skip the refcount)
			buf = cs->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			// now memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.CreateCall(memcpyf, { buf, lhsbuf, cs->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			fir::Value* offsetbuf = cs->irb.CreatePointerAdd(buf, lhslen);

			// null terminator
			cs->irb.CreateStore(fir::ConstantInt::getInt8(0), offsetbuf);

			// ok, now fix it

			fir::Value* str = cs->irb.CreateValue(fir::Type::getString());

			str = cs->irb.CreateSetStringData(str, buf);
			str = cs->irb.CreateSetStringLength(str, lhslen);
			cs->irb.CreateSetStringRefCount(str, fir::ConstantInt::getInt64(1));

			#if DEBUG_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("clone string '%s' / %ld / %p\n");
				cs->irb.CreateCall(printfn, { tmpstr, buf, lhslen, buf });
			}
			#endif


			cs->irb.CreateReturn(str);

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

			fir::Value* lhslen = cs->irb.CreateGetStringLength(s1, "l1");
			fir::Value* rhslen = cs->irb.CreateGetStringLength(s2, "l2");

			fir::Value* lhsbuf = cs->irb.CreateGetStringData(s1, "d1");
			fir::Value* rhsbuf = cs->irb.CreateGetStringData(s2, "d2");

			// ok. combine the lengths
			fir::Value* newlen = cs->irb.CreateAdd(lhslen, rhslen);

			// space for null + refcount
			fir::Value* malloclen = cs->irb.CreateAdd(newlen, fir::ConstantInt::getInt64(1 + REFCOUNT_SIZE));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = cs->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			// now memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.CreateCall(memcpyf, { buf, lhsbuf, cs->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			fir::Value* offsetbuf = cs->irb.CreatePointerAdd(buf, lhslen);
			cs->irb.CreateCall(memcpyf, { offsetbuf, rhsbuf, cs->irb.CreateIntSizeCast(rhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			// null terminator
			fir::Value* nt = cs->irb.CreateGetPointer(offsetbuf, rhslen);
			cs->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if DEBUG_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("malloc (%zu): %p ('%s')\n");
				cs->irb.CreateCall(printfn, { tmpstr, malloclen, buf, buf });
			}
			#endif

			// ok, now fix it
			fir::Value* str = cs->irb.CreateValue(fir::Type::getString());

			str = cs->irb.CreateSetStringData(str, buf);
			str = cs->irb.CreateSetStringLength(str, newlen);
			cs->irb.CreateSetStringRefCount(str, fir::ConstantInt::getInt64(1));

			cs->irb.CreateReturn(str);

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

			fir::Value* lhsbuf = cs->irb.CreateGetStringData(s1, "d1");
			fir::Value* lhslen = cs->irb.CreateGetStringLength(s1, "l1");


			// space for null (1) + refcount (i64size) + the char (another 1)
			fir::Value* malloclen = cs->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(2 + REFCOUNT_SIZE));

			// now malloc.
			fir::Function* mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			fir::Value* buf = cs->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = cs->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(REFCOUNT_SIZE));

			// now memcpy
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
			cs->irb.CreateCall(memcpyf, { buf, lhsbuf, lhslen,
				fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

			fir::Value* offsetbuf = cs->irb.CreatePointerAdd(buf, lhslen);

			// store the char.
			fir::Value* ch = cs->irb.CreateBitcast(s2, fir::Type::getInt8());
			cs->irb.CreateStore(ch, offsetbuf);

			// null terminator
			fir::Value* nt = cs->irb.CreatePointerAdd(offsetbuf, fir::ConstantInt::getInt64(1));
			cs->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if DEBUG_ALLOCATION
			{
				fir::Value* tmpstr = cs->module->createGlobalString("malloc: %p / %p ('%s') // ('%s') // (appc)\n");
				cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { tmpstr, lhsbuf, buf, buf, lhsbuf });
			}
			#endif

			// ok, now fix it
			// get an empty string
			fir::Value* str = cs->irb.CreateValue(fir::Type::getString());

			str = cs->irb.CreateSetStringData(str, buf);
			str = cs->irb.CreateSetStringLength(str, cs->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(1)));
			cs->irb.CreateSetStringRefCount(str, fir::ConstantInt::getInt64(1));

			cs->irb.CreateReturn(str);

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
				fir::Value* str1p = cs->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				cs->irb.CreateStore(cs->irb.CreateGetStringData(s1, "s1"), str1p);

				fir::Value* str2p = cs->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				cs->irb.CreateStore(cs->irb.CreateGetStringData(s2, "s2"), str2p);


				fir::IRBlock* loopcond = cs->irb.addNewBlockInFunction("cond1", func);
				fir::IRBlock* loopincr = cs->irb.addNewBlockInFunction("loopincr", func);
				fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

				cs->irb.CreateUnCondBranch(loopcond);
				cs->irb.setCurrentBlock(loopcond);
				{
					fir::IRBlock* cond2 = cs->irb.addNewBlockInFunction("cond2", func);

					fir::Value* str1 = cs->irb.CreateLoad(str1p);
					fir::Value* str2 = cs->irb.CreateLoad(str2p);

					// make sure ptr1 is not null
					fir::Value* cnd = cs->irb.CreateICmpNEQ(cs->irb.CreateLoad(str1), fir::ConstantInt::getInt8(0));
					cs->irb.CreateCondBranch(cnd, cond2, merge);

					cs->irb.setCurrentBlock(cond2);
					{
						// check that they are equal
						fir::Value* iseq = cs->irb.CreateICmpEQ(cs->irb.CreateLoad(str1), cs->irb.CreateLoad(str2));
						cs->irb.CreateCondBranch(iseq, loopincr, merge);
					}


					cs->irb.setCurrentBlock(loopincr);
					{
						// increment str1 and str2
						fir::Value* v1 = cs->irb.CreatePointerAdd(str1, fir::ConstantInt::getInt64(1));
						fir::Value* v2 = cs->irb.CreatePointerAdd(str2, fir::ConstantInt::getInt64(1));

						cs->irb.CreateStore(v1, str1p);
						cs->irb.CreateStore(v2, str2p);

						cs->irb.CreateUnCondBranch(loopcond);
					}
				}

				cs->irb.setCurrentBlock(merge);
				fir::Value* ret = cs->irb.CreateSub(cs->irb.CreateLoad(cs->irb.CreateLoad(str1p)),
					cs->irb.CreateLoad(cs->irb.CreateLoad(str2p)));

				ret = cs->irb.CreateIntSizeCast(ret, func->getReturnType());

				cs->irb.CreateReturn(ret);
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
				fir::Value* ptr = cs->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = cs->irb.CreateICmpEQ(ptr, fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));

				cs->irb.CreateCondBranch(cond, merge, getref);
			}


			cs->irb.setCurrentBlock(getref);
			fir::Value* curRc = cs->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// never increment the refcount if this is a string literal
			// how do we know? the refcount was -1 to begin with.

			// check.
			fir::IRBlock* doadd = cs->irb.addNewBlockInFunction("doref", func);
			{
				fir::Value* cond = cs->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				cs->irb.CreateCondBranch(cond, merge, doadd);
			}

			cs->irb.setCurrentBlock(doadd);
			fir::Value* newRc = cs->irb.CreateAdd(curRc, fir::ConstantInt::getInt64(1));
			cs->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_REFCOUNTING
			{
				fir::Value* tmpstr = cs->module->createGlobalString("(incr) new rc of %p ('%s') = %d\n");

				auto bufp = cs->irb.CreateGetStringData(func->getArguments()[0]);
				cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { tmpstr, bufp, bufp, newRc });
			}
			#endif

			cs->irb.CreateUnCondBranch(merge);
			cs->irb.setCurrentBlock(merge);
			cs->irb.CreateReturnVoid();

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
				fir::Value* ptr = cs->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = cs->irb.CreateICmpEQ(ptr, fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));

				cs->irb.CreateCondBranch(cond, merge, checkneg);
			}


			// needs to handle freeing the thing.
			cs->irb.setCurrentBlock(checkneg);
			fir::Value* curRc = cs->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// check.
			{
				fir::Value* cond = cs->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				cs->irb.CreateCondBranch(cond, merge, dotest);
			}


			cs->irb.setCurrentBlock(dotest);
			fir::Value* newRc = cs->irb.CreateSub(curRc, fir::ConstantInt::getInt64(1));
			cs->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_REFCOUNTING
			{
				fir::Value* tmpstr = cs->module->createGlobalString("(decr) new rc of %p ('%s') = %d\n");


				auto bufp = cs->irb.CreateGetStringData(func->getArguments()[0]);
				cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { tmpstr, bufp, bufp, newRc });
			}
			#endif

			{
				fir::Value* cond = cs->irb.CreateICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				cs->irb.CreateCondBranch(cond, dealloc, merge);

				cs->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = cs->irb.CreateGetStringData(func->getArguments()[0]);


				#if DEBUG_ALLOCATION
				{
					fir::Value* tmpstr = cs->module->createGlobalString("free %p ('%s') (%d)\n");
					cs->irb.CreateCall(cs->getOrDeclareLibCFunction("printf"), { tmpstr, bufp, bufp, newRc });
				}
				#endif

				fir::Function* freefn = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
				iceAssert(freefn);

				cs->irb.CreateCall1(freefn, cs->irb.CreatePointerSub(bufp, fir::ConstantInt::getInt64(REFCOUNT_SIZE)));

				// cs->irb.CreateSetStringData(func->getArguments()[0], fir::ConstantValue::getZeroValue(fir::Type::getInt8Ptr()));
				cs->irb.CreateUnCondBranch(merge);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.CreateReturnVoid();

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

			fir::Value* len = cs->irb.CreateGetStringLength(arg1);
			fir::Value* res = cs->irb.CreateICmpGEQ(arg2, len);

			cs->irb.CreateCondBranch(res, failb, checkneg);
			cs->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cs->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = cs->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = cs->module->createGlobalString("w");
				fir::Value* fmtstr = cs->module->createGlobalString("%s: Tried to index string at index '%zd'; length is only '%zd'! (max index is thus '%zu')\n");
				fir::Value* posstr = cs->irb.CreateGetStringData(func->getArguments()[2]);

				fir::Value* err = cs->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cs->irb.CreateCall(fprintfn, { err, fmtstr, posstr, arg2, len, cs->irb.CreateSub(len, fir::ConstantInt::getInt64(1)) });

				cs->irb.CreateCall0(cs->getOrDeclareLibCFunction("abort"));
				cs->irb.CreateUnreachable();
			}

			cs->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res = cs->irb.CreateICmpLT(arg2, fir::ConstantInt::getInt64(0));
				cs->irb.CreateCondBranch(res, failb, merge);
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.CreateReturnVoid();
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

			fir::Value* rc = cs->irb.CreateGetStringRefCount(arg1);
			fir::Value* res = cs->irb.CreateICmpLT(rc, fir::ConstantInt::getInt64(0));

			cs->irb.CreateCondBranch(res, failb, merge);
			cs->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cs->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()),
					fir::LinkageType::External);

				fir::Function* fdopenf = cs->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = cs->module->createGlobalString("w");
				fir::Value* fmtstr = cs->module->createGlobalString("%s: Tried to write to immutable string literal '%s' at index '%zd'!\n");
				fir::Value* posstr = cs->irb.CreateGetStringData(func->getArguments()[2]);

				fir::Value* err = cs->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				fir::Value* dp = cs->irb.CreateGetStringData(arg1);
				cs->irb.CreateCall(fprintfn, { err, fmtstr, posstr, dp, arg2 });

				cs->irb.CreateCall0(cs->getOrDeclareLibCFunction("abort"));
				cs->irb.CreateUnreachable();
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.CreateReturnVoid();
			}

			fn = func;


			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}
}
}
}

