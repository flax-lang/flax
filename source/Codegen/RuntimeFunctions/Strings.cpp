// Strings.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

using namespace Codegen;
using namespace Ast;

#define BUILTIN_STRINGREF_INCR_FUNC_NAME			"__.stringref_incr"
#define BUILTIN_STRINGREF_DECR_FUNC_NAME			"__.stringref_decr"

#define BUILTIN_STRING_CLONE_FUNC_NAME				"__.string_clone"
#define BUILTIN_STRING_APPEND_FUNC_NAME				"__.string_append"
#define BUILTIN_STRING_APPEND_CHAR_FUNC_NAME		"__.string_appendchar"
#define BUILTIN_STRING_CMP_FUNC_NAME				"__.string_compare"

#define BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME		"__.string_checkliteralmodify"
#define BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME		"__.string_boundscheck"

#define DEBUG_ARC 0

namespace Codegen {
namespace RuntimeFuncs {
namespace String
{
	fir::Function* getCloneFunction(CodegenInstance* cgi)
	{
		fir::Function* clonef = cgi->module->getFunction(Identifier(BUILTIN_STRING_CLONE_FUNC_NAME, IdKind::Name));

		if(!clonef)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CLONE_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getStringType(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			iceAssert(s1);

			// get an empty string
			fir::Value* newstrp = cgi->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			fir::Value* lhslen = cgi->irb.CreateGetStringLength(s1, "l1");
			fir::Value* lhsbuf = cgi->irb.CreateGetStringData(s1, "d1");


			// space for null + refcount
			size_t i64Size = cgi->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = cgi->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(1 + i64Size));

			// now malloc.
			fir::Function* mallocf = cgi->module->getFunction(cgi->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = cgi->irb.CreateCall1(mallocf, malloclen);



			// move it forward (skip the refcount)
			buf = cgi->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");
			cgi->irb.CreateCall(memcpyf, { buf, lhsbuf, cgi->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = cgi->irb.CreatePointerAdd(buf, lhslen);

			// null terminator
			cgi->irb.CreateStore(fir::ConstantInt::getInt8(0), offsetbuf);

			// ok, now fix it
			cgi->irb.CreateSetStringData(newstrp, buf);
			cgi->irb.CreateSetStringLength(newstrp, lhslen);
			cgi->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));


			#if DEBUG_ARC
			{
				fir::Function* printfn = cgi->module->getOrCreateFunction(Identifier("printf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Value* tmp = cgi->module->createGlobalString("clone string '%s' / %ld / %p\n");
				tmp = cgi->irb.CreateConstGEP2(tmp, 0, 0);

				cgi->irb.CreateCall(printfn, { tmp, buf, lhslen, buf });
			}
			#endif


			cgi->irb.CreateReturn(cgi->irb.CreateLoad(newstrp));

			clonef = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(clonef);
		return clonef;
	}





	fir::Function* getAppendFunction(CodegenInstance* cgi)
	{
		fir::Function* appendf = cgi->module->getFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getStringType()->getPointerTo() },
				fir::Type::getStringType(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

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

			// get an empty string
			fir::Value* newstrp = cgi->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = cgi->irb.CreateGetStringLength(s1, "l1");
			fir::Value* rhslen = cgi->irb.CreateGetStringLength(s2, "l2");

			fir::Value* lhsbuf = cgi->irb.CreateGetStringData(s1, "d1");
			fir::Value* rhsbuf = cgi->irb.CreateGetStringData(s2, "d2");

			// ok. combine the lengths
			fir::Value* newlen = cgi->irb.CreateAdd(lhslen, rhslen);

			// space for null + refcount
			size_t i64Size = cgi->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = cgi->irb.CreateAdd(newlen, fir::ConstantInt::getInt64(1 + i64Size));

			// now malloc.
			fir::Function* mallocf = cgi->module->getFunction(cgi->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = cgi->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = cgi->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");
			cgi->irb.CreateCall(memcpyf, { buf, lhsbuf, cgi->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = cgi->irb.CreatePointerAdd(buf, lhslen);
			cgi->irb.CreateCall(memcpyf, { offsetbuf, rhsbuf, cgi->irb.CreateIntSizeCast(rhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			// null terminator
			fir::Value* nt = cgi->irb.CreateGetPointer(offsetbuf, rhslen);
			cgi->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if 0
			{
				fir::Value* tmpstr = cgi->module->createGlobalString("malloc: %p / %p (%s)\n");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				cgi->irb.CreateCall(cgi->module->getFunction(cgi->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, buf, tmp, buf });
			}
			#endif

			// ok, now fix it
			cgi->irb.CreateSetStringData(newstrp, buf);
			cgi->irb.CreateSetStringLength(newstrp, newlen);
			cgi->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));

			cgi->irb.CreateReturn(cgi->irb.CreateLoad(newstrp));

			appendf = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}

	fir::Function* getCharAppendFunction(CodegenInstance* cgi)
	{
		fir::Function* appendf = cgi->module->getFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getCharType() },
				fir::Type::getStringType(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			// add a char to a string
			// steps:
			//
			// 1. get the size of the left string
			// 2. malloc a string of that size + 1=2
			// 3. make a new string
			// 4. set the buffer to the malloced buffer
			// 5. memcpy.
			// 6. set the length to a + b
			// 7. return.

			// get an empty string
			fir::Value* newstrp = cgi->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = cgi->irb.CreateGetStringLength(s1, "l1");
			fir::Value* lhsbuf = cgi->irb.CreateGetStringData(s1, "d1");


			// space for null (1) + refcount (i64size) + the char (another 1)
			size_t i64Size = cgi->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = cgi->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(2 + i64Size));

			// now malloc.
			fir::Function* mallocf = cgi->module->getFunction(cgi->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = cgi->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = cgi->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");
			cgi->irb.CreateCall(memcpyf, { buf, lhsbuf, cgi->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = cgi->irb.CreatePointerAdd(buf, lhslen);

			// store the char.
			fir::Value* ch = cgi->irb.CreateBitcast(s2, fir::Type::getInt8());
			cgi->irb.CreateStore(ch, offsetbuf);

			// null terminator
			fir::Value* nt = cgi->irb.CreateGetPointer(offsetbuf, fir::ConstantInt::getInt64(1));
			cgi->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if 0
			{
				fir::Value* tmpstr = cgi->module->createGlobalString("malloc: %p / %p (%s)\n");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				cgi->irb.CreateCall(cgi->module->getFunction(cgi->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, buf, tmp, buf });
			}
			#endif

			// ok, now fix it
			cgi->irb.CreateSetStringData(newstrp, buf);
			cgi->irb.CreateSetStringLength(newstrp, cgi->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(1)));
			cgi->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));

			cgi->irb.CreateReturn(cgi->irb.CreateLoad(newstrp));

			appendf = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}







	fir::Function* getCompareFunction(CodegenInstance* cgi)
	{
		fir::Function* cmpf = cgi->module->getFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name));

		if(!cmpf)
		{
			// great.
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getStringType()->getPointerTo() },
				fir::Type::getInt64(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			cgi->irb.setCurrentBlock(entry);

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
				fir::Value* str1p = cgi->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				cgi->irb.CreateStore(cgi->irb.CreateGetStringData(s1, "s1"), str1p);

				fir::Value* str2p = cgi->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				cgi->irb.CreateStore(cgi->irb.CreateGetStringData(s2, "s2"), str2p);


				fir::IRBlock* loopcond = cgi->irb.addNewBlockInFunction("cond1", func);
				fir::IRBlock* loopincr = cgi->irb.addNewBlockInFunction("loopincr", func);
				fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

				cgi->irb.CreateUnCondBranch(loopcond);
				cgi->irb.setCurrentBlock(loopcond);
				{
					fir::IRBlock* cond2 = cgi->irb.addNewBlockInFunction("cond2", func);

					fir::Value* str1 = cgi->irb.CreateLoad(str1p);
					fir::Value* str2 = cgi->irb.CreateLoad(str2p);

					// make sure ptr1 is not null
					fir::Value* cnd = cgi->irb.CreateICmpNEQ(cgi->irb.CreateLoad(str1), fir::ConstantInt::getInt8(0));
					cgi->irb.CreateCondBranch(cnd, cond2, merge);

					cgi->irb.setCurrentBlock(cond2);
					{
						// check that they are equal
						fir::Value* iseq = cgi->irb.CreateICmpEQ(cgi->irb.CreateLoad(str1), cgi->irb.CreateLoad(str2));
						cgi->irb.CreateCondBranch(iseq, loopincr, merge);
					}


					cgi->irb.setCurrentBlock(loopincr);
					{
						// increment str1 and str2
						fir::Value* v1 = cgi->irb.CreatePointerAdd(str1, fir::ConstantInt::getInt64(1));
						fir::Value* v2 = cgi->irb.CreatePointerAdd(str2, fir::ConstantInt::getInt64(1));

						cgi->irb.CreateStore(v1, str1p);
						cgi->irb.CreateStore(v2, str2p);

						cgi->irb.CreateUnCondBranch(loopcond);
					}
				}

				cgi->irb.setCurrentBlock(merge);
				fir::Value* ret = cgi->irb.CreateSub(cgi->irb.CreateLoad(cgi->irb.CreateLoad(str1p)),
					cgi->irb.CreateLoad(cgi->irb.CreateLoad(str2p)));

				ret = cgi->irb.CreateIntSizeCast(ret, func->getReturnType());

				cgi->irb.CreateReturn(ret);
			}

			cmpf = func;
			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}






	fir::Function* getRefCountIncrementFunction(CodegenInstance* cgi)
	{
		fir::Function* incrf = cgi->module->getFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name));

		if(!incrf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* getref = cgi->irb.addNewBlockInFunction("getref", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);
			cgi->irb.setCurrentBlock(entry);

			// if ptr is 0, we exit early.
			{
				fir::Value* ptr = cgi->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = cgi->irb.CreateICmpEQ(ptr, fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));

				cgi->irb.CreateCondBranch(cond, merge, getref);
			}


			cgi->irb.setCurrentBlock(getref);
			fir::Value* curRc = cgi->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// never increment the refcount if this is a string literal
			// how do we know? the refcount was -1 to begin with.

			// check.
			fir::IRBlock* doadd = cgi->irb.addNewBlockInFunction("doref", func);
			{
				fir::Value* cond = cgi->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, merge, doadd);
			}

			cgi->irb.setCurrentBlock(doadd);
			fir::Value* newRc = cgi->irb.CreateAdd(curRc, fir::ConstantInt::getInt64(1));
			cgi->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_ARC
			{
				fir::Value* tmpstr = cgi->module->createGlobalString("(incr) new rc of %p ('%s') = %d\n");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				auto bufp = cgi->irb.CreateGetStringData(func->getArguments()[0]);

				cgi->irb.CreateCall(cgi->getOrDeclareLibCFunc("printf"), { tmpstr, bufp, bufp, newRc });
			}
			#endif

			cgi->irb.CreateUnCondBranch(merge);
			cgi->irb.setCurrentBlock(merge);
			cgi->irb.CreateReturnVoid();

			cgi->irb.setCurrentBlock(restore);

			incrf = func;
		}

		iceAssert(incrf);
		return incrf;
	}


