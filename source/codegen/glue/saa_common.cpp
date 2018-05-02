// saa_common.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

// namespace cgn::glue::saa_common
namespace cgn {
namespace glue {
namespace saa_common
{
	static inline bool isSAA(fir::Type* t) { return t->isStringType() || t->isDynamicArrayType(); }
	static inline fir::Type* getSAAElm(fir::Type* t) { iceAssert(isSAA(t)); return (t->isStringType() ? fir::Type::getInt8() : t->getArrayElementType()); }
	static inline fir::Type* getSAASlice(fir::Type* t, bool mut = true) { iceAssert(isSAA(t)); return fir::ArraySliceType::get(getSAAElm(t), mut); }
	static inline fir::ConstantInt* getCI(int64_t i) { return fir::ConstantInt::getInt64(i); }

	static fir::Value* castRawBufToElmPtr(CodegenState* cs, fir::Type* saa, fir::Value* buf)
	{
		auto ptrty = getSAAElm(saa)->getMutablePointerTo();

		if(buf->getType()->isPointerType())
			return cs->irb.PointerTypeCast(buf, ptrty);

		else
			return cs->irb.IntToPointerCast(buf, ptrty);
	}

	static fir::Function* generateIncrementArrayRefCountInLoopFunction(CodegenState* cs, fir::Type* elm)
	{
		iceAssert(cs->isRefCountedType(elm));

		auto fname = "__loop_incr_rc_" + elm->str();
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ elm->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::IRBlock* cond = cs->irb.addNewBlockInFunction("loopCond", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("loopBody", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			fir::Value* ctrPtr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), ctrPtr);

			fir::Value* s2ptr = func->getArguments()[0];
			fir::Value* s2len = func->getArguments()[1];

			cs->irb.UnCondBranch(cond);
			cs->irb.setCurrentBlock(cond);
			{
				// check the condition
				fir::Value* ctr = cs->irb.Load(ctrPtr);
				fir::Value* res = cs->irb.ICmpLT(ctr, s2len);

				cs->irb.CondBranch(res, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				// increment refcount
				fir::Value* val = cs->irb.Load(cs->irb.PointerAdd(s2ptr, cs->irb.Load(ctrPtr)));

				cs->incrementRefCount(val);

				// increment counter
				cs->irb.Store(cs->irb.Add(fir::ConstantInt::getInt64(1), cs->irb.Load(ctrPtr)), ctrPtr);
				cs->irb.UnCondBranch(cond);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();


			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}

	fir::Value* initSAAWithRefCount(CodegenState* cs, fir::Value* saa, fir::Value* rc)
	{
		iceAssert(isSAA(saa->getType()) && "not saa type");
		iceAssert(rc->getType()->isIntegerType() && "not integer type");

		auto rcp = cs->irb.Call(cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC), getCI(REFCOUNT_SIZE));
		rcp = cs->irb.PointerTypeCast(rcp, fir::Type::getInt64Ptr());

		auto ret = cs->irb.SetSAARefCountPointer(saa, rcp);
		cs->irb.SetSAARefCount(ret, rc);

		return ret;
	}







	/*
		* NOTE *

		since we're changing strings and dynamic arrays to behave much the same way, why not just collapse the runtime gluecode as much
		as possible.

		we're going with the { ptr, len, cap, rcp } structure for both types, and so we can do a lot of things commonly. one thing is that
		we still want null terminators on strings, so that's just a couple of if-checks sprinkled around -- nothing too obnoxious.
	 */


	static void _callCloneFunctionInLoop(CodegenState* cs, fir::Function* curfunc, fir::Function* fn,
		fir::Value* ptr, fir::Value* len, fir::Value* newptr, fir::Value* startIndex)
	{
		fir::IRBlock* loopcond = cs->irb.addNewBlockInFunction("loopcond", curfunc);
		fir::IRBlock* loopbody = cs->irb.addNewBlockInFunction("loopbody", curfunc);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", curfunc);

		fir::Value* counter = cs->irb.StackAlloc(fir::Type::getInt64());
		cs->irb.Store(startIndex, counter);

		cs->irb.UnCondBranch(loopcond);
		cs->irb.setCurrentBlock(loopcond);
		{
			fir::Value* res = cs->irb.ICmpEQ(cs->irb.Load(counter), len);
			cs->irb.CondBranch(res, merge, loopbody);
		}

		cs->irb.setCurrentBlock(loopbody);
		{
			// make clone
			fir::Value* origElm = cs->irb.PointerAdd(ptr, cs->irb.Load(counter));
			fir::Value* clone = 0;

			//* note: the '0' argument specifies the offset to clone from -- since want the whole thing, the offset is 0.
			clone = cs->irb.Call(fn, cs->irb.Load(origElm), fir::ConstantInt::getInt64(0));

			// store clone
			fir::Value* newElm = cs->irb.PointerAdd(newptr, cs->irb.Load(counter));
			cs->irb.Store(clone, newElm);

			// increment counter
			cs->irb.Store(cs->irb.Add(cs->irb.Load(counter), fir::ConstantInt::getInt64(1)), counter);
			cs->irb.UnCondBranch(loopcond);
		}

		cs->irb.setCurrentBlock(merge);
	}

	static void _handleCallingAppropriateCloneFunction(CodegenState* cs, fir::Function* func, fir::Type* elmType, fir::Value* oldptr,
		fir::Value* newptr, fir::Value* oldlen, fir::Value* bytecount, fir::Value* startIndex)
	{
		if(elmType->isPrimitiveType() || elmType->isCharType() || elmType->isEnumType())
		{
			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			#if DEBUG_ARRAY_ALLOCATION
			{
				fir::Function* printfn = cs->getOrDeclareLibCFunction("printf");

				fir::Value* tmpstr = cs->module->createGlobalString("oldptr: %p, newptr: %p, oldlen: %d, bytecount: %d, index: %d\n");
				cs->irb.Call(printfn, { tmpstr, oldptr, newptr, oldlen, bytecount, startIndex });
			}
			#endif

			cs->irb.Call(memcpyf, { newptr, cs->irb.PointerTypeCast(cs->irb.PointerAdd(oldptr,
				startIndex), fir::Type::getMutInt8Ptr()), bytecount, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });
		}
		else if(elmType->isDynamicArrayType())
		{
			// yo dawg i heard you like arrays...
			fir::Function* clonef = generateCloneFunction(cs, elmType);
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.PointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, oldptr, oldlen, cloneptr, startIndex);
		}
		else if(elmType->isArraySliceType())
		{
			// yo dawg i heard you like arrays...
			fir::Function* clonef = generateCloneFunction(cs, elmType);
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.PointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, oldptr, oldlen, cloneptr, startIndex);
		}
		else if(elmType->isStringType())
		{
			fir::Function* clonef = glue::string::getCloneFunction(cs);
			iceAssert(clonef);

			// loop
			fir::Value* cloneptr = cs->irb.PointerTypeCast(newptr, elmType->getPointerTo());
			_callCloneFunctionInLoop(cs, func, clonef, oldptr, oldlen, cloneptr, startIndex);
		}
		else if(elmType->isStructType() || elmType->isClassType() || elmType->isTupleType() || elmType->isArrayType())
		{
			// todo: call copy constructors and stuff

			fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

			cs->irb.Call(memcpyf, { newptr, cs->irb.PointerTypeCast(cs->irb.PointerAdd(oldptr,
				startIndex), fir::Type::getMutInt8Ptr()), bytecount, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });
		}
		else
		{
			error("unsupported element type '%s' for array clone", elmType);
		}
	}






