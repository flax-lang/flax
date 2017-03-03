// Any.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

#include <algorithm>

using namespace Codegen;
using namespace Ast;

#define BUILTIN_ANY_INCR_FUNC_NAME			"__anyref_incr"
#define BUILTIN_ANY_DECR_FUNC_NAME			"__anyref_decr"

#define DEBUG_MASTER		1
#define DEBUG_REFCOUNTING	(1 && DEBUG_MASTER)

namespace Codegen {
namespace RuntimeFuncs {
namespace Any
{
	fir::Function* getRefCountIncrementFunction(CodegenInstance* cgi)
	{
		fir::Function* incrf = cgi->module->getFunction(Identifier(BUILTIN_ANY_INCR_FUNC_NAME, IdKind::Name));

		if(!incrf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_ANY_INCR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getAnyType() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);
			cgi->irb.setCurrentBlock(entry);

			fir::Value* ptr = cgi->irb.CreateImmutStackAlloc(fir::Type::getAnyType(), func->getArguments()[0]);
			fir::Value* flag = cgi->irb.CreateGetAnyFlag(ptr);

			// never increment the refcount if this is a string literal
			// how do we know? the refcount was -1 to begin with.

			// check.
			fir::IRBlock* doadd = cgi->irb.addNewBlockInFunction("doref", func);
			{
				fir::Value* cond = cgi->irb.CreateICmpLT(flag, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, merge, doadd);
			}

			cgi->irb.setCurrentBlock(doadd);
			{
				// the refcount is, as usual, stored in the heap
				// we never really need to access the refcount of any, so it's not a proper instruction
				// do it manually here

				// the thing is, the `data` we get from the any is a **pointer to the array**.
				// so first, we must convert that i8* into an i64*, load that i64*, to get the actual **pointer value** that points
				// to some memory on the heap.

				// then we use inttoptr to convert that i64 we loaded into an i64*, to get the actual pointer to heap data.

				fir::Value* rcptr = cgi->irb.CreateGetAnyData(ptr);
				rcptr = cgi->irb.CreatePointerTypeCast(rcptr, cgi->module->getExecutionTarget()->getPointerSizedIntegerType()->getPointerTo());

				// load the actual pointer, now this is a pointer
				rcptr = cgi->irb.CreateLoad(rcptr);

				// inttoptr
				rcptr = cgi->irb.CreateIntToPointerCast(rcptr, fir::Type::getInt64Ptr());

				fir::IRBlock* doit = cgi->irb.addNewBlockInFunction("actuallydo", func);

				// if ptr is 0, we exit
				{
					fir::Value* cond = cgi->irb.CreateICmpEQ(rcptr, fir::ConstantValue::getNullValue(fir::Type::getInt64Ptr()));
					cgi->irb.CreateCondBranch(cond, merge, doit);
				}



				cgi->irb.setCurrentBlock(doit);

				// now sub, and stuff.
				rcptr = cgi->irb.CreatePointerSub(rcptr, fir::ConstantInt::getInt64(1));
				fir::Value* curRc = cgi->irb.CreateLoad(rcptr);

				fir::Value* newRc = cgi->irb.CreateAdd(curRc, fir::ConstantInt::getInt64(1));
				cgi->irb.CreateStore(newRc, rcptr);

				#if DEBUG_REFCOUNTING
				{
					fir::Value* tmpstr = cgi->module->createGlobalString("(incr any) new rc of %p = %d\n");
					cgi->irb.CreateCall(cgi->getOrDeclareLibCFunc("printf"), { tmpstr, rcptr, newRc });
				}
				#endif
			}

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
		fir::Function* decrf = cgi->module->getFunction(Identifier(BUILTIN_ANY_DECR_FUNC_NAME, IdKind::Name));

		if(!decrf)
		{
			auto restore = cgi->irb.getCurrentBlock();

			fir::Function* func = cgi->module->getOrCreateFunction(Identifier(BUILTIN_ANY_DECR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getAnyType() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* dotest = cgi->irb.addNewBlockInFunction("dotest", func);
			fir::IRBlock* doit = cgi->irb.addNewBlockInFunction("doit", func);
			fir::IRBlock* dealloc = cgi->irb.addNewBlockInFunction("deallocate", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);


			// note:
			// if the ptr is 0, we exit immediately
			// if the refcount is -1, we exit as well.


			cgi->irb.setCurrentBlock(entry);

			// needs to handle freeing the thing.
			fir::Value* ptr = cgi->irb.CreateImmutStackAlloc(fir::Type::getAnyType(), func->getArguments()[0]);
			fir::Value* flag = cgi->irb.CreateGetAnyFlag(ptr);

			// check.
			{
				fir::Value* cond = cgi->irb.CreateICmpLT(flag, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, merge, dotest);
			}



			cgi->irb.setCurrentBlock(dotest);
			{
				// see above comments, same thing as increment
				fir::Value* rcptr = cgi->irb.CreateGetAnyData(ptr);
				rcptr = cgi->irb.CreatePointerTypeCast(rcptr, cgi->module->getExecutionTarget()->getPointerSizedIntegerType()->getPointerTo());

				// load the actual pointer, now this is a pointer
				rcptr = cgi->irb.CreateLoad(rcptr);

				// inttoptr
				rcptr = cgi->irb.CreateIntToPointerCast(rcptr, fir::Type::getInt64Ptr());


				// if ptr is 0, we exit
				{
					fir::Value* cond = cgi->irb.CreateICmpEQ(rcptr, fir::ConstantValue::getNullValue(fir::Type::getInt64Ptr()));
					cgi->irb.CreateCondBranch(cond, merge, doit);
				}



				cgi->irb.setCurrentBlock(doit);

				// now sub, and stuff.
				rcptr = cgi->irb.CreatePointerSub(rcptr, fir::ConstantInt::getInt64(1));
				fir::Value* curRc = cgi->irb.CreateLoad(rcptr);

				fir::Value* newRc = cgi->irb.CreateSub(curRc, fir::ConstantInt::getInt64(1));
				cgi->irb.CreateStore(newRc, rcptr);


				#if DEBUG_REFCOUNTING
				{
					fir::Value* tmpstr = cgi->module->createGlobalString("(decr any) new rc of %p = %d\n");
					cgi->irb.CreateCall(cgi->getOrDeclareLibCFunc("printf"), { tmpstr, rcptr, newRc });
				}
				#endif


				fir::Value* cond = cgi->irb.CreateICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, dealloc, merge);

				cgi->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = cgi->irb.CreatePointerTypeCast(rcptr, fir::Type::getInt8Ptr());

				fir::Function* freefn = cgi->getOrDeclareLibCFunc(FREE_MEMORY_FUNC);
				iceAssert(freefn);

				// bufp is already at the rc.
				cgi->irb.CreateCall1(freefn, bufp);

				#if DEBUG_REFCOUNTING
				{
					fir::Value* tmpstr = cgi->module->createGlobalString("(free any) %p\n");
					cgi->irb.CreateCall(cgi->getOrDeclareLibCFunc("printf"), { tmpstr, rcptr });
				}
				#endif
			}


			cgi->irb.CreateUnCondBranch(merge);

			cgi->irb.setCurrentBlock(merge);
			cgi->irb.CreateReturnVoid();

			cgi->irb.setCurrentBlock(restore);

			decrf = func;
		}

		iceAssert(decrf);
		return decrf;
	}
}
}





