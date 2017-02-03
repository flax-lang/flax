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
				{
					auto cmplxtype = cgi->getType(elmtype);

					if(cmplxtype)
					{
						// todo: this leaks also
						auto res = Operators::performActualAssignment(cgi, this, new VarRef(this->mapping[i].second, name),
							0, ArithmeticOp::Assign, cgi->irb.CreateLoad(ai), ai, cgi->irb.CreateLoad(ptr), ptr, vk);

						// it's stored already, no need to do shit.
						iceAssert(res.value);
					}
					else
					{
						// ok, just do it normally
						cgi->irb.CreateStore(cgi->irb.CreateLoad(ptr), ai);
					}

					if(cgi->isRefCountedType(rhs->getType()))
					{
						// (isInit = true, doAssign = false -- we already assigned it above)
						cgi->assignRefCountedExpression(new VarRef(this->mapping[i].second, name), cgi->irb.CreateLoad(ptr), ptr,
							cgi->irb.CreateLoad(ai), ai, vk, true, false);
					}
				}

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


					auto cmplxtype = cgi->getType(elmtype);

					if(cmplxtype)
					{
						// todo: this leaks also
						auto res = Operators::performActualAssignment(cgi, this, new VarRef(this->mapping[-1].second, name),
							0, ArithmeticOp::Assign, cgi->irb.CreateLoad(dstptr), dstptr, cgi->irb.CreateLoad(srcptr), srcptr, vk);

						// it's stored already, no need to do shit.
						iceAssert(res.value);
					}
					else
					{
						// ok, just do it normally
						cgi->irb.CreateStore(cgi->irb.CreateLoad(srcptr), dstptr);
					}

					if(cgi->isRefCountedType(elmtype))
					{
						// (isInit = true, doAssign = false -- we already assigned it above)
						cgi->assignRefCountedExpression(new VarRef(this->mapping[-1].second, name), cgi->irb.CreateLoad(srcptr), srcptr,
							cgi->irb.CreateLoad(dstptr), dstptr, vk, true, false);
					}
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
		else if(rtype->isParameterPackType() || rtype->isDynamicArrayType())
		{
			// todo: question should the 'remaining' bit of the array be a copy or reference?
			// it'll be a copy for now.

			fir::Value* ptr = 0;
			fir::Value* len = 0;

			if(rtype->isParameterPackType())	ptr = cgi->irb.CreateGetParameterPackData(rhsptr);
			else								ptr = cgi->irb.CreateGetDynamicArrayData(rhsptr);

			if(rtype->isParameterPackType())	len = cgi->irb.CreateGetParameterPackLength(rhsptr);
			else								len = cgi->irb.CreateGetDynamicArrayLength(rhsptr);

			iceAssert(ptr && len);


			// ok... first, do a length check
			{
				fir::Function* checkf = RuntimeFuncs::Array::getBoundsCheckFunction(cgi, true);
				iceAssert(checkf);

				// this uses index, so we just kinda fudge it.
				cgi->irb.CreateCall3(checkf, len, fir::ConstantInt::getInt64(numNormalBindings),
					fir::ConstantString::get(Parser::pinToString(this->pin)));
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
