// AllocCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

using namespace Ast;
using namespace Codegen;


static Result_t recursivelyDoAlloc(CodegenInstance* cgi, Alloc* user, fir::Type* type, fir::Value* size, std::vector<Expr*> params,
	std::vector<fir::Value*>& sizes)
{
	fir::Function* mallocf = cgi->getOrDeclareLibCFunc(ALLOCATE_MEMORY_FUNC);
	iceAssert(mallocf);

	mallocf = cgi->module->getFunction(mallocf->getName());
	iceAssert(mallocf);


	fir::Value* oneValue = fir::ConstantInt::getInt64(1, cgi->getContext());
	fir::Value* zeroValue = fir::ConstantInt::getInt64(0, cgi->getContext());

	// uint64_t typesize = cgi->execTarget->getTypeSizeInBits(type) / 8;

	// fir::Value* allocsize = cgi->irb.CreateGEP2(fir::ConstantValue::getZeroValue(type->getPointerTo()), zeroValue, oneValue);
	// allocsize = cgi->irb.CreatePointerToIntCast(allocsize, fir::Type::getInt64());

	fir::Value* allocsize = cgi->irb.CreateSizeof(type);

	size = cgi->autoCastType(fir::Type::getInt64(), size);

	fir::Value* totalAlloc = cgi->irb.CreateMul(allocsize, size, "totalalloc");
	fir::Value* allocmemptr = cgi->getStackAlloc(type->getPointerTo(), "allocmemptr");

	fir::Value* amem = cgi->irb.CreatePointerTypeCast(cgi->irb.CreateCall1(mallocf, totalAlloc), type->getPointerTo());
	cgi->irb.CreateStore(amem, allocmemptr);


	fir::IRBlock* curbb = cgi->irb.getCurrentBlock();	// store the current bb
	fir::IRBlock* loopHead = cgi->irb.addNewBlockInFunction("loopHead", curbb->getParentFunction());
	fir::IRBlock* loopBegin = cgi->irb.addNewBlockInFunction("loopBegin", curbb->getParentFunction());
	fir::IRBlock* loopEnd = cgi->irb.addNewBlockInFunction("loopEnd", curbb->getParentFunction());

	fir::IRBlock* allocZeroCase = cgi->irb.addNewBlockInFunction("allocZeroCase", curbb->getParentFunction());
	fir::IRBlock* loopMerge = cgi->irb.addNewBlockInFunction("loopMerge", curbb->getParentFunction());

	cgi->irb.setCurrentBlock(curbb);
	cgi->irb.CreateUnCondBranch(loopHead);

	cgi->irb.setCurrentBlock(loopHead);

	// set a counter
	fir::Value* counterPtr = cgi->irb.CreateStackAlloc(fir::Type::getInt64(), "counterPtr");
	cgi->irb.CreateStore(zeroValue, counterPtr);

	// check for zero.
	{
		fir::Value* isZero = cgi->irb.CreateICmpEQ(size, zeroValue, "iszero");
		cgi->irb.CreateCondBranch(isZero, allocZeroCase, loopBegin);
	}


	// begin the loop
	cgi->irb.setCurrentBlock(loopBegin);
	{
		// get the pointer.
		fir::Value* pointer = cgi->irb.CreateGetPointer(cgi->irb.CreateLoad(allocmemptr),
			cgi->irb.CreateLoad(counterPtr), "pointerPtr");

		if(type->isStructType() || type->isClassType())
		{
			// call the init func
			TypePair_t* typePair = cgi->getType(type);

			std::vector<fir::Value*> args;
			args.push_back(pointer);
			for(Expr* e : params)
				args.push_back(e->codegen(cgi).value);

			fir::Function* initfunc = cgi->getStructInitialiser(user, typePair, args, { }, user->ptype);
			iceAssert(initfunc);

			cgi->irb.CreateCall(initfunc, args);
		}
		else if(type->isDynamicArrayType() && sizes.size() > 0)
		{
			fir::Value* front = sizes.front();
			sizes.erase(sizes.begin());

			fir::Value* rret = recursivelyDoAlloc(cgi, user, type->toDynamicArrayType()->getElementType(), front, params, sizes).value;
			cgi->irb.CreateStore(rret, pointer);
		}
		else if(params.size() > 0)
		{
			// ok do it
			fir::Value* val = 0;
			fir::Value* vptr = 0;

			std::tie(val, vptr) = params[0]->codegen(cgi);

			val = cgi->autoCastType(pointer->getType()->getPointerElementType(), val, vptr);

			if(val->getType() != pointer->getType()->getPointerElementType())
			{
				error(user, "Cannot initialise values of type '%s' with one of type '%s'",
					pointer->getType()->getPointerElementType()->str().c_str(), val->getType()->str().c_str());
			}

			cgi->irb.CreateStore(val, pointer);
		}
		else if(type->isStringType())
		{
			cgi->irb.CreateStore(cgi->getEmptyString().value, pointer);
		}
		else
		{
			cgi->irb.CreateStore(fir::ConstantValue::getZeroValue(type), pointer);
		}


		// increment counter
		fir::Value* incremented = cgi->irb.CreateAdd(oneValue, cgi->irb.CreateLoad(counterPtr));
		cgi->irb.CreateStore(incremented, counterPtr);


		// check.
		// basically this: if counter < size goto loopBegin else goto loopEnd

		cgi->irb.CreateCondBranch(cgi->irb.CreateICmpLT(cgi->irb.CreateLoad(counterPtr), size), loopBegin, loopEnd);
	}


	// in zeroBlock, set the pointer to 0 and branch to merge
	cgi->irb.setCurrentBlock(allocZeroCase);
	{
		cgi->irb.CreateStore(fir::ConstantValue::getZeroValue(type->getPointerTo()), allocmemptr);
		cgi->irb.CreateUnCondBranch(loopMerge);
	}



	// in end block... do nothing.
	cgi->irb.setCurrentBlock(loopEnd);
	{
		cgi->irb.CreateUnCondBranch(loopMerge);
	}


	cgi->irb.setCurrentBlock(loopMerge);
	fir::Value* data = cgi->irb.CreateLoad(allocmemptr, "mem");

	// make the dynamic array
	return cgi->createDynamicArrayFromPointer(data, size, size);
}






