	Result_t CodegenInstance::assignValueToAny(Expr* user, fir::Value* any, fir::Value* val, fir::Value* ptr, ValueKind vk)
	{
		size_t sz = this->module->getExecutionTarget()->getTypeSizeInBytes(val->getType());
		bool isSmall = (sz != -1 && sz <= 24);

		iceAssert(any);
		iceAssert(any->getType()->isPointerType() && any->getType()->getPointerElementType()->isAnyType());

		if(!ptr) ptr = this->getImmutStackAllocValue(val);


		if(isSmall)
		{
			// set flag = -1, and the id
			this->irb.CreateSetAnyFlag(any, fir::ConstantInt::getInt64(-1));
			this->irb.CreateSetAnyTypeID(any, fir::ConstantInt::getInt64(val->getType()->getID()));

			// get the data pointer
			fir::Value* data = this->irb.CreateGetAnyData(any);

			// cast it appropriately.
			data = this->irb.CreatePointerTypeCast(data, val->getType()->getPointerTo());

			// store it.
			this->performComplexValueStore(user, val->getType(), ptr, data, "_", user ? user->pin : Parser::Pin(), vk);

			// that's it.
			return Result_t(0, 0);
		}
		else
		{
			// set flag = 1, and id
			this->irb.CreateSetAnyFlag(any, fir::ConstantInt::getInt64(1));
			this->irb.CreateSetAnyTypeID(any, fir::ConstantInt::getInt64(val->getType()->getID()));

			// call malloc with the size + 8
			size_t i64size = 8;

			// use runtime sizeof
			fir::Value* tysz = this->irb.CreateSizeof(val->getType());
			tysz = this->irb.CreateAdd(tysz, fir::ConstantInt::getInt64(i64size));

			// malloc
			auto allocfn = this->getOrDeclareLibCFunc(ALLOCATE_MEMORY_FUNC);
			iceAssert(allocfn);

			// call
			fir::Value* alloc = this->irb.CreateCall1(allocfn, tysz);

			// set refcount = 1
			fir::Value* rcptr = this->irb.CreatePointerTypeCast(alloc, fir::Type::getInt64Ptr());
			this->irb.CreateStore(fir::ConstantInt::getInt64(1), rcptr);

			// increment
			fir::Value* dataptr = this->irb.CreatePointerAdd(rcptr, fir::ConstantInt::getInt64(1));

			// memcpy
			dataptr = this->irb.CreatePointerTypeCast(dataptr, val->getType()->getPointerTo());
			this->performComplexValueStore(user, val->getType(), ptr, dataptr, "_", user ? user->pin : Parser::Pin(), vk);


			// store the pointer into the thing
			this->irb.CreateSetAnyData(any, dataptr);

			if(std::find(this->getRefCountedValues().begin(), this->getRefCountedValues().end(), any) == this->getRefCountedValues().end())
				this->addRefCountedValue(any);

			return Result_t(0, 0);
		}
	}

