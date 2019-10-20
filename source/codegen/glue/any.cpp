// any.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.


#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

namespace cgn {
namespace glue {
namespace any
{
	static void _doRefCount(CodegenState* cs, fir::Function* func, bool decrement)
	{
		auto any = func->getArguments()[0];
		auto rcp = cs->irb.GetAnyRefCountPointer(any, "rcp");

		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);
		fir::IRBlock* dorc = cs->irb.addNewBlockInFunction("dorc", func);

		cs->irb.CondBranch(cs->irb.ICmpEQ(rcp, fir::ConstantValue::getZeroValue(fir::Type::getNativeWordPtr())),
			merge, dorc);

		cs->irb.setCurrentBlock(dorc);
		{
			auto oldrc = cs->irb.ReadPtr(rcp, "oldrc");
			auto newrc = cs->irb.Add(oldrc, fir::ConstantInt::getNative(decrement ? -1 : 1));

			cs->irb.SetAnyRefCount(any, newrc);

			#if DEBUG_ANY_REFCOUNTING
			{
				std::string x = decrement ? "(decr)" : "(incr)";

				cs->printIRDebugMessage("*    ANY: " + x + " - new rc of: (rcptr: %p) = %d",
					{ cs->irb.GetAnyRefCountPointer(any), cs->irb.GetAnyRefCount(any) });
			}
			#endif

			if(decrement)
			{
				fir::IRBlock* dofree = cs->irb.addNewBlockInFunction("dofree", func);

				auto cond = cs->irb.ICmpEQ(newrc, fir::ConstantInt::getNative(0));

				// this thing checks for the MSB of the typeid; if it's set, means we used heap memory and so we need to free.
				cond = cs->irb.BitwiseAND(cond,
					cs->irb.ICmpGT(cs->irb.BitwiseAND(cs->irb.GetAnyTypeID(any), fir::ConstantInt::getUNative(BUILTIN_ANY_FLAG_MASK)),
					fir::ConstantInt::getUNative(0)));

				cs->irb.CondBranch(cond, dofree, merge);

				cs->irb.setCurrentBlock(dofree);
				{
					auto freefn = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
					iceAssert(freefn);

					//* because llvm is a little restrictive wrt. how we can fiddle with types and memory, in
					//* order to cast the data buffer (which is an array) to a i64*, we first get the array,
					//* then make a stack alloc, store the array value, use the alloc as an address and cast it
					//* to i64*, then dereference that to get the actual pointer to the heap memory.

					// 1. this gets us a memory location we can use.
					auto _buf = cs->irb.GetAnyData(any, "buf");
					auto buf = cs->irb.StackAlloc(_buf->getType());
					cs->irb.WritePtr(_buf, buf);

					// 2. 'buf' is a pointer to the array itself -- we cast it to i64*, so the dereference
					//    gives us the first 8 bytes of the data buffer.
					buf = cs->irb.PointerTypeCast(buf, fir::Type::getNativeWordPtr());

					// 3. this is the dereference.
					auto ptr = cs->irb.ReadPtr(buf);

					// 4. the first 8 bytes are actually a pointer to the heap memory.
					ptr = cs->irb.IntToPointerCast(ptr, fir::Type::getMutInt8Ptr());

					cs->irb.Call(freefn, ptr);
					cs->irb.Call(freefn, cs->irb.PointerTypeCast(cs->irb.GetAnyRefCountPointer(any), fir::Type::getMutInt8Ptr()));

					#if DEBUG_ANY_ALLOCATION
					{
						cs->printIRDebugMessage("*    ANY: free(): (ptr: %p / rcp: %p)", {
							ptr, cs->irb.GetAnyRefCountPointer(any) });
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
		auto fname = misc::getIncrRefcount_FName(fir::Type::getAny());
		fir::Function* retfn = cs->module->getFunction(fname);

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getAny() }, fir::Type::getVoid()), fir::LinkageType::Internal);

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
		auto fname = misc::getDecrRefcount_FName(fir::Type::getAny());
		fir::Function* retfn = cs->module->getFunction(fname);

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getAny() }, fir::Type::getVoid()), fir::LinkageType::Internal);

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