Result_t Alloc::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is the only place in the compiler where a hardcoded call is made to a non-provided function.

	fir::Type* allocType = 0;

	allocType = cgi->getTypeFromParserType(this, this->ptype);
	iceAssert(allocType);

	fir::Value* oneValue = fir::ConstantInt::getInt64(1, cgi->getContext());

	if(!allocType->isClassType() && !allocType->isStructType() && this->params.size() > 1)
	{
		error(this, "Primitive types (have '%s' here) only accept one initial value, have '%zu' (too many)",
			allocType->str().c_str(), this->params.size());
	}

	if(this->counts.size() > 0)
	{
		std::vector<fir::Value*> cnts;

		for(auto c : this->counts)
			cnts.push_back(c->codegen(cgi).value);

		fir::Value* firstSize = cnts.front();
		cnts.erase(cnts.begin());

		for(size_t i = 1; i < this->counts.size(); i++)
			allocType = fir::DynamicArrayType::get(allocType);

		return recursivelyDoAlloc(cgi, this, allocType, firstSize, this->params, cnts);
	}
	else
	{
		std::vector<fir::Value*> cnts;

		fir::Value* dptr = recursivelyDoAlloc(cgi, this, allocType, oneValue, this->params, cnts).pointer;

		// this is a no-size alloc,
		// return a straight pointer.

		iceAssert(dptr);
		fir::Value* ret = cgi->irb.CreateGetDynamicArrayData(dptr);
		return Result_t(ret, 0);
	}
}

fir::Type* Alloc::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	fir::Type* base = cgi->getTypeFromParserType(this, this->ptype);

	if(this->counts.size() == 0) return base->getPointerTo();

	for(size_t i = 0; i < this->counts.size(); i++)
		base = fir::DynamicArrayType::get(base);

	return base;
}










