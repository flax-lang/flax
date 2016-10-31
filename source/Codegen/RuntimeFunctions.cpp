// RuntimeFunctions.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Codegen;
using namespace Ast;

#define BUILTIN_STRINGREF_INCR_FUNC_NAME			"__.stringref_incr"
#define BUILTIN_STRINGREF_DECR_FUNC_NAME			"__.stringref_decr"
#define BUILTIN_STRING_APPEND_FUNC_NAME				"__.string_append"
#define BUILTIN_STRING_APPEND_CHAR_FUNC_NAME		"__.string_appendchar"
#define BUILTIN_STRING_CMP_FUNC_NAME				"__.string_compare"

#define BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME		"__.string_checkliteralmodify"
#define BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME		"__.string_boundscheck"

#define BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME		"__.array_boundscheck"

#define BUILTIN_DYNARRAY_APPEND_FUNC_NAME			"__.darray_append"
#define BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME	"__.darray_appendelement"
#define BUILTIN_DYNARRAY_CMP_FUNC_NAME				"__.darray_compare"

#define DEBUG_ARC 0

namespace Codegen
{
	fir::Function* CodegenInstance::getStringAppendFunction()
	{
		fir::Function* appendf = this->module->getFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getStringType()->getPointerTo() },
				fir::Type::getStringType(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

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
			fir::Value* newstrp = this->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = this->irb.CreateGetStringLength(s1, "l1");
			fir::Value* rhslen = this->irb.CreateGetStringLength(s2, "l2");

			fir::Value* lhsbuf = this->irb.CreateGetStringData(s1, "d1");
			fir::Value* rhsbuf = this->irb.CreateGetStringData(s2, "d2");

			// ok. combine the lengths
			fir::Value* newlen = this->irb.CreateAdd(lhslen, rhslen);

			// space for null + refcount
			size_t i64Size = this->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = this->irb.CreateAdd(newlen, fir::ConstantInt::getInt64(1 + i64Size));

			// now malloc.
			fir::Function* mallocf = this->module->getFunction(this->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = this->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = this->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = this->module->getIntrinsicFunction("memcpy");
			this->irb.CreateCall(memcpyf, { buf, lhsbuf, this->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = this->irb.CreatePointerAdd(buf, lhslen);
			this->irb.CreateCall(memcpyf, { offsetbuf, rhsbuf, this->irb.CreateIntSizeCast(rhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			// null terminator
			fir::Value* nt = this->irb.CreateGetPointer(offsetbuf, rhslen);
			this->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if 0
			{
				fir::Value* tmpstr = this->module->createGlobalString("malloc: %p / %p (%s)\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, buf, tmp, buf });
			}
			#endif

			// ok, now fix it
			this->irb.CreateSetStringData(newstrp, buf);
			this->irb.CreateSetStringLength(newstrp, newlen);
			this->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));

			this->irb.CreateReturn(this->irb.CreateLoad(newstrp));

			appendf = func;
			this->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}

	fir::Function* CodegenInstance::getStringCharAppendFunction()
	{
		fir::Function* appendf = this->module->getFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getCharType() },
				fir::Type::getStringType(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

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
			fir::Value* newstrp = this->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = this->irb.CreateGetStringLength(s1, "l1");
			fir::Value* lhsbuf = this->irb.CreateGetStringData(s1, "d1");


			// space for null (1) + refcount (i64size) + the char (another 1)
			size_t i64Size = this->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = this->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(2 + i64Size));

			// now malloc.
			fir::Function* mallocf = this->module->getFunction(this->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = this->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = this->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = this->module->getIntrinsicFunction("memcpy");
			this->irb.CreateCall(memcpyf, { buf, lhsbuf, this->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = this->irb.CreatePointerAdd(buf, lhslen);

			// store the char.
			fir::Value* ch = this->irb.CreateBitcast(s2, fir::Type::getInt8());
			this->irb.CreateStore(ch, offsetbuf);

			// null terminator
			fir::Value* nt = this->irb.CreateGetPointer(offsetbuf, fir::ConstantInt::getInt64(1));
			this->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if 0
			{
				fir::Value* tmpstr = this->module->createGlobalString("malloc: %p / %p (%s)\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, buf, tmp, buf });
			}
			#endif

			// ok, now fix it
			this->irb.CreateSetStringData(newstrp, buf);
			this->irb.CreateSetStringLength(newstrp, this->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(1)));
			this->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));

			this->irb.CreateReturn(this->irb.CreateLoad(newstrp));

			appendf = func;
			this->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}







	fir::Function* CodegenInstance::getStringCompareFunction()
	{
		fir::Function* cmpf = this->module->getFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name));

		if(!cmpf)
		{
			// great.
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getStringType()->getPointerTo() },
				fir::Type::getInt64(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

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
				fir::Value* str1p = this->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				this->irb.CreateStore(this->irb.CreateGetStringData(s1, "s1"), str1p);

				fir::Value* str2p = this->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				this->irb.CreateStore(this->irb.CreateGetStringData(s2, "s2"), str2p);


				fir::IRBlock* loopcond = this->irb.addNewBlockInFunction("cond1", func);
				fir::IRBlock* loopincr = this->irb.addNewBlockInFunction("loopincr", func);
				fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

				this->irb.CreateUnCondBranch(loopcond);
				this->irb.setCurrentBlock(loopcond);
				{
					fir::IRBlock* cond2 = this->irb.addNewBlockInFunction("cond2", func);

					fir::Value* str1 = this->irb.CreateLoad(str1p);
					fir::Value* str2 = this->irb.CreateLoad(str2p);

					// make sure ptr1 is not null
					fir::Value* cnd = this->irb.CreateICmpNEQ(this->irb.CreateLoad(str1), fir::ConstantInt::getInt8(0));
					this->irb.CreateCondBranch(cnd, cond2, merge);

					this->irb.setCurrentBlock(cond2);
					{
						// check that they are equal
						fir::Value* iseq = this->irb.CreateICmpEQ(this->irb.CreateLoad(str1), this->irb.CreateLoad(str2));
						this->irb.CreateCondBranch(iseq, loopincr, merge);
					}


					this->irb.setCurrentBlock(loopincr);
					{
						// increment str1 and str2
						fir::Value* v1 = this->irb.CreatePointerAdd(str1, fir::ConstantInt::getInt64(1));
						fir::Value* v2 = this->irb.CreatePointerAdd(str2, fir::ConstantInt::getInt64(1));

						this->irb.CreateStore(v1, str1p);
						this->irb.CreateStore(v2, str2p);

						this->irb.CreateUnCondBranch(loopcond);
					}
				}

				this->irb.setCurrentBlock(merge);
				fir::Value* ret = this->irb.CreateSub(this->irb.CreateLoad(this->irb.CreateLoad(str1p)),
					this->irb.CreateLoad(this->irb.CreateLoad(str2p)));

				ret = this->irb.CreateIntSizeCast(ret, func->getReturnType());

				this->irb.CreateReturn(ret);
			}

			cmpf = func;
			this->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}






	fir::Function* CodegenInstance::getStringRefCountIncrementFunction()
	{
		fir::Function* incrf = this->module->getFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name));

		if(!incrf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* getref = this->irb.addNewBlockInFunction("getref", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);
			this->irb.setCurrentBlock(entry);

			// if ptr is 0, we exit early.
			{
				fir::Value* ptr = this->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = this->irb.CreateICmpEQ(ptr, fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));

				this->irb.CreateCondBranch(cond, merge, getref);
			}


			this->irb.setCurrentBlock(getref);
			fir::Value* curRc = this->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// never increment the refcount if this is a string literal
			// how do we know? the refcount was -1 to begin with.

			// check.
			fir::IRBlock* doadd = this->irb.addNewBlockInFunction("doref", func);
			{
				fir::Value* cond = this->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				this->irb.CreateCondBranch(cond, merge, doadd);
			}

			this->irb.setCurrentBlock(doadd);
			fir::Value* newRc = this->irb.CreateAdd(curRc, fir::ConstantInt::getInt64(1));
			this->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_ARC
			{
				fir::Value* tmpstr = this->module->createGlobalString("(incr) new rc of %p (%s) = %d\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				auto bufp = this->irb.CreateGetStringData(func->getArguments()[0]);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, bufp, bufp,
					newRc });
			}
			#endif

			this->irb.CreateUnCondBranch(merge);
			this->irb.setCurrentBlock(merge);
			this->irb.CreateReturnVoid();

			this->irb.setCurrentBlock(restore);

			incrf = func;
		}