	fir::Function* generateCloneFunction(CodegenState* cs, fir::Type* _saa)
	{
		auto fname = "__clone_" + _saa->str();

		iceAssert(isSAA(_saa) || _saa->isArraySliceType());
		auto slicetype = (isSAA(_saa) ? getSAASlice(_saa, false) : fir::ArraySliceType::get(_saa->getArrayElementType(), false));

		iceAssert(slicetype->isArraySliceType());
		bool isArray = !_saa->isStringType();

		fir::Type* outtype = (isSAA(_saa) ? _saa : fir::DynamicArrayType::get(slicetype->getArrayElementType()));

		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ slicetype, fir::Type::getInt64() }, outtype), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			auto s1 = func->getArguments()[0];
			auto cloneofs = func->getArguments()[1];

			auto lhsbuf = cs->irb.GetArraySliceData(s1, "lhsbuf");

			fir::IRBlock* isnull = cs->irb.addNewBlockInFunction("isnull", func);
			fir::IRBlock* notnull = cs->irb.addNewBlockInFunction("notnull", func);

			// if it's null we just fuck off now.
			cs->irb.CondBranch(cs->irb.ICmpEQ(lhsbuf, fir::ConstantValue::getZeroValue(slicetype->getArrayElementType()->getPointerTo())),
				isnull, notnull);

