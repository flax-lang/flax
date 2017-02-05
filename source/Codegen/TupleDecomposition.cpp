// TupleDecomposition.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Codegen;
using Mapping = Ast::TupleDecompDecl::Mapping;

namespace Ast
{
	static void recursivelyDestructureTuple(CodegenInstance* cgi, Expr* user, Mapping mapping, fir::Value* rhs, fir::Value* rhsptr,
		ValueKind vk, bool immut)
	{
		// ok, first...
		if(mapping.isRecursive)
		{
			auto type = rhs->getType();
			iceAssert(type->toTupleType());

			size_t maxElm = type->toTupleType()->getElementCount();
			if(mapping.inners.size() != type->toTupleType()->getElementCount())
			{
				// check if the last one is an ellipsis -- if it is, treat the _ as a gobbler and ignore the rest
				// of course, this implies that we need to have more elements on the right than on the left.

				iceAssert(mapping.inners.size() > 0);

				// ok, check if the last one is an underscore
				if(!mapping.inners.back().isRecursive && mapping.inners.back().name == "..." && mapping.inners.size() <= type->toTupleType()->getElementCount())
				{
					maxElm = mapping.inners.size() - 1;
				}
				else
				{
					// todo: this leaks
					error(new DummyExpr(mapping.pos), "Mismatched element count; binding has %zu members, but tuple "
						"on the right side has %zu members", mapping.inners.size(), type->toTupleType()->getElementCount());
				}
			}

			size_t counter = 0;
			iceAssert(rhsptr);

			for(auto m : mapping.inners)
			{
				if(counter == maxElm) break;

				// first, get the thing
				fir::Value* ptr = cgi->irb.CreateStructGEP(rhsptr, counter);
				fir::Value* val = cgi->irb.CreateLoad(ptr);

				// send it off.
				recursivelyDestructureTuple(cgi, user, m, val, ptr, vk, immut);

				counter++;
			}
		}
		else
		{
			bool isPtr = false;
			std::string name = mapping.name;

			if(name[0] == '&')
			{
				isPtr = true;
				name = name.substr(1);
			}

			// only do something if we're not ignoring it.
			if(mapping.name != "_")
			{

				// todo: this leaks.
				if(cgi->isDuplicateSymbol(name))
					GenError::duplicateSymbol(cgi, new VarRef(mapping.pos, name), name, SymbolType::Variable);

				// ok, declare it first.
				fir::Value* ai = cgi->getStackAlloc(isPtr ? rhs->getType()->getPointerTo() : rhs->getType(), name);
				iceAssert(ai);

				if(isPtr)
				{
					// well, in this case we don't need to do any of the weird stuff, we just kinda store it.
					// first check if we're refcounted

					if(cgi->isRefCountedType(rhs->getType()))
					{
						// todo: leaks
						error(new DummyExpr(mapping.pos), "Cannot bind to refcounted type '%s' by reference",
							rhs->getType()->str().c_str());
					}

					// ok, then check for rvalue/lvalue nonsense
					if(vk != ValueKind::LValue)
						error(new DummyExpr(mapping.pos), "Cannot take the address of an rvalue");


					// ok, now do it.
					iceAssert(rhsptr);

					// well yea that's just it.
					cgi->irb.CreateStore(rhsptr, ai);

					if(immut || rhsptr->isImmutable())
						ai->makeImmutable();
				}
				else
				{
					// ok, set it.
					// check if we're some special snowflake
					auto cmplxtype = cgi->getType(rhs->getType());

					if(cmplxtype)
					{
						// todo: this leaks also
						auto res = Operators::performActualAssignment(cgi, user, new VarRef(mapping.pos, name), 0, ArithmeticOp::Assign,
							cgi->irb.CreateLoad(ai), ai, rhs, rhsptr, vk);

						// it's stored already, no need to do shit.
						iceAssert(res.value);
					}
					else
					{
						// ok, just do it normally
						cgi->irb.CreateStore(rhs, ai);
					}

					if(cgi->isRefCountedType(rhs->getType()))
					{
						// (isInit = true, doAssign = false -- we already assigned it above)
						cgi->assignRefCountedExpression(new VarRef(mapping.pos, name), rhs, rhsptr, cgi->irb.CreateLoad(ai), ai, vk, true, false);
					}
				}

				if(immut) ai->makeImmutable();

				// add it.
				// but first, make a fake decl
				VarDecl* fakeDecl = new VarDecl(mapping.pos, name, immut);
				fakeDecl->didCodegen = true;
				fakeDecl->concretisedType = rhs->getType();

				cgi->addSymbol(name, ai, fakeDecl);
			}
		}
	}

	Result_t TupleDecompDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
	{
		// ok. first, we need to codegen, and get the type of, the right side.
		fir::Value* rhs = 0; fir::Value* rhsptr = 0; ValueKind vk;
		std::tie(rhs, rhsptr, vk) = this->rightSide->codegen(cgi);
		iceAssert(rhs);

		// make some shit up.
		if(!rhsptr)
			rhsptr = cgi->irb.CreateImmutStackAlloc(rhs->getType(), rhs);

		// ok.
		fir::Type* _rtype = rhs->getType();
		if(!_rtype->isTupleType())
			error(this->rightSide, "Right side of tuple decomposition must, clearly, be a tuple type; have '%s'", _rtype->str().c_str());

		// ok.
		fir::TupleType* tt = _rtype->toTupleType();
		iceAssert(tt);

		// right. now...
		recursivelyDestructureTuple(cgi, this, this->mapping, rhs, rhsptr, vk, this->immutable);

		// there's no one value...
		return Result_t(0, 0);
	}

	fir::Type* TupleDecompDecl::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
	{
		// there's no one type...
		error(this, "Decomposing declarations do not yield a value");
	}
}