	fir::Function* getRefCountDecrementFunction(CodegenInstance* cgi)
	{
		fir::Function* decrf = cgi->module->getFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name));

		if(!decrf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* checkneg = cgi->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* dotest = cgi->irb.addNewBlockInFunction("dotest", func);
			fir::IRBlock* dealloc = cgi->irb.addNewBlockInFunction("deallocate", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);


			// note:
			// if the ptr is 0, we exit immediately
			// if the refcount is -1, we exit as well.


			cgi->irb.setCurrentBlock(entry);
			{
				fir::Value* ptr = cgi->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = cgi->irb.CreateICmpEQ(ptr, fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));

				cgi->irb.CreateCondBranch(cond, merge, checkneg);
			}


			// needs to handle freeing the thing.
			cgi->irb.setCurrentBlock(checkneg);
			fir::Value* curRc = cgi->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// check.
			{
				fir::Value* cond = cgi->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, merge, dotest);
			}


			cgi->irb.setCurrentBlock(dotest);
			fir::Value* newRc = cgi->irb.CreateSub(curRc, fir::ConstantInt::getInt64(1));
			cgi->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_ARC
			{
				fir::Value* tmpstr = cgi->module->createGlobalString("(decr) new rc of %p ('%s') = %d\n");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				auto bufp = cgi->irb.CreateGetStringData(func->getArguments()[0]);

				cgi->irb.CreateCall(cgi->getOrDeclareLibCFunc("printf"), { tmpstr, bufp, bufp, newRc });
			}
			#endif