// note: i'm quite proud of this function
// took a good 15 minutes of brain-ing to figure out
// basically, a function from nest = N to nest = 0 will be generated
// functions with nest > 0 will call (and generate if needed) the function for nest = N - 1
// aka recursion... in recursion!
static fir::Function* makeRecursiveDeallocFunction(CodegenInstance* cgi, fir::Type* type, int nest)
{
	std::string name = "__recursive_dealloc_" + type->encodedStr() + "_N" + std::to_string(nest);
	fir::Function* df = cgi->module->getFunction(Identifier(name, IdKind::Name));

	if(!df)
	{
		auto restore = cgi->irb.getCurrentBlock();

		fir::Function* func = cgi->module->getOrCreateFunction(Identifier(name, IdKind::Name),
			fir::FunctionType::get({ type, fir::Type::getInt64(), fir::Type::getBool() },
				fir::Type::getVoid(), false), fir::LinkageType::Internal);

		func->setAlwaysInline();

		fir::IRBlock* entry = cgi->irb.addNewBlockInFunction("entry", func);
		cgi->irb.setCurrentBlock(entry);

		fir::Value* ptr = func->getArguments()[0];
		fir::Value* len = func->getArguments()[1];
		fir::Value* top_skipFree = func->getArguments()[2];

		// check what kind of function we are
		if(nest == 0)
		{
			if(cgi->isRefCountedType(type->getPointerElementType()))
			{
				// just call that monster
				fir::Function* decrfn = RuntimeFuncs::Array::getDecrementArrayRefCountFunction(cgi, type->getPointerElementType());
				iceAssert(decrfn);

				cgi->irb.CreateCall2(decrfn, ptr, len);
			}

			fir::IRBlock* dofree = cgi->irb.addNewBlockInFunction("doFree", func);
			fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

			cgi->irb.CreateCondBranch(top_skipFree, merge, dofree);
			cgi->irb.setCurrentBlock(dofree);
			{
				// just free
				fir::Function* freef = cgi->getOrDeclareLibCFunc(FREE_MEMORY_FUNC);
				iceAssert(freef);

				cgi->irb.CreateCall1(freef, cgi->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()));

				cgi->irb.CreateUnCondBranch(merge);
			}

			cgi->irb.setCurrentBlock(merge);
			cgi->irb.CreateReturnVoid();
		}
		else
		{
			// loop from 0 to len
			fir::IRBlock* loopcond = cgi->irb.addNewBlockInFunction("loopCond", func);
			fir::IRBlock* loopbody = cgi->irb.addNewBlockInFunction("loopBody", func);
			fir::IRBlock* loopmerge = cgi->irb.addNewBlockInFunction("loopMerge", func);

			// make a new var
			fir::Value* counter = cgi->irb.CreateStackAlloc(fir::Type::getInt64());
			cgi->irb.CreateStore(fir::ConstantInt::getInt64(0), counter);

			cgi->irb.CreateUnCondBranch(loopcond);
			cgi->irb.setCurrentBlock(loopcond);

			// check
			fir::Value* cond = cgi->irb.CreateICmpLT(cgi->irb.CreateLoad(counter), len);
			cgi->irb.CreateCondBranch(cond, loopbody, loopmerge);


			cgi->irb.setCurrentBlock(loopbody);
			if(!(ptr->getType()->isPointerType() && ptr->getType()->getPointerElementType()->isDynamicArrayType()))
			{
				error("%s, %s, %d", ptr->getType()->str().c_str(), type->str().c_str(), nest);
			}

			// ok. first, do pointer arithmetic to get the current array
			fir::Value* arr = cgi->irb.CreatePointerAdd(ptr, cgi->irb.CreateLoad(counter));

			// next, get the data pointer and the length
			fir::Value* dptr = cgi->irb.CreateGetDynamicArrayData(arr);
			fir::Value* dlen = cgi->irb.CreateGetDynamicArrayLength(arr);
			fir::Value* dcap = cgi->irb.CreateGetDynamicArrayCapacity(arr);

			// now, call the next function with nest - 1.

			fir::Function* recursiveCallee = makeRecursiveDeallocFunction(cgi,
				type->getPointerElementType()->toDynamicArrayType()->getElementType()->getPointerTo(), nest - 1);

			iceAssert(recursiveCallee);

			fir::Value* skipFree = cgi->irb.CreateICmpEQ(dcap, fir::ConstantInt::getInt64(-1));
			cgi->irb.CreateCall3(recursiveCallee, dptr, dlen, skipFree);


			// set the length and data to 0
			auto zc = fir::ConstantInt::getInt64(0);

			cgi->irb.CreateSetDynamicArrayData(arr, fir::ConstantValue::getZeroValue(dptr->getType()));
			cgi->irb.CreateSetDynamicArrayLength(arr, zc);
			cgi->irb.CreateSetDynamicArrayCapacity(arr, zc);



			// increment counter
			cgi->irb.CreateStore(cgi->irb.CreateAdd(cgi->irb.CreateLoad(counter), fir::ConstantInt::getInt64(1)), counter);

			// branch to top
			cgi->irb.CreateUnCondBranch(loopcond);




			// merge:
			cgi->irb.setCurrentBlock(loopmerge);
			{
				// free the pointer anyway

				fir::IRBlock* dofree = cgi->irb.addNewBlockInFunction("doFree", func);
				fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

				cgi->irb.CreateCondBranch(top_skipFree, merge, dofree);
				cgi->irb.setCurrentBlock(dofree);
				{
					// just free
					fir::Function* freef = cgi->getOrDeclareLibCFunc(FREE_MEMORY_FUNC);
					iceAssert(freef);

					cgi->irb.CreateCall1(freef, cgi->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()));

					cgi->irb.CreateUnCondBranch(merge);
				}

				cgi->irb.setCurrentBlock(merge);
				cgi->irb.CreateReturnVoid();
			}




			// fir::Function* freef = cgi->getOrDeclareLibCFunc(FREE_MEMORY_FUNC);
			// iceAssert(freef);


			// cgi->irb.CreateCall1(freef, cgi->irb.CreatePointerTypeCast(ptr, fir::Type::getInt8Ptr()));
			// cgi->irb.CreateReturnVoid();
		}


		df = func;
		cgi->irb.setCurrentBlock(restore);
	}

	iceAssert(df);
	return df;
}


