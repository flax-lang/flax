// arrays.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

#define BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME        "__array_boundscheck"
#define BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME "__array_boundscheckdecomp"

#define BUILTIN_ARRAY_CMP_FUNC_NAME                 "__array_compare"

#define BUILTIN_ARRAY_SET_ELEMENTS_DEFAULT_NAME     "__array_setelementsdefault"
#define BUILTIN_ARRAY_SET_ELEMENTS_NAME             "__array_setelements"
#define BUILTIN_ARRAY_CALL_CLASS_CONSTRUCTOR        "__array_callclassinit"

#define BUILTIN_DYNARRAY_CLONE_FUNC_NAME            "__darray_clone"
#define BUILTIN_DYNARRAY_APPEND_FUNC_NAME           "__darray_append"
#define BUILTIN_DYNARRAY_APPEND_ELEMENT_FUNC_NAME   "__darray_appendelement"
#define BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME         "__darray_popback"
#define BUILTIN_DYNARRAY_MAKE_FROM_TWO_FUNC_NAME    "__darray_combinetwo"

#define BUILTIN_DYNARRAY_RESERVE_ENOUGH_NAME        "__darray_reservesufficient"
#define BUILTIN_DYNARRAY_RESERVE_EXTRA_NAME         "__darray_reserveextra"

#define BUITLIN_DYNARRAY_RECURSIVE_REFCOUNT_NAME    "__darray_recursiverefcount"

#define BUILTIN_SLICE_CLONE_FUNC_NAME               "__slice_clone"
#define BUILTIN_SLICE_APPEND_FUNC_NAME              "__slice_append"
#define BUILTIN_SLICE_APPEND_ELEMENT_FUNC_NAME      "__slice_appendelement"

#define BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME        "__loop_incr_refcount"
#define BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME        "__loop_decr_refcount"


namespace cgn {
namespace glue {
namespace array
{
	fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isPerformingDecomposition)
	{
		// return saa_common::generateBoundsCheckFunction
		fir::Function* fn = cs->module->getFunction(Identifier(isPerformingDecomposition
			? BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME : BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(isPerformingDecomposition ? BUILTIN_ARRAY_DECOMP_BOUNDS_CHECK_FUNC_NAME : BUILTIN_ARRAY_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
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
			if(isPerformingDecomposition)
				res = cs->irb.ICmpGT(ind, max);

			else
				res = cs->irb.ICmpGEQ(ind, max);

			iceAssert(res);

			cs->irb.CondBranch(res, failb, checkneg);
			cs->irb.setCurrentBlock(failb);
			{
				if(isPerformingDecomposition)
					printRuntimeError(cs, func->getArguments()[2], "Tried to decompose array with only '%ld' elements into '%ld' bindings\n", { max, ind });

				else
					printRuntimeError(cs, func->getArguments()[2], "Index '%ld' out of bounds for array of length %ld\n", { ind, max });
			}

			cs->irb.setCurrentBlock(checkneg);
			{
				fir::Value* res2 = cs->irb.ICmpLT(ind, fir::ConstantInt::getInt64(0));
				cs->irb.CondBranch(res2, failb, merge);
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




	fir::Function* getCloneFunction(CodegenState* cs, fir::Type* arrtype)
	{
		// if(arrtype->isDynamicArrayType())		return getDynamicArrayCloneFunction(cs, arrtype->toDynamicArrayType());
		// else if(arrtype->isArraySliceType())	return getSliceCloneFunction(cs, arrtype->toArraySliceType());
		// else									error("unsupported type '%s'", arrtype);

		return saa_common::generateCloneFunction(cs, arrtype);
	}






	fir::Function* getReserveExtraFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return saa_common::generateReserveExtraFunction(cs, arrtype);
	}

	fir::Function* getReserveAtLeastFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return saa_common::generateReserveAtLeastFunction(cs, arrtype);
	}




	fir::Function* getAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return saa_common::generateAppendFunction(cs, arrtype);
	}

	fir::Function* getElementAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return saa_common::generateElementAppendFunction(cs, arrtype);
	}





	fir::Function* getCallClassConstructorOnElementsFunction(CodegenState* cs, fir::ClassType* cls, sst::FunctionDefn* constr,
		const std::vector<FnCallArgument>& args)
	{
		iceAssert(cls);

		auto name = BUILTIN_ARRAY_CALL_CLASS_CONSTRUCTOR + std::string("_") + cls->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ cls->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			// ok: the real difference with the one below is that we need to call the constructor function on every element.

			fir::Value* arrdata = func->getArguments()[0];
			fir::Value* len = func->getArguments()[1];


			fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto ctrptr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), ctrptr);

