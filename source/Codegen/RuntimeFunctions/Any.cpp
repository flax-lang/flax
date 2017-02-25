// Any.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

using namespace Codegen;
using namespace Ast;

#define BUILTIN_ANY_INCR_FUNC_NAME			"__anyref_incr"
#define BUILTIN_ANY_DECR_FUNC_NAME			"__anyref_decr"


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



				fir::Value* cond = cgi->irb.CreateICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(cond, dealloc, merge);

				cgi->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = cgi->irb.CreatePointerTypeCast(rcptr, fir::Type::getInt8Ptr());

				fir::Function* freefn = cgi->getOrDeclareLibCFunc(FREE_MEMORY_FUNC);
				iceAssert(freefn);

				// bufp is already at the rc.
				cgi->irb.CreateCall1(freefn, bufp);
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
}
}
}