	fir::Function* generateCreateAnyWithValueFunction(CodegenState* cs, fir::Type* type)
	{
		auto fname = misc::getCreateAnyOf_FName(type);
		fir::Function* retfn = cs->module->getFunction(fname);

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ type }, fir::Type::getAny()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);


			auto any = cs->irb.CreateValue(fir::Type::getAny());
			auto dataarrty = fir::ArrayType::get(fir::Type::getInt8(), BUILTIN_ANY_DATA_BYTECOUNT);

			// make the refcount pointer.
			auto rcp = cs->irb.PointerTypeCast(cs->irb.Call(cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC),
				fir::ConstantInt::getNative(REFCOUNT_SIZE)), fir::Type::getNativeWordPtr());

			any = cs->irb.SetAnyRefCountPointer(any, rcp);
			cs->irb.SetAnyRefCount(any, fir::ConstantInt::getNative(1));

			size_t tid = type->getID();
			if(auto typesz = fir::getSizeOfType(type); typesz > BUILTIN_ANY_DATA_BYTECOUNT)
			{
				tid |= BUILTIN_ANY_FLAG_MASK;

				auto ptr = cs->irb.PointerTypeCast(cs->irb.Call(cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC),
					fir::ConstantInt::getNative(typesz)), type->getMutablePointerTo());

				#if DEBUG_ANY_ALLOCATION
				{
					cs->printIRDebugMessage("*    ANY: alloc(): (id: %lu, ptr: %p / rcp: %p)", {
						fir::ConstantInt::getUNative(tid), ptr, rcp });
				}
				#endif

				cs->irb.WritePtr(func->getArguments()[0], ptr);
				ptr = cs->irb.PointerToIntCast(ptr, fir::Type::getNativeWord());

				// now, we make a fake data, and then store it.
				auto arrptr = cs->irb.StackAlloc(dataarrty);
				auto fakeptr = cs->irb.PointerTypeCast(arrptr, fir::Type::getNativeWordPtr()->getMutablePointerVersion());
				cs->irb.WritePtr(ptr, fakeptr);

				auto arr = cs->irb.ReadPtr(arrptr);
				any = cs->irb.SetAnyData(any, arr);
			}
			else
			{
				auto arrptr = cs->irb.StackAlloc(dataarrty);
				auto fakeptr = cs->irb.PointerTypeCast(arrptr, type->getMutablePointerTo());
				cs->irb.WritePtr(func->getArguments()[0], fakeptr);

				auto arr = cs->irb.ReadPtr(arrptr);
				any = cs->irb.SetAnyData(any, arr);


			}

			any = cs->irb.SetAnyTypeID(any, fir::ConstantInt::getUNative(tid));

			cs->irb.Return(any);

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}

	fir::Function* generateGetValueFromAnyFunction(CodegenState* cs, fir::Type* type)
	{
		auto fname = misc::getGetValueFromAny_FName(type);
		fir::Function* retfn = cs->module->getFunction(fname);

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(fname,
				fir::FunctionType::get({ fir::Type::getAny() }, type), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			auto any = func->getArguments()[0];

			auto dataarrty = fir::ArrayType::get(fir::Type::getInt8(), BUILTIN_ANY_DATA_BYTECOUNT);
			auto tid = cs->irb.BitwiseAND(cs->irb.GetAnyTypeID(any), fir::ConstantInt::getUNative(~BUILTIN_ANY_FLAG_MASK));

			fir::IRBlock* invalid = cs->irb.addNewBlockInFunction("invalid", cs->irb.getCurrentFunction());
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", cs->irb.getCurrentFunction());

			auto valid = cs->irb.ICmpEQ(tid, fir::ConstantInt::getUNative(type->getID()));
			cs->irb.CondBranch(valid, merge, invalid);

			cs->irb.setCurrentBlock(invalid);
			{
				printRuntimeError(cs, fir::ConstantCharSlice::get(cs->loc().toString()),
					"invalid unwrap of 'any' with type id '%ld' into type '%s' (with id '%ld')",
					{ tid, cs->module->createGlobalString(type->str()), fir::ConstantInt::getUNative(type->getID()) }
				);
			}

			cs->irb.setCurrentBlock(merge);
			{
				if(fir::getSizeOfType(type) > BUILTIN_ANY_DATA_BYTECOUNT)
				{
					// same as above, but in reverse.
					auto arrptr = cs->irb.StackAlloc(dataarrty);
					cs->irb.WritePtr(cs->irb.GetAnyData(any), arrptr);

					// cast the array* into a type**, so when we dereference it, we get the first 8 bytes interpreted as a type*.
					// we then just load and return that.
					auto fakeptr = cs->irb.PointerTypeCast(arrptr, type->getMutablePointerTo()->getMutablePointerTo());
					auto typeptr = cs->irb.ReadPtr(fakeptr);

					cs->irb.Return(cs->irb.ReadPtr(typeptr));
				}
				else
				{
					auto arrptr = cs->irb.StackAlloc(dataarrty);
					cs->irb.WritePtr(cs->irb.GetAnyData(any), arrptr);

					// same as above but we skip a load.
					auto fakeptr = cs->irb.PointerTypeCast(arrptr, type->getMutablePointerTo());
					auto ret = cs->irb.ReadPtr(fakeptr);

					cs->irb.Return(ret);
				}
			}


			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}
}
}
}





