			cs->irb.setCurrentBlock(notnull);
			{
				auto lhslen = cs->irb.Subtract(cs->irb.GetArraySliceLength(s1, "l1"), cloneofs, "lhslen");
				auto newcap = cs->irb.Call(cs->module->getIntrinsicFunction("roundup_pow2"), lhslen, "newcap");

				auto lhsbytecount = cs->irb.Multiply(lhslen, cs->irb.Sizeof(slicetype->getArrayElementType()), "lhsbytecount");
				auto newbytecount = cs->irb.Multiply(newcap, cs->irb.Sizeof(slicetype->getArrayElementType()), "newbytecount");


				auto mallocf = cs->getOrDeclareLibCFunction(ALLOCATE_MEMORY_FUNC);
				iceAssert(mallocf);

				fir::Value* newbuf = cs->irb.Call(mallocf, !isArray ? cs->irb.Add(newbytecount, getCI(1)) : newbytecount, "buf");
				{
					// fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
					// cs->irb.Call(memcpyf, { buf, castRawBufToElmPtr(cs, saa, lhsbuf), lhsbytecount,
					// 	fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false) });

					_handleCallingAppropriateCloneFunction(cs, func, slicetype->getArrayElementType(), lhsbuf,
						newbuf, lhslen, lhsbytecount, cloneofs);

					// null terminator
					if(!isArray)
						cs->irb.Store(fir::ConstantInt::getInt8(0), cs->irb.PointerAdd(newbuf, lhsbytecount));
				}


				{
					auto ret = cs->irb.CreateValue(outtype);
					ret = cs->irb.SetSAAData(ret, castRawBufToElmPtr(cs, outtype, newbuf));
					ret = cs->irb.SetSAALength(ret, lhslen);                    //? vv for the null terminator
					ret = cs->irb.SetSAACapacity(ret, !isArray ? cs->irb.Subtract(newcap, getCI(1)) : newcap);
					ret = initSAAWithRefCount(cs, ret, getCI(1));

					cs->irb.Return(ret);
				}
			}

			cs->irb.setCurrentBlock(isnull);
			{
				auto ret = cs->irb.CreateValue(outtype);
				ret = cs->irb.SetSAAData(ret, castRawBufToElmPtr(cs, outtype, getCI(0)));
				ret = cs->irb.SetSAALength(ret, getCI(0));
				ret = cs->irb.SetSAACapacity(ret, getCI(0));
				ret = initSAAWithRefCount(cs, ret, getCI(1));

				cs->irb.Return(ret);
			}