			cs->irb.UnCondBranch(check);
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLT(cs->irb.Load(ctrptr), len);
				cs->irb.CondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				auto ctr = cs->irb.Load(ctrptr);
				auto ptr = cs->irb.PointerAdd(arrdata, ctr);

				cs->constructClassWithArguments(cls, constr, ptr, args, true);

				cs->irb.Store(cs->irb.Add(ctr, fir::ConstantInt::getInt64(1)), ctrptr);

				cs->irb.UnCondBranch(check);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();




			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}


	fir::Function* getSetElementsToValueFunction(CodegenState* cs, fir::Type* elmType)
	{
		iceAssert(elmType);

		auto name = BUILTIN_ARRAY_SET_ELEMENTS_NAME + std::string("_") + elmType->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ elmType->getMutablePointerTo(), fir::Type::getInt64(), elmType }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arrdata = func->getArguments()[0];
			fir::Value* len = func->getArguments()[1];
			fir::Value* value = func->getArguments()[2];

			iceAssert(value);
			fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto ctrptr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), ctrptr);

			cs->irb.UnCondBranch(check);
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLT(cs->irb.Load(ctrptr), len);
				cs->irb.CondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				auto ctr = cs->irb.Load(ctrptr);
				auto ptr = cs->irb.PointerAdd(arrdata, ctr);

				cs->autoAssignRefCountedValue(CGResult(cs->irb.Load(ptr), ptr), CGResult(value, 0, CGResult::VK::LitRValue), true, true);

				cs->irb.Store(cs->irb.Add(ctr, fir::ConstantInt::getInt64(1)), ctrptr);

				cs->irb.UnCondBranch(check);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}


	fir::Function* getSetElementsToDefaultValueFunction(CodegenState* cs, fir::Type* elmType)
	{
		iceAssert(elmType);

		auto name = BUILTIN_ARRAY_SET_ELEMENTS_DEFAULT_NAME + std::string("_") + elmType->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ elmType->getMutablePointerTo(), fir::Type::getInt64() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* value = 0;

			if(elmType->isClassType())
				value = cs->irb.CreateValue(elmType);

			else
				value = cs->getDefaultValue(elmType);

			iceAssert(value);

			auto setfn = getSetElementsToValueFunction(cs, elmType);
			iceAssert(setfn);

			cs->irb.Call(setfn, func->getArguments()[0], func->getArguments()[1], value);

			cs->irb.ReturnVoid();


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}








	static void _compareFunctionUsingBuiltinCompare(CodegenState* cs, fir::Type* arrtype, fir::Function* func,
		fir::Value* arg1, fir::Value* arg2)
	{
		// ok, ez.
		fir::Value* zeroval = fir::ConstantInt::getInt64(0);
		fir::Value* oneval = fir::ConstantInt::getInt64(1);

		fir::IRBlock* cond = cs->irb.addNewBlockInFunction("cond", func);
		fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
		fir::IRBlock* incr = cs->irb.addNewBlockInFunction("incr", func);
		fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

		fir::Value* ptr1 = 0; fir::Value* ptr2 = 0;

		if(arrtype->isDynamicArrayType())
		{
			ptr1 = cs->irb.GetDynamicArrayData(arg1);
			ptr2 = cs->irb.GetDynamicArrayData(arg2);
		}
		else if(arrtype->isArraySliceType())
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

		if(arrtype->isDynamicArrayType())
		{
			len1 = cs->irb.GetDynamicArrayLength(arg1);
			len2 = cs->irb.GetDynamicArrayLength(arg2);
		}
		else if(arrtype->isArraySliceType())
		{
			len1 = cs->irb.GetArraySliceLength(arg1);
			len2 = cs->irb.GetArraySliceLength(arg2);
		}
		else if(arrtype->isArrayType())
		{
			len1 = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());
			len2 = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());
		}
		else
		{
			error("invalid type '%s'", arrtype);
		}

		// we compare to this to break
		fir::Value* counter = cs->irb.StackAlloc(fir::Type::getInt64());
		cs->irb.Store(zeroval, counter);

		fir::Value* res = cs->irb.StackAlloc(fir::Type::getInt64());
		cs->irb.Store(zeroval, res);


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

			fir::Value* t1 = cs->irb.ICmpEQ(cs->irb.Load(counter), len1);
			fir::Value* t2 = cs->irb.ICmpEQ(cs->irb.Load(counter), len2);

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
			cs->irb.Return(fir::ConstantInt::getInt64(-1));

			cs->irb.setCurrentBlock(reteq);
			cs->irb.Return(fir::ConstantInt::getInt64(0));

			cs->irb.setCurrentBlock(retgt);
			cs->irb.Return(fir::ConstantInt::getInt64(+1));
		}


		cs->irb.setCurrentBlock(body);
		{
			fir::Value* v1 = cs->irb.Load(cs->irb.PointerAdd(ptr1, cs->irb.Load(counter)));
			fir::Value* v2 = cs->irb.Load(cs->irb.PointerAdd(ptr2, cs->irb.Load(counter)));

			fir::Value* c = cs->performBinaryOperation(cs->loc(), { cs->loc(), CGResult(v1) }, { cs->loc(), CGResult(v2) }, "==").value;

			// c is a bool, because it's very generic in nature
			// so we just take !c and convert to i64 to get our result.
			// if c == true, then lhs == rhs, and so we should have 0.

			c = cs->irb.LogicalNot(c);
			c = cs->irb.IntSizeCast(c, fir::Type::getInt64());

			cs->irb.Store(c, res);

			// compare to 0.
			fir::Value* cmpres = cs->irb.ICmpEQ(cs->irb.Load(res), zeroval);

			// if equal, go to incr, if not return directly
			cs->irb.CondBranch(cmpres, incr, merge);
		}


		cs->irb.setCurrentBlock(incr);
		{
			cs->irb.Store(cs->irb.Add(cs->irb.Load(counter), oneval), counter);
			cs->irb.UnCondBranch(cond);
		}



		cs->irb.setCurrentBlock(merge);
		{
			// load and return
			cs->irb.Return(cs->irb.Load(res));
		}
	}


	static void _compareFunctionUsingOperatorFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* curfunc,
		fir::Value* arg1, fir::Value* arg2, fir::Function* opf)
	{
		// fir::Value* zeroval = fir::ConstantInt::getInt64(0);
		error("notsup");
	}



	fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf)
	{
		iceAssert(arrtype);

		auto name = BUILTIN_ARRAY_CMP_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, arrtype }, fir::Type::getInt64()), fir::LinkageType::Internal);

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








	static fir::Function* makeRecursiveRefCountingFunction(CodegenState* cs, fir::DynamicArrayType* arrtype, bool incr)
	{
		auto name = BUITLIN_DYNARRAY_RECURSIVE_REFCOUNT_NAME + std::string(incr ? "_incr_" : "_decr_") + arrtype->encodedStr();
		fir::Function* retf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!retf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arr = func->getArguments()[0];

			auto ptr = cs->irb.GetDynamicArrayData(arr);
			auto len = cs->irb.GetDynamicArrayLength(arr);
			auto cap = cs->irb.GetDynamicArrayCapacity(arr);


			{
				auto elmtype = arrtype->getElementType();

				// here we check whether we actually have a refcount pointer. If we don't, then we're a literal, and there's no need to change
				// the refcount anyway.
				auto prevblk = cs->irb.getCurrentBlock();
				auto dorc = cs->irb.addNewBlockInFunction("dorc", cs->irb.getCurrentFunction());
				auto dontrc = cs->irb.addNewBlockInFunction("dontrcliteral", cs->irb.getCurrentFunction());
				{
					auto rcp = cs->irb.GetDynamicArrayRefCountPointer(arr);
					auto cond = cs->irb.ICmpNEQ(cs->irb.PointerToIntCast(rcp, fir::Type::getInt64()), fir::ConstantInt::getInt64(0));

					cs->irb.CondBranch(cond, dorc, dontrc);
				}

				fir::Value* therefc = 0;
				cs->irb.setCurrentBlock(dorc);
				{
					therefc = cs->irb.GetDynamicArrayRefCount(arr);

					fir::Value* newrc = 0;
					if(incr)    newrc = cs->irb.Add(therefc, fir::ConstantInt::getInt64(1));
					else        newrc = cs->irb.Subtract(therefc, fir::ConstantInt::getInt64(1));

					// update it.
					therefc = newrc;
					cs->irb.SetDynamicArrayRefCount(arr, newrc);
					cs->irb.UnCondBranch(dontrc);
				}

				cs->irb.setCurrentBlock(dontrc);

				#if DEBUG_ARRAY_REFCOUNTING
				{
					std::string x = incr ? "(incr)" : "(decr)";

					cs->printIRDebugMessage("* ARRAY:  " + x + " - new rc of: (ptr: %p, len: %ld, cap: %ld) = %d",
						{ cs->irb.GetSAAData(arr), cs->irb.GetSAALength(arr), cs->irb.GetSAACapacity(arr), cs->irb.GetSAARefCount(arr) });
				}
				#endif

				// ok. if we're incrementing, then we're done -- but if we're decrementing, we may need to free the memory.
				if(!incr)
				{
					fir::IRBlock* dealloc = cs->irb.addNewBlockInFunction("dealloc", func);
					fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

					auto zv = fir::ConstantInt::getInt64(0);

					//! NOTE: what we want to happen here is for us to free the memory, but only if refcnt == 0 && capacity >= 0
					//* so our condition is (REFCOUNT == 0) & (CAP >= 0)

					auto refc = cs->irb.CreatePHINode(fir::Type::getInt64());
					refc->addIncoming(therefc, dorc);
					refc->addIncoming(fir::ConstantInt::getInt64(-1), prevblk);

					auto dofree = cs->irb.BitwiseAND(cs->irb.ICmpEQ(refc, zv), cs->irb.ICmpGEQ(cap, zv));


					cs->irb.CondBranch(dofree, dealloc, merge);

					cs->irb.setCurrentBlock(dealloc);
					{
						auto memptr = cs->irb.PointerTypeCast(ptr, fir::Type::getMutInt8Ptr());

						auto freefn = cs->getOrDeclareLibCFunction(FREE_MEMORY_FUNC);
						iceAssert(freefn);

						// only when we free, do we loop through our array and decrement its refcount.
						if(cs->isRefCountedType(elmtype))
						{
							auto ctrp = cs->irb.StackAlloc(fir::Type::getInt64());
							cs->irb.Store(zv, ctrp);

							cs->createWhileLoop([cs, ctrp, len](auto pass, auto fail) {
								auto cond = cs->irb.ICmpLT(cs->irb.Load(ctrp), len);
								cs->irb.CondBranch(cond, pass, fail);
							},
							[cs, ctrp, ptr]() {

								auto ctr = cs->irb.Load(ctrp);
								auto p = cs->irb.PointerAdd(ptr, ctr);

								// cs->decrementRefCount(cs->irb.Load(p));

								cs->irb.Store(cs->irb.Add(ctr, fir::ConstantInt::getInt64(1)), ctrp);
							});
						}

						cs->irb.Call(freefn, memptr);
						cs->irb.Call(freefn, cs->irb.PointerTypeCast(cs->irb.GetDynamicArrayRefCountPointer(arr), fir::Type::getMutInt8Ptr()));

						#if DEBUG_ARRAY_ALLOCATION
						{
							cs->printIRDebugMessage("* ARRAY:  free(): (ptr: %p / rcp: %p)", {
								memptr, cs->irb.GetSAARefCountPointer(arr) });
						}
						#endif


						cs->irb.UnCondBranch(merge);
					}

					cs->irb.setCurrentBlock(merge);
				}
			}



			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);
			retf = func;
		}

		iceAssert(retf);
		return retf;
	}






	static fir::Function* _getDoRefCountFunctionForDynamicArray(CodegenState* cs, fir::DynamicArrayType* arrtype, bool increment)
	{
		auto name = (increment ? BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME : BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME)
			+ std::string("_") + arrtype->encodedStr();

		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype }, arrtype), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arr = func->getArguments()[0];
			auto fn = makeRecursiveRefCountingFunction(cs, arr->getType()->toDynamicArrayType(), increment);
			iceAssert(fn);

			cs->irb.Call(fn, arr);

			cs->irb.Return(arr);

			cs->irb.setCurrentBlock(restore);
			cmpf = func;
		}

		iceAssert(cmpf);
		return cmpf;
	}

	static fir::Function* _getDoRefCountFunctionForArray(CodegenState* cs, fir::ArrayType* arrtype, bool incr)
	{
		error("NO!");
		#if 0
		auto elmtype = arrtype->getElementType();

		//* note that we don't need a separate name for it, since the type of the array is added to the name itself
		auto name = (incr ? BUILTIN_LOOP_INCR_REFCOUNT_FUNC_NAME : BUILTIN_LOOP_DECR_REFCOUNT_FUNC_NAME)
			+ std::string("_") + arrtype->encodedStr();

		fir::Function* cmpf = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!cmpf)
		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype->getPointerTo() }, fir::Type::getVoid()), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			fir::Value* arrptr = func->getArguments()[0];
			fir::Value* ptr = cs->irb.ConstGEP2(arrptr, 0, 0);
			fir::Value* len = fir::ConstantInt::getInt64(arrtype->toArrayType()->getArraySize());

			fir::IRBlock* check = cs->irb.addNewBlockInFunction("check", func);
			fir::IRBlock* body = cs->irb.addNewBlockInFunction("body", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto idxptr = cs->irb.StackAlloc(fir::Type::getInt64());

			// already set to 0 internally
			// cs->irb.Store(fir::ConstantInt::getInt64(0), idxptr);

			cs->irb.UnCondBranch(check);
			cs->irb.setCurrentBlock(check);
			{
				auto cond = cs->irb.ICmpLT(cs->irb.Load(idxptr), len);
				cs->irb.CondBranch(cond, body, merge);
			}

			cs->irb.setCurrentBlock(body);
			{
				auto valptr = cs->irb.PointerAdd(ptr, cs->irb.Load(idxptr));
				auto val = cs->irb.Load(valptr);

				// if we're a dynamic array, then call the dynamic array version.
				// if it's an array again, then call ourselves.
				if(elmtype->isDynamicArrayType())
				{
					auto fn = (incr ? getIncrementArrayRefCountFunction(cs, elmtype) : getDecrementArrayRefCountFunction(cs, elmtype));
					iceAssert(fn);

					cs->irb.Call(fn, val);
				}
				else if(elmtype->isArrayType())
				{
					// call ourselves. we already declared and everything, so it should be fine.
					auto fn = _getDoRefCountFunctionForArray(cs, elmtype->toArrayType(), incr);
					iceAssert(fn);

					cs->irb.Call(fn, valptr);
				}
				else
				{
					if(incr)	cs->incrementRefCount(val);
					else		cs->decrementRefCount(val);
				}

				cs->irb.Store(cs->irb.Add(cs->irb.Load(idxptr), fir::ConstantInt::getInt64(1)), idxptr);

				cs->irb.UnCondBranch(check);
			}

			cs->irb.setCurrentBlock(merge);
			cs->irb.ReturnVoid();

			cs->irb.setCurrentBlock(restore);
			cmpf = func;
		}

		iceAssert(cmpf);
		return cmpf;
		#endif
	}


	fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype)
	{
		if(arrtype->isDynamicArrayType())	return _getDoRefCountFunctionForDynamicArray(cs, arrtype->toDynamicArrayType(), true);
		else								return _getDoRefCountFunctionForArray(cs, arrtype->toArrayType(), true);
	}

	fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype)
	{
		if(arrtype->isDynamicArrayType())	return _getDoRefCountFunctionForDynamicArray(cs, arrtype->toDynamicArrayType(), false);
		else								return _getDoRefCountFunctionForArray(cs, arrtype->toArrayType(), false);
	}












	fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype)
	{
		return saa_common::generateConstructFromTwoFunction(cs, arrtype);
	}


	fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::Type* arrtype)
	{
		iceAssert(arrtype);
		iceAssert(arrtype->isDynamicArrayType() || arrtype->isArraySliceType());

		auto name = BUILTIN_DYNARRAY_POP_BACK_FUNC_NAME + std::string("_") + arrtype->encodedStr();
		fir::Function* fn = cs->module->getFunction(Identifier(name, IdKind::Name));

		if(!fn)
		{
			bool isslice = arrtype->isArraySliceType();

			auto restore = cs->irb.getCurrentBlock();
			auto retTy = fir::TupleType::get({ arrtype, arrtype->getArrayElementType() });

			fir::Function* func = cs->module->getOrCreateFunction(Identifier(name, IdKind::Name),
				fir::FunctionType::get({ arrtype, fir::Type::getCharSlice(false) }, retTy), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);


			fir::Value* arr = func->getArguments()[0];
			fir::Value* loc = func->getArguments()[1];

			fir::Value* origlen = (isslice ? cs->irb.GetArraySliceLength(arr) : cs->irb.GetDynamicArrayLength(arr));

			fir::IRBlock* fail = cs->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = cs->irb.addNewBlockInFunction("merge", func);

			auto cond = cs->irb.ICmpLT(origlen, fir::ConstantInt::getInt64(1));

			cs->irb.CondBranch(cond, fail, merge);
			cs->irb.setCurrentBlock(fail);
			{
				printRuntimeError(cs, loc, "Calling pop() on an empty array\n", { });
			}


			cs->irb.setCurrentBlock(merge);
			{
				auto newlen = cs->irb.Subtract(origlen, fir::ConstantInt::getInt64(1));
				fir::Value* ret = 0;

				// first, load the last value
				if(isslice)
				{
					auto ptr = cs->irb.GetArraySliceData(arr);
					auto val = cs->irb.Load(cs->irb.PointerAdd(ptr, newlen));

					auto newarr = cs->irb.SetArraySliceLength(arr, newlen);
					ret = cs->irb.CreateValue(retTy);
					ret = cs->irb.InsertValue(ret, { 0 }, newarr);
					ret = cs->irb.InsertValue(ret, { 1 }, val);
				}
				else
				{
					auto ptr = cs->irb.GetDynamicArrayData(arr);
					auto val = cs->irb.Load(cs->irb.PointerAdd(ptr, newlen));

					auto newarr = cs->irb.SetDynamicArrayLength(arr, newlen);
					ret = cs->irb.CreateValue(retTy);
					ret = cs->irb.InsertValue(ret, { 0 }, newarr);
					ret = cs->irb.InsertValue(ret, { 1 }, val);
				}

				iceAssert(ret);
				cs->irb.Return(ret);
			}


			cs->irb.setCurrentBlock(restore);
			fn = func;
		}

		return fn;
	}




}
}
}

