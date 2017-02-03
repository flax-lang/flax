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



	Result_t ArrayDecompDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
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
				std::string name = this->mapping[i].first;
				if(name == "_")
					continue;

				if(cgi->isDuplicateSymbol(name))
					GenError::duplicateSymbol(cgi, new VarRef(this->mapping[i].second, name), name, SymbolType::Variable);

				// ok. make it.
				VarDecl* fakeDecl = new VarDecl(this->mapping[i].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = elmtype;

				fir::Value* ai = cgi->irb.CreateStackAlloc(elmtype);

				// get the value from the array
				fir::Value* ptr = cgi->irb.CreateConstGEP2(rhsptr, 0, i);
				_doStore(cgi, this, elmtype, ptr, ai, name, this->mapping[i].second, vk);

				if(this->immutable)
					ai->makeImmutable();

				// add.
				cgi->addSymbol(name, ai, fakeDecl);
			}

			// ok, handle the last thing
			// get the number of elms we need
			if(haveNamedEllipsis)
			{
				size_t needed = arrtype->getArraySize() - numNormalBindings;
				fir::Type* at = fir::ArrayType::get(elmtype, needed);

				// ok.
				std::string name = this->mapping[-1].first;

				if(cgi->isDuplicateSymbol(name))
					GenError::duplicateSymbol(cgi, new VarRef(this->mapping[-1].second, name), name, SymbolType::Variable);

				// ok make it
				fir::Value* ai = cgi->irb.CreateStackAlloc(at);

				for(size_t i = numNormalBindings; i < arrtype->getArraySize(); i++)
				{
					fir::Value* srcptr = cgi->irb.CreateConstGEP2(rhsptr, 0, i);
					fir::Value* dstptr = cgi->irb.CreateConstGEP2(ai, 0, i - numNormalBindings);

					_doStore(cgi, this, elmtype, srcptr, dstptr, name, this->mapping[i].second, vk);
				}

				if(this->immutable)
					ai->makeImmutable();

				// add it.
				VarDecl* fakeDecl = new VarDecl(this->mapping[-1].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = at;

				cgi->addSymbol(name, ai, fakeDecl);
			}
		}
		else if(rtype->isDynamicArrayType())
		{
			// todo: question should the 'remaining' bit of the array be a copy or reference?
			// it'll be a copy for now.

			fir::Value* ptr = cgi->irb.CreateGetDynamicArrayData(rhsptr);
			fir::Value* len = cgi->irb.CreateGetDynamicArrayLength(rhsptr);
			fir::Type* elmType = rtype->toDynamicArrayType()->getElementType();

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
				std::string name = this->mapping[i].first;
				if(name == "_")
					continue;

				// no need to bounds check here.
				fir::Value* gep = cgi->irb.CreateGetPointer(ptr, fir::ConstantInt::getInt64(i));
				fir::Value* ai = cgi->irb.CreateStackAlloc(elmType);

				// do the store-y thing
				_doStore(cgi, this, elmType, gep, ai, name, this->mapping[i].second, vk);

				if(this->immutable)
					ai->makeImmutable();

				VarDecl* fakeDecl = new VarDecl(this->mapping[i].second, name, this->immutable);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = elmType;

				cgi->addSymbol(name, ai, fakeDecl);
			}


			if(haveNamedEllipsis)
			{
				// so, this is the fucked up part
			}
		}
		else
		{
			error(this->rightSide, "Expected array type on right side of array decomposition, have '%s'", rtype->str().c_str());
		}

		// there's no one value...
		return Result_t(0, 0);
	}

	fir::Type* ArrayDecompDecl::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
	{
		// there's no one type...
		error(this, "Decomposing declarations do not yield a value");
	}
}