			retfn = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(retfn);
		return retfn;
	}


	fir::Function* generateAppendFunction(CodegenState* cs, fir::Type* saa)
	{
		auto fname = "__append_" + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ saa, getSAASlice(saa) }, saa), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* lhs = func->getArguments()[0];
			fir::Value* rhs = func->getArguments()[1];

			auto lhslen = cs->irb.GetSAALength(lhs, "lhslen");

			auto rhsbuf = cs->irb.GetArraySliceData(rhs, "rhsbuf");
			auto rhslen = cs->irb.GetArraySliceLength(rhs, "rhslen");
			auto rhsbytecount = cs->irb.Multiply(rhslen, cs->irb.Sizeof(rhs->getType()->getArrayElementType()), "rhsbytecount");

			// this handles the null case as well.
			lhs = cs->irb.Call(generateReserveAtLeastFunction(cs, saa), lhs, cs->irb.Add(lhslen, rhslen));
			auto lhsbuf = cs->irb.GetSAAData(lhs, "lhsbuf");

			// do a copy over the rhs.
			{
				fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");
				cs->irb.Call(memcpyf, {
					cs->irb.PointerTypeCast(cs->irb.PointerAdd(lhsbuf, lhslen), fir::Type::getMutInt8Ptr()),
					cs->irb.PointerTypeCast(rhsbuf, fir::Type::getMutInt8Ptr()), rhsbytecount,
					fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false)
				});

				// null terminator
				if(saa->isStringType())
				{
					cs->irb.Store(fir::ConstantInt::getInt8(0), cs->irb.PointerTypeCast(cs->irb.PointerAdd(lhsbuf, cs->irb.Add(lhslen, rhslen)),
						fir::Type::getMutInt8Ptr()));
				}
			}

			lhs = cs->irb.SetSAALength(lhs, cs->irb.Add(lhslen, rhslen));

			// handle refcounting
			if(cs->isRefCountedType(getSAAElm(saa)))
			{
				auto incrfn = generateIncrementArrayRefCountInLoopFunction(cs, getSAAElm(saa));
				iceAssert(incrfn);

				cs->irb.Call(incrfn, rhsbuf, rhslen);
			}

			cs->irb.Return(lhs);


			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}


	fir::Function* generateElementAppendFunction(CodegenState* cs, fir::Type* saa)
	{
		auto fname = "__append_elm_" + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ saa, getSAAElm(saa) }, saa), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* lhs = func->getArguments()[0];
			fir::Value* rhs = func->getArguments()[1];

			auto rhsp = cs->irb.ImmutStackAlloc(getSAAElm(saa), rhs, "rhsptr");

			auto rhsslice = cs->irb.CreateValue(getSAASlice(saa), "rhsslice");
			rhsslice = cs->irb.SetArraySliceData(rhsslice, cs->irb.PointerTypeCast(rhsp, rhsp->getType()->getMutablePointerVersion()));
			rhsslice = cs->irb.SetArraySliceLength(rhsslice, getCI(1));

			auto appf = generateAppendFunction(cs, saa);
			iceAssert(appf);

			auto ret = cs->irb.Call(appf, lhs, rhsslice);
			cs->irb.Return(ret);

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}




	fir::Function* generateConstructFromTwoFunction(CodegenState* cs, fir::Type* saa)
	{
		auto fname = "__construct_fromtwo_" + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ getSAASlice(saa, false), getSAASlice(saa, false) }, saa), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* lhs = func->getArguments()[0];
			fir::Value* rhs = func->getArguments()[1];

			auto lhslen = cs->irb.GetArraySliceLength(lhs, "lhslen");
			auto rhslen = cs->irb.GetArraySliceLength(rhs, "rhslen");

			auto lhsbuf = cs->irb.GetArraySliceData(lhs, "lhsbuf");
			auto rhsbuf = cs->irb.GetArraySliceData(rhs, "rhsbuf");

			// step 1 -- make a null of the SAA
			auto ret = cs->irb.CreateValue(saa);
			ret = cs->irb.SetSAAData(ret, castRawBufToElmPtr(cs, saa, getCI(0)));
			ret = cs->irb.SetSAALength(ret, getCI(0));
			ret = cs->irb.SetSAACapacity(ret, getCI(0));
			ret = cs->irb.SetSAARefCountPointer(ret, fir::ConstantValue::getZeroValue(fir::Type::getInt64Ptr()));


			ret = cs->irb.Call(generateReserveAtLeastFunction(cs, saa), ret, cs->irb.Add(cs->irb.Add(lhslen, rhslen),
				saa->isStringType() ? getCI(1) : getCI(0)));


			auto buf = cs->irb.GetSAAData(ret, "buf");
			{
				fir::Function* memcpyf = cs->module->getIntrinsicFunction("memmove");

				auto rawbuf = cs->irb.PointerTypeCast(buf, fir::Type::getMutInt8Ptr(), "rawbuf");
				auto rawlhsbuf = cs->irb.PointerTypeCast(lhsbuf, fir::Type::getMutInt8Ptr(), "rawlhsbuf");
				auto rawrhsbuf = cs->irb.PointerTypeCast(rhsbuf, fir::Type::getMutInt8Ptr(), "rawrhsbuf");

				auto lhsbytecount = cs->irb.Multiply(lhslen, cs->irb.Sizeof(getSAAElm(saa)), "lhsbytecount");
				auto rhsbytecount = cs->irb.Multiply(rhslen, cs->irb.Sizeof(getSAAElm(saa)), "rhsbytecount");

				cs->irb.Call(memcpyf, { rawbuf, rawlhsbuf,
					lhsbytecount, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false)
				});

				cs->irb.Call(memcpyf, { cs->irb.PointerAdd(rawbuf, lhsbytecount), rawrhsbuf,
					rhsbytecount, fir::ConstantInt::getInt32(0), fir::ConstantBool::get(false)
				});

				// if it's a string, again, null terminator.
				if(saa->isStringType())
				{
					cs->irb.Store(fir::ConstantInt::getInt8(0), cs->irb.PointerAdd(rawbuf, cs->irb.Add(lhsbytecount, rhsbytecount)));
				}
				else if(cs->isRefCountedType(getSAAElm(saa)))
				{
					auto incrfn = generateIncrementArrayRefCountInLoopFunction(cs, getSAAElm(saa));
					iceAssert(incrfn);

					cs->irb.Call(incrfn, lhsbuf, lhslen);
					cs->irb.Call(incrfn, rhsbuf, rhslen);
				}

				ret = cs->irb.SetSAALength(ret, cs->irb.Add(lhslen, rhslen));
				cs->irb.Return(ret);
			}


			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}



	fir::Function* generateConstructWithElementFunction(CodegenState* cs, fir::Type* saa)
	{
		auto fname = "__construct_withelm_" + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ getSAASlice(saa), getSAAElm(saa) }, saa), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* lhs = func->getArguments()[0];
			fir::Value* rhs = func->getArguments()[1];

			auto rhsslice = cs->irb.CreateValue(getSAASlice(saa), "rhsslice");
			rhsslice = cs->irb.SetArraySliceData(rhsslice, cs->irb.ImmutStackAlloc(getSAAElm(saa), rhs, "rhsptr"));
			rhsslice = cs->irb.SetArraySliceLength(rhsslice, getCI(1));

			auto appf = generateConstructFromTwoFunction(cs, saa);
			iceAssert(appf);

			auto ret = cs->irb.Call(appf, lhs, rhsslice);
			cs->irb.Return(ret);

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}









	fir::Function* generateReserveAtLeastFunction(CodegenState* cs, fir::Type* saa)
	{
		auto fname = "__reserve_atleast_" + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ saa, fir::Type::getInt64() }, saa), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);


			auto s1 = func->getArguments()[0];
			auto minsz = func->getArguments()[1];

			auto oldlen = cs->irb.GetSAALength(s1, "oldlen");
			auto oldcap = cs->irb.GetSAACapacity(s1, "oldcap");

			fir::IRBlock* nullrc = cs->irb.addNewBlockInFunction("nullrc", func);
			fir::IRBlock* notnullrc = cs->irb.addNewBlockInFunction("notnullrc", func);
			fir::IRBlock* mergerc = cs->irb.addNewBlockInFunction("mergerc", func);

			auto oldrcp = cs->irb.GetSAARefCountPointer(s1, "oldrcp");
			cs->irb.CondBranch(cs->irb.ICmpEQ(oldrcp, cs->irb.IntToPointerCast(getCI(0), fir::Type::getInt64Ptr())), nullrc, notnullrc);

			fir::Value* nullphi = 0;
			fir::Value* notnullphi = 0;

			cs->irb.setCurrentBlock(nullrc);
			{
				nullphi = getCI(1);
				cs->irb.UnCondBranch(mergerc);
			}

			cs->irb.setCurrentBlock(notnullrc);
			{
				notnullphi = cs->irb.GetSAARefCount(s1, "oldref");
				cs->irb.UnCondBranch(mergerc);
			}


			cs->irb.setCurrentBlock(mergerc);
			{
				auto oldrc = cs->irb.CreatePHINode(fir::Type::getInt64());
				oldrc->addIncoming(nullphi, nullrc);
				oldrc->addIncoming(notnullphi, notnullrc);

				fir::IRBlock* returnUntouched = cs->irb.addNewBlockInFunction("noExpansion", func);
				fir::IRBlock* doExpansion = cs->irb.addNewBlockInFunction("expand", func);

				cs->irb.CondBranch(cs->irb.ICmpLEQ(minsz, oldcap), returnUntouched, doExpansion);



				cs->irb.setCurrentBlock(doExpansion);
				{

					// TODO: is it faster to times 3 divide by 2, or do FP casts and times 1.5?
					auto newlen = cs->irb.Divide(cs->irb.Multiply(minsz, getCI(3)), getCI(2), "mul1.5");

					// call realloc. handles the null case as well, which is nice.
					auto oldbuf = cs->irb.PointerTypeCast(cs->irb.GetSAAData(s1), fir::Type::getMutInt8Ptr(), "oldbuf");

					auto newbytecount = cs->irb.Multiply(newlen, cs->irb.Sizeof(getSAAElm(saa)), "newbytecount");

					if(saa->isStringType())
						newbytecount = cs->irb.Add(newbytecount, getCI(1));

					auto newbuf = cs->irb.Call(cs->getOrDeclareLibCFunction(REALLOCATE_MEMORY_FUNC), oldbuf, newbytecount, "newbuf");
					newbuf = castRawBufToElmPtr(cs, saa, newbuf);

					// null terminator
					if(saa->isStringType())
						cs->irb.Store(fir::ConstantInt::getInt8(0), cs->irb.PointerAdd(newbuf, cs->irb.Subtract(newbytecount, getCI(1))));

					auto ret = cs->irb.CreateValue(saa);
					ret = cs->irb.SetSAAData(ret, newbuf);
					ret = cs->irb.SetSAALength(ret, oldlen);
					ret = cs->irb.SetSAACapacity(ret, newlen);
					ret = initSAAWithRefCount(cs, ret, oldrc);

					cs->irb.Return(ret);
				}

				cs->irb.setCurrentBlock(returnUntouched);
				{
					// as the name implies, do nothing.
					cs->irb.Return(s1);
				}
			}

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}


	fir::Function* generateReserveExtraFunction(CodegenState* cs, fir::Type* saa)
	{
		// we can just do this in terms of reserveAtLeast.

		auto fname = "__reserve_extra" + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ saa, fir::Type::getInt64() }, saa), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			auto s1 = func->getArguments()[0];
			auto extrasz = func->getArguments()[1];

			auto minsz = cs->irb.Add(cs->irb.GetSAACapacity(s1), extrasz);
			auto ret = cs->irb.Call(generateReserveAtLeastFunction(cs, saa), s1, minsz);

			cs->irb.Return(ret);

			cs->irb.setCurrentBlock(restore);
			retfn = func;
		}

		iceAssert(retfn);
		return retfn;
	}



	fir::Function* generateBoundsCheckFunction(CodegenState* cs, fir::Type* saa, bool isDecomp)
	{
		auto fname = (isDecomp ? "__boundscheck_" : "__boundscheck_decomp_") + saa->str();

		iceAssert(isSAA(saa));
		fir::Function* retfn = cs->module->getFunction(Identifier(fname, IdKind::Name));

		if(!retfn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(fname, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64(), fir::Type::getInt64(), fir::Type::getCharSlice(false) },
					fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = cs->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* checkneg = cs->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			cs->irb.setCurrentBlock(entry);

			fir::Value* max = func->getArguments()[0];
			fir::Value* ind = func->getArguments()[1];
			fir::Value* res = 0;

			// if we're decomposing, it's length vs length, so compare strictly greater.
			if(isDecomp)    res = cs->irb.ICmpGT(ind, max);
			else            res = cs->irb.ICmpGEQ(ind, max);


			cs->irb.CondBranch(res, failb, checkneg);
			cs->irb.setCurrentBlock(failb);
			{
				if(isDecomp)
				{
					printRuntimeError(cs, func->getArguments()[2], "Index '%ld' out of bounds for "
						+ std::string(saa->isStringType() ? "string" : "array") + " of length %ld\n", { ind, max });
				}
				else
				{
					printRuntimeError(cs, func->getArguments()[2], "Binding of '%ld' "
						+ std::string(saa->isStringType() ? "chars" : "elements") + " out of bounds for string of length %ld\n", { ind, max });
				}
			}

			cs->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res = cs->irb.ICmpLT(ind, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(res, failb, merge);
			}

			cs->irb.setCurrentBlock(merge);
			{
				cs->irb.ReturnVoid();
			}

			retfn = func;
			cs->irb.setCurrentBlock(restore);
		}

		iceAssert(retfn);
		return retfn;
	}

}
}
}