		iceAssert(incrf);
		return incrf;
	}


	fir::Function* CodegenInstance::getStringRefCountDecrementFunction()
	{
		fir::Function* decrf = this->module->getFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name));

		if(!decrf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* checkneg = this->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* dotest = this->irb.addNewBlockInFunction("dotest", func);
			fir::IRBlock* dealloc = this->irb.addNewBlockInFunction("deallocate", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);


			// note:
			// if the ptr is 0, we exit immediately
			// if the refcount is -1, we exit as well.


			this->irb.setCurrentBlock(entry);
			{
				fir::Value* ptr = this->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = this->irb.CreateICmpEQ(ptr, fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));

				this->irb.CreateCondBranch(cond, merge, checkneg);
			}


			// needs to handle freeing the thing.
			this->irb.setCurrentBlock(checkneg);
			fir::Value* curRc = this->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// check.
			{
				fir::Value* cond = this->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				this->irb.CreateCondBranch(cond, merge, dotest);
			}


			this->irb.setCurrentBlock(dotest);
			fir::Value* newRc = this->irb.CreateSub(curRc, fir::ConstantInt::getInt64(1));
			this->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_ARC
			{
				fir::Value* tmpstr = this->module->createGlobalString("(decr) new rc of %p (%s) = %d\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				auto bufp = this->irb.CreateGetStringData(func->getArguments()[0]);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, bufp, bufp,
					newRc });
			}
			#endif

			{
				fir::Value* cond = this->irb.CreateICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				this->irb.CreateCondBranch(cond, dealloc, merge);

				this->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = this->irb.CreateGetStringData(func->getArguments()[0]);


				#if DEBUG_ARC
				{
					fir::Value* tmpstr = this->module->createGlobalString("free %p (%s)\n");
					tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);
					this->irb.CreateCall3(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), tmpstr,
						bufp, bufp);
				}
				#endif



				fir::Function* freefn = this->module->getFunction(this->getOrDeclareLibCFunc("free")->getName());
				iceAssert(freefn);

				this->irb.CreateCall1(freefn, this->irb.CreatePointerAdd(bufp, fir::ConstantInt::getInt64(-8)));

				this->irb.CreateSetStringData(func->getArguments()[0], fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));
				this->irb.CreateUnCondBranch(merge);
			}

			this->irb.setCurrentBlock(merge);
			this->irb.CreateReturnVoid();

			this->irb.setCurrentBlock(restore);

			decrf = func;
		}

		iceAssert(decrf);
		return decrf;
	}




	fir::Function* CodegenInstance::getStringBoundsCheckFunction()
	{
		fir::Function* fn = this->module->getFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = this->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

			this->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* len = this->irb.CreateGetStringLength(arg1);
			fir::Value* res = this->irb.CreateICmpGEQ(arg2, len);

			this->irb.CreateCondBranch(res, failb, merge);
			this->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = this->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = this->module->createGlobalString("w");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = this->module->createGlobalString("Tried to index string at index '%zu'; length is only '%zu'! (max index is thus '%zu')\n");
				fmtstr = this->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				this->irb.CreateCall(fprintfn, { err, fmtstr, arg2, len, this->irb.CreateSub(len, fir::ConstantInt::getInt64(1)) });

				this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
				this->irb.CreateUnreachable();
			}

			this->irb.setCurrentBlock(merge);
			{
				this->irb.CreateReturnVoid();
			}

			fn = func;


			this->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






	fir::Function* CodegenInstance::getStringCheckLiteralWriteFunction()
	{
		fir::Function* fn = this->module->getFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = this->irb.getCurrentBlock();


			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = this->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

			this->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* rc = this->irb.CreateGetStringRefCount(arg1);
			fir::Value* res = this->irb.CreateICmpLT(rc, fir::ConstantInt::getInt64(0));

			this->irb.CreateCondBranch(res, failb, merge);
			this->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()),
					fir::LinkageType::External);

				fir::Function* fdopenf = this->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = this->module->createGlobalString("w");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = this->module->createGlobalString("Tried to write to immutable string literal '%s' at index '%zu'!\n");
				fmtstr = this->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				fir::Value* dp = this->irb.CreateGetStringData(arg1);
				this->irb.CreateCall(fprintfn, { err, fmtstr, dp, arg2 });

				this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
				this->irb.CreateUnreachable();
			}

			this->irb.setCurrentBlock(merge);
			{
				this->irb.CreateReturnVoid();
			}

			fn = func;


			this->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






































	fir::Function* CodegenInstance::getArrayBoundsCheckFunction()
	{
		fir::Function* fn = this->module->getFunction(Identifier(BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = this->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

			this->irb.setCurrentBlock(entry);

			fir::Value* max = func->getArguments()[0];
			fir::Value* len = func->getArguments()[1];

			fir::Value* res = this->irb.CreateICmpGEQ(len, max);

			this->irb.CreateCondBranch(res, failb, merge);
			this->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = this->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = this->module->createGlobalString("w");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = this->module->createGlobalString("Tried to index array at index '%zu'; length is only '%zu'! (max index is thus '%zu')\n");
				fmtstr = this->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				this->irb.CreateCall(fprintfn, { err, fmtstr, len, max, this->irb.CreateSub(max, fir::ConstantInt::getInt64(1)) });

				this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
				this->irb.CreateUnreachable();
			}

			this->irb.setCurrentBlock(merge);
			{
				this->irb.CreateReturnVoid();
			}

			fn = func;

			this->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}
}




