	Result_t CodegenInstance::extractValueFromAny(Expr* user, fir::Value* any, fir::Type* type)
	{
		size_t sz = this->module->getExecutionTarget()->getTypeSizeInBytes(type);
		bool isSmall = (sz != -1 && sz <= 24);

		iceAssert(any);
		iceAssert(any->getType()->isPointerType() && any->getType()->getPointerElementType()->isAnyType());

		fir::Function* fprintfn = this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
			fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
			fir::Type::getInt32()), fir::LinkageType::External);

		fir::Function* fdopenf = this->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
			fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
			fir::LinkageType::External);


		// runtime check
		// make sure that if it's small, flag is -1
		{
			fir::Value* flag = this->irb.CreateGetAnyFlag(any);
			fir::IRBlock* fail = this->irb.addNewBlockInFunction("fail", this->irb.getCurrentFunction());
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", this->irb.getCurrentFunction());


			if(isSmall)
			{
				fir::Value* cond = this->irb.CreateICmpEQ(flag, fir::ConstantInt::getInt64(-1));
				this->irb.CreateCondBranch(cond, merge, fail);

				this->irb.setCurrentBlock(fail);
				{
					// basically:
					// void* stderr = fdopen(2, "w")
					// fprintf(stderr, "", bla bla)

					fir::Value* tmpstr = this->module->createGlobalString("w");
					fir::Value* fmtstr = this->module->createGlobalString("%s: Expected trivial type (<= 24 bytes) in any, found non-trival type (flag: %d, not -1)\n");

					fir::Value* posstr = this->module->createGlobalString(Parser::pinToString(user->pin));

					fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

					this->irb.CreateCall(fprintfn, { err, fmtstr, posstr, flag });

					this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
					this->irb.CreateUnreachable();
				}
			}
			else
			{
				fir::Value* cond = this->irb.CreateICmpNEQ(flag, fir::ConstantInt::getInt64(-1));
				this->irb.CreateCondBranch(cond, merge, fail);

				this->irb.setCurrentBlock(fail);
				{
					fir::Value* tmpstr = this->module->createGlobalString("w");
					fir::Value* fmtstr = this->module->createGlobalString("%s: Expected non-trivial type (> 24 bytes) in any, found trivial type (flag: %d)\n");

					fir::Value* posstr = this->module->createGlobalString(Parser::pinToString(user->pin));

					fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

					this->irb.CreateCall(fprintfn, { err, fmtstr, posstr, flag });

					this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
					this->irb.CreateUnreachable();
				}
			}

			this->irb.setCurrentBlock(merge);
		}


		// check the typeid
		{
			fir::Value* tid = this->irb.CreateGetAnyTypeID(any);

			fir::IRBlock* fail = this->irb.addNewBlockInFunction("invalidcast", this->irb.getCurrentFunction());
			fir::IRBlock* success = this->irb.addNewBlockInFunction("validcast", this->irb.getCurrentFunction());

			// compare
			fir::Value* cond = this->irb.CreateICmpEQ(tid, fir::ConstantInt::getInt64(type->getID()));
			this->irb.CreateCondBranch(cond, success, fail);

			this->irb.setCurrentBlock(fail);
			{
				fir::Value* tmpstr = this->module->createGlobalString("w");
				fir::Value* fmtstr = this->module->createGlobalString("%s: Invalid any type coercion (from id %zu to %zu)\n");
				fir::Value* posstr = this->module->createGlobalString(Parser::pinToString(user->pin));

				fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				this->irb.CreateCall(fprintfn, { err, fmtstr, posstr, tid, fir::ConstantInt::getInt64(type->getID()) });

				this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
				this->irb.CreateUnreachable();
			}


			this->irb.setCurrentBlock(success);
		}



		// ok can
		if(isSmall)
		{
			// get the data
			fir::Value* data = this->irb.CreateGetAnyData(any);

			// cast it to the proper type
			fir::Value* ptr = this->irb.CreatePointerTypeCast(data, type->getPointerTo());

			// load it, and return it.
			return Result_t(this->irb.CreateLoad(ptr), ptr);
		}
		else
		{
			// get the data
			fir::Value* data = this->irb.CreateGetAnyData(any);

			// cast it to the proper type
			fir::Value* ptrptr = this->irb.CreatePointerTypeCast(data,
				this->module->getExecutionTarget()->getPointerSizedIntegerType()->getPointerTo());

			// get the pointer
			fir::Value* ptr = this->irb.CreateLoad(ptrptr);

			// ptr is an int, so make it actually a pointer
			ptr = this->irb.CreateIntToPointerCast(ptr, type->getPointerTo());

			// load it
			return Result_t(this->irb.CreateLoad(ptr), ptr);
		}
	}

	Result_t CodegenInstance::makeAnyFromValue(Expr* user, fir::Value* val, fir::Value* ptr, ValueKind vk)
	{
		// check if we can fit the thing into ptrsize bytes
		fir::Value* ai = this->irb.CreateStackAlloc(fir::Type::getAnyType());
		if(!ptr) ptr = this->getImmutStackAllocValue(val);

		// heh.
		this->assignValueToAny(user, ai, val, ptr, vk);
		return Result_t(this->irb.CreateLoad(ai), ai);
	}
}