			{
				fir::Value* cond = cgi->irb.CreateICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, dealloc, merge);

				cgi->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = cgi->irb.CreateGetStringData(func->getArguments()[0]);


				#if DEBUG_ARC
				{
					fir::Value* tmpstr = cgi->module->createGlobalString("free %p ('%s')\n");
					tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);
					cgi->irb.CreateCall3(cgi->getOrDeclareLibCFunc("printf"), tmpstr, bufp, bufp);
				}
				#endif



				fir::Function* freefn = cgi->getOrDeclareLibCFunc("free");
				iceAssert(freefn);

				cgi->irb.CreateCall1(freefn, cgi->irb.CreatePointerAdd(bufp, fir::ConstantInt::getInt64(-8)));

				cgi->irb.CreateSetStringData(func->getArguments()[0], fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));
				cgi->irb.CreateUnCondBranch(merge);
			}

			cgi->irb.setCurrentBlock(merge);
			cgi->irb.CreateReturnVoid();

			cgi->irb.setCurrentBlock(restore);

			decrf = func;
		}

		iceAssert(decrf);
		return decrf;
	}




	fir::Function* getBoundsCheckFunction(CodegenInstance* cgi)
	{
		fir::Function* fn = cgi->module->getFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cgi->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* checkneg = cgi->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

			cgi->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* len = cgi->irb.CreateGetStringLength(arg1);
			fir::Value* res = cgi->irb.CreateICmpGEQ(arg2, len);

			cgi->irb.CreateCondBranch(res, failb, checkneg);
			cgi->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cgi->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = cgi->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = cgi->module->createGlobalString("w");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = cgi->module->createGlobalString("Tried to index string at index '%zd'; length is only '%zd'! (max index is thus '%zu')\n");
				fmtstr = cgi->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				cgi->irb.CreateCall(fprintfn, { err, fmtstr, arg2, len, cgi->irb.CreateSub(len, fir::ConstantInt::getInt64(1)) });

				cgi->irb.CreateCall0(cgi->getOrDeclareLibCFunc("abort"));
				cgi->irb.CreateUnreachable();
			}

			cgi->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res = cgi->irb.CreateICmpLT(arg2, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(res, failb, merge);
			}

			cgi->irb.setCurrentBlock(merge);
			{
				cgi->irb.CreateReturnVoid();
			}

			fn = func;


			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






	fir::Function* getCheckLiteralWriteFunction(CodegenInstance* cgi)
	{
		fir::Function* fn = cgi->module->getFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cgi->irb.getCurrentBlock();


			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cgi->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

			cgi->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* rc = cgi->irb.CreateGetStringRefCount(arg1);
			fir::Value* res = cgi->irb.CreateICmpLT(rc, fir::ConstantInt::getInt64(0));

			cgi->irb.CreateCondBranch(res, failb, merge);
			cgi->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = cgi->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()),
					fir::LinkageType::External);

				fir::Function* fdopenf = cgi->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = cgi->module->createGlobalString("w");
				tmpstr = cgi->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = cgi->module->createGlobalString("Tried to write to immutable string literal '%s' at index '%zd'!\n");
				fmtstr = cgi->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				fir::Value* dp = cgi->irb.CreateGetStringData(arg1);
				cgi->irb.CreateCall(fprintfn, { err, fmtstr, dp, arg2 });

				cgi->irb.CreateCall0(cgi->getOrDeclareLibCFunc("abort"));
				cgi->irb.CreateUnreachable();
			}

			cgi->irb.setCurrentBlock(merge);
			{
				cgi->irb.CreateReturnVoid();
			}

			fn = func;


			cgi->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}
}
}
}





