static void recursivelyDeallocate(CodegenInstance* cgi, fir::Value* val, fir::Value* ptr, Expr* user)
{
	iceAssert(ptr);
	if(val->getType()->isDynamicArrayType())
	{
		// need to loop through each element, and free the thing.
		// go to the lowest level

		int nest = 0;
		fir::Type* t = val->getType();
		while(t->isDynamicArrayType())
			t = t->toDynamicArrayType()->getElementType(), nest++;

		// ok, ptr is the pointer to the outermost array
		// get the data, get the length, and call.

		fir::Value* data = cgi->irb.CreateGetDynamicArrayData(ptr);
		fir::Value* len = cgi->irb.CreateGetDynamicArrayLength(ptr);
		fir::Value* cap = cgi->irb.CreateGetDynamicArrayCapacity(ptr);

		fir::Function* fn = makeRecursiveDeallocFunction(cgi, data->getType(), nest - 1);
		iceAssert(fn);

		fir::Value* skipFree = cgi->irb.CreateICmpEQ(cap, fir::ConstantInt::getInt64(-1));
		cgi->irb.CreateCall3(fn, data, len, skipFree);

		// set to 0
		auto zc = fir::ConstantInt::getInt64(0);

		cgi->irb.CreateSetDynamicArrayData(ptr, fir::ConstantValue::getZeroValue(data->getType()));
		cgi->irb.CreateSetDynamicArrayLength(ptr, zc);
		cgi->irb.CreateSetDynamicArrayCapacity(ptr, zc);
	}
	else if(val->getType()->isArraySliceType())
	{
		error(user, "Array slices (type '%s') do not own memory, and cannot be deallocated", val->getType()->str().c_str());
	}
	else if(!ptr->getType()->isPointerType())
	{
		error(user, "Cannot deallocate non-pointer type");
	}
	else if(val->getType()->isPointerType())
	{
		val = cgi->irb.CreatePointerTypeCast(val, fir::Type::getInt8Ptr());

		// call 'free'
		fir::Function* freef = cgi->getOrDeclareLibCFunc(FREE_MEMORY_FUNC);
		iceAssert(freef);

		cgi->irb.CreateCall1(freef, val);
	}
	else
	{
		error(user, "Cannot deallocate value of type '%s'", val->getType()->str().c_str());
	}
}

Result_t Dealloc::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	fir::Value* val = 0;
	fir::Value* ptr = 0;

	std::tie(val, ptr) = this->expr->codegen(cgi);

	recursivelyDeallocate(cgi, val, ptr, this);

	return Result_t(0, 0);
}

fir::Type* Dealloc::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return fir::Type::getVoid();
}




















