// ArrayDecomposition.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"
#include "runtimefuncs.h"

using namespace Codegen;
namespace Ast
{
	static void _doStore(CodegenInstance* cgi, Expr* user, fir::Type* elmType, fir::Value* src, fir::Value* dst, std::string name,
		Parser::Pin pos, ValueKind vk)
	{
		auto cmplxtype = cgi->getType(elmType);
		if(cmplxtype)
		{
			// todo: this leaks also
			auto res = Operators::performActualAssignment(cgi, user, new VarRef(pos, name),
				0, ArithmeticOp::Assign, cgi->irb.CreateLoad(dst), dst, cgi->irb.CreateLoad(src), src, vk);

			// it's stored already, no need to do shit.
			iceAssert(res.value);
		}
		else
		{
			// ok, just do it normally
			cgi->irb.CreateStore(cgi->irb.CreateLoad(src), dst);
		}

		if(cgi->isRefCountedType(elmType))
		{
			// (isInit = true, doAssign = false -- we already assigned it above)
			cgi->assignRefCountedExpression(new VarRef(pos, name), cgi->irb.CreateLoad(src), src,
				cgi->irb.CreateLoad(dst), dst, vk, true, false);
		}
	}



	Result_t ArrayDecompDecl::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
	{
		// ok. first, we need to codegen, and get the type of, the right side.
		fir::Value* rhs = 0; fir::Value* rhsptr = 0; ValueKind vk;
		std::tie(rhs, rhsptr, vk) = this->rightSide->codegen(cgi);
		iceAssert(rhs);

		// make some shit up.
		if(!rhsptr) rhsptr = cgi->irb.CreateImmutStackAlloc(rhs->getType(), rhs);

		// ok.
		fir::Type* rtype = rhs->getType();

		// ok, first check the number of named bindings, excluding the ellipsis
		bool haveNamedEllipsis = (this->mapping.find(-1) != this->mapping.end());
		size_t numNormalBindings = this->mapping.size() - (haveNamedEllipsis ? 1 : 0);

		if(rtype->isArrayType())
		{
			auto arrtype = rtype->toArrayType();
			auto elmtype = arrtype->getElementType();

			// well, we can check this at compile time
			if(numNormalBindings > arrtype->getArraySize())
			{
				error(this, "Too many bindings in array decomposition; array only has %zu elements, wanted at least %zu",
					arrtype->getArraySize(), numNormalBindings);
			}

			// ok, check if we have a named last binding, and if it's gonna be empty
			// this is only relevant for fixed-size arrays, anyway
			if(haveNamedEllipsis && numNormalBindings == arrtype->getArraySize())
				warn(this, "Named binding for remaining elements in array will be empty");

			// ok, all is good now.
			for(size_t i = 0; i < numNormalBindings; i++)
			{
				bool isPtr = false;

				std::string name = this->mapping[i].first;
				if(name == "_")
					continue;

				if(name[0] == '&')
				{
					isPtr = true;
					name = name.substr(1);
				}

				if(cgi->isDuplicateSymbol(name))
					GenError::duplicateSymbol(cgi, new VarRef(this->mapping[i].second, name), name, SymbolType::Variable);

				fir::Value* ai = cgi->irb.CreateStackAlloc(isPtr ? elmtype->getPointerTo() : elmtype);

				if(isPtr)
				{
					// check refcounted
					if(cgi->isRefCountedType(elmtype))
					{
						error(new DummyExpr(this->mapping[i].second), "Cannot bind to refcounted type '%s' by reference",
							elmtype->str().c_str());
					}

					fir::Value* ptr = cgi->irb.CreateConstGEP2(rhsptr, 0, i);
					if(vk != ValueKind::LValue)
						error(new DummyExpr(this->mapping[i].second), "Cannot take the address of an rvalue");

					cgi->irb.CreateStore(ptr, ai);

					if(this->immutable || ptr->isImmutable())
						ai->makeImmutable();
				}
				else
				{
					// get the value from the array
					fir::Value* ptr = cgi->irb.CreateConstGEP2(rhsptr, 0, i);
					_doStore(cgi, this, elmtype, ptr, ai, name, this->mapping[i].second, vk);
				}

				if(this->immutable)
					ai->makeImmutable();

				// add.
				VarDecl* fakeDecl = new VarDecl(this->mapping[i].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = ai->getType()->getPointerElementType();

				cgi->addSymbol(name, ai, fakeDecl);
			}


			// ok, handle the last thing
			// get the number of elms we need
			if(haveNamedEllipsis)
			{
				bool isPtr = false;

				size_t needed = arrtype->getArraySize() - numNormalBindings;
				fir::Type* at = fir::ArrayType::get(elmtype, needed);

				// ok.
				std::string name = this->mapping[-1].first;

				if(name[0] == '&')
				{
					isPtr = true;
					name = name.substr(1);
				}


				if(cgi->isDuplicateSymbol(name))
					GenError::duplicateSymbol(cgi, new VarRef(this->mapping[-1].second, name), name, SymbolType::Variable);

				fir::Value* ai = 0;

				if(isPtr)
				{
					// this actually isn't possible, unless we get slices.
					// ok, make a slice
					ai = cgi->irb.CreateStackAlloc(fir::ArraySliceType::get(elmtype));

					// get the pointer to the first elm that goes in here
					fir::Value* ptr = cgi->irb.CreateConstGEP2(rhsptr, 0, numNormalBindings);

					// ok, this is our data.
					// the length:

					fir::Value* len = fir::ConstantInt::getInt64(needed);

					// set that shit up
					cgi->irb.CreateSetArraySliceData(ai, ptr);
					cgi->irb.CreateSetArraySliceLength(ai, len);


					if(cgi->isRefCountedType(elmtype))
					{
						// increment the refcounts for the strings
						fir::Function* incrfn = RuntimeFuncs::Array::getIncrementArrayRefCountFunction(cgi, elmtype);
						iceAssert(incrfn);

						cgi->irb.CreateCall2(incrfn, ptr, len);
					}



					// check immutability
					if(this->immutable || ptr->isImmutable())
						ai->makeImmutable();
				}
				else
				{
					ai = cgi->irb.CreateStackAlloc(at);
					for(size_t i = numNormalBindings; i < arrtype->getArraySize(); i++)
					{
						fir::Value* srcptr = cgi->irb.CreateConstGEP2(rhsptr, 0, i);
						fir::Value* dstptr = cgi->irb.CreateConstGEP2(ai, 0, i - numNormalBindings);

						_doStore(cgi, this, elmtype, srcptr, dstptr, name, this->mapping[i].second, vk);
					}
				}

				iceAssert(ai);

				if(this->immutable)
					ai->makeImmutable();

				// add it.
				VarDecl* fakeDecl = new VarDecl(this->mapping[-1].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = ai->getType()->getPointerElementType();

				cgi->addSymbol(name, ai, fakeDecl);
			}
		}
		else if(rtype->isDynamicArrayType() || rtype->isArraySliceType())
		{
			bool isSlice = rtype->isArraySliceType();
			// todo: question should the 'remaining' bit of the array be a copy or reference?
			// it'll be a copy for now.

			fir::Value* ptr = (isSlice ? cgi->irb.CreateGetArraySliceData(rhsptr) : cgi->irb.CreateGetDynamicArrayData(rhsptr));
			fir::Value* len = (isSlice ? cgi->irb.CreateGetArraySliceLength(rhsptr) : cgi->irb.CreateGetDynamicArrayLength(rhsptr));
			fir::Type* elmtype = (isSlice ? rtype->toArraySliceType()->getElementType() : rtype->toDynamicArrayType()->getElementType());

			iceAssert(ptr && len);


			// ok... first, do a length check
			{
				fir::Function* checkf = RuntimeFuncs::Array::getBoundsCheckFunction(cgi, true);
				iceAssert(checkf);

				// this uses index, so we just kinda fudge it.
				cgi->irb.CreateCall3(checkf, len, fir::ConstantInt::getInt64(numNormalBindings),
					fir::ConstantString::get(Parser::pinToString(this->pin)));
			}


			// okay, now, loop + copy
			for(size_t i = 0; i < numNormalBindings; i++)
			{
				bool isPtr = false;
				std::string name = this->mapping[i].first;
				if(name == "_")
					continue;

				if(name[0] == '&')
				{
					isPtr = true;
					name = name.substr(1);
				}

				// no need to bounds check here.
				fir::Value* gep = cgi->irb.CreateGetPointer(ptr, fir::ConstantInt::getInt64(i));
				fir::Value* ai = cgi->irb.CreateStackAlloc(isPtr ? elmtype->getPointerTo() : elmtype);

				if(isPtr)
				{
					// check refcounted
					if(cgi->isRefCountedType(elmtype))
					{
						error(new DummyExpr(this->mapping[i].second), "Cannot bind to refcounted type '%s' by reference",
							elmtype->str().c_str());
					}

					if(vk != ValueKind::LValue)
						error(new DummyExpr(this->mapping[i].second), "Cannot take the address of an rvalue");

					cgi->irb.CreateStore(gep, ai);

					if(this->immutable || rhsptr->isImmutable())
						ai->makeImmutable();
				}
				else
				{
					// do the store-y thing
					_doStore(cgi, this, elmtype, gep, ai, name, this->mapping[i].second, vk);
				}

				if(this->immutable)
					ai->makeImmutable();

				VarDecl* fakeDecl = new VarDecl(this->mapping[i].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = ai->getType()->getPointerElementType();

				cgi->addSymbol(name, ai, fakeDecl);
			}


			if(haveNamedEllipsis)
			{
				bool isPtr = false;
				std::string name = this->mapping[-1].first;

				if(name[0] == '&')
				{
					isPtr = true;
					name = name.substr(1);
				}

				fir::Value* ai = 0;


				if(isPtr)
				{
					// make slicey
					ai = cgi->irb.CreateStackAlloc(fir::ArraySliceType::get(elmtype));

					// get the pointer to the first elm that goes in here
					fir::Value* newptr = cgi->irb.CreatePointerAdd(ptr, fir::ConstantInt::getInt64(numNormalBindings));

					// ok, this is our data.
					// the length:

					fir::Value* newlen = cgi->irb.CreateSub(len, fir::ConstantInt::getInt64(numNormalBindings));

					// set that shit up
					cgi->irb.CreateSetArraySliceData(ai, newptr);
					cgi->irb.CreateSetArraySliceLength(ai, newlen);


					if(cgi->isRefCountedType(elmtype))
					{
						// increment the refcounts for the strings
						fir::Function* incrfn = RuntimeFuncs::Array::getIncrementArrayRefCountFunction(cgi, elmtype);
						iceAssert(incrfn);

						cgi->irb.CreateCall2(incrfn, ptr, len);
					}


					// check immutability
					if(this->immutable || rhsptr->isImmutable())
						ai->makeImmutable();
				}
				else
				{
					ai = cgi->irb.CreateStackAlloc(rtype->toDynamicArrayType());

					// so, we have a nice function to clone an array, and it now takes a starting index
					// et voila, problem solved.

					fir::Function* clonef = RuntimeFuncs::Array::getCloneFunction(cgi, rtype->toDynamicArrayType());
					iceAssert(clonef);

					fir::Value* clone = cgi->irb.CreateCall2(clonef, rhsptr, fir::ConstantInt::getInt64(numNormalBindings));

					// well, there we go. that's the clone, store that shit.
					cgi->irb.CreateStore(clone, ai);
				}

				iceAssert(ai);
				if(this->immutable)
					ai->makeImmutable();

				VarDecl* fakeDecl = new VarDecl(this->mapping[-1].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = ai->getType()->getPointerElementType();

				cgi->addSymbol(name, ai, fakeDecl);
			}
		}
		else
		{
			error(this->rightSide, "Expected array type on right side of array decomposition, have '%s'", rtype->str().c_str());
		}

		// there's no one value...
		return Result_t(0, 0);
	}

	fir::Type* ArrayDecompDecl::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
	{
		// there's no one type...
		error(this, "Decomposing declarations do not yield a value");
	}
}














