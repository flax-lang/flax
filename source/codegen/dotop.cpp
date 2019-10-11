// dotops.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"
#include "memorypool.h"

static bool isAutoDereferencable(fir::Type* t)
{
	return (t->isStructType() || t->isClassType() || t->isRawUnionType());
}

static CGResult getAppropriateValuePointer(cgn::CodegenState* cs, sst::Expr* user, sst::Expr* lhs, fir::Type** baseType)
{
	auto res = lhs->codegen(cs);
	auto restype = res.value->getType();

	fir::Value* retv = 0;

	if(isAutoDereferencable(restype))
	{
		auto t = res.value->getType();
		iceAssert(isAutoDereferencable(t));

		retv = res.value;
		*baseType = restype;
	}
	else if(restype->isTupleType())
	{
		retv = res.value;
		*baseType = restype;
	}
	else if(restype->isPointerType() && isAutoDereferencable(restype->getPointerElementType()))
	{
		iceAssert(isAutoDereferencable(res.value->getType()->getPointerElementType()));

		retv = cs->irb.Dereference(res.value);
		*baseType = restype->getPointerElementType();
	}
	else
	{
		error(user, "invalid type '%s' for instance dot op", restype);
	}

	return CGResult(retv);
}




CGResult sst::MethodDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());


	if(auto fc = dcast(sst::FunctionCall, this->call))
	{
		// basically what we need to do is just get the pointer
		fir::Type* sty = 0;
		auto res = getAppropriateValuePointer(cs, this, this->lhs, &sty);

		if(!res->islvalue())
		{
			auto tmplval = cs->irb.CreateLValue(this->lhs->type);
			cs->irb.Store(res.value, tmplval);

			res.value = tmplval;
			res->makeConst();
		}

		// then we insert it as the first argument
		auto rv = util::pool<sst::RawValueExpr>(this->loc, res.value->getType()->getMutablePointerTo());
		rv->rawValue = CGResult(cs->irb.AddressOf(res.value, true));

		//! SELF HANDLING (INSERTION) (CODEGEN)
		fc->arguments.insert(fc->arguments.begin(), FnCallArgument(this->loc, "this", rv, 0));
		return fc->codegen(cs);
	}
	else if(auto ec = dcast(sst::ExprCall, this->call))
	{
		return ec->codegen(cs);
	}
	else
	{
		error(this->call, "what?");
	}
}




CGResult sst::FieldDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->isMethodRef)
		error("method ref not supported");

	fir::Type* sty = 0;
	auto res = getAppropriateValuePointer(cs, this, this->lhs, &sty);

	// TODO: clean up the code dupe here
	if(this->isTransparentField)
	{
		iceAssert(this->lhs->type->isRawUnionType() || this->lhs->type->isStructType());
		if(this->lhs->type->isRawUnionType())
		{
			fir::Value* field = 0;
			if(res->islvalue())
			{
				field = cs->irb.GetRawUnionFieldByType(res.value, this->type);
			}
			else
			{
				auto addr = cs->irb.ImmutStackAlloc(this->lhs->type, res.value);
				field = cs->irb.GetRawUnionFieldByType(addr, this->type);
			}

			return CGResult(field);
		}
		else
		{
			if(res->islvalue())
			{
				// ok, at this point it's just a normal, instance field.
				return CGResult(cs->irb.StructGEP(res.value, this->indexOfTransparentField));
			}
			else
			{
				// use extractvalue.
				return CGResult(cs->irb.ExtractValue(res.value, { this->indexOfTransparentField }));
			}
		}
	}
	else
	{
		if(this->lhs->type->isRawUnionType())
		{
			fir::Value* field = 0;
			if(res->islvalue())
			{
				field = cs->irb.GetRawUnionField(res.value, this->rhsIdent);
			}
			else
			{
				auto addr = cs->irb.ImmutStackAlloc(this->lhs->type, res.value);
				field = cs->irb.GetRawUnionField(addr, this->rhsIdent);
			}

			return CGResult(field);
		}
		else
		{
			if(res->islvalue())
			{
				// ok, at this point it's just a normal, instance field.
				return CGResult(cs->irb.GetStructMember(res.value, this->rhsIdent));
			}
			else
			{
				// use extractvalue.
				return CGResult(cs->irb.ExtractValueByName(res.value, this->rhsIdent));
			}
		}
	}
}



CGResult sst::TupleDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Type* _sty = 0;
	auto res = getAppropriateValuePointer(cs, this, this->lhs, &_sty);

	fir::TupleType* tty = _sty->toTupleType();
	iceAssert(tty);

	// make sure something didn't somehow manage to fuck up -- we should've checked this in the typechecker.
	iceAssert(this->index < tty->getElementCount());

	// ok, if we have a pointer, then return an lvalue
	// if not, return an rvalue
	if(res->islvalue())
	{
		return CGResult(cs->irb.StructGEP(res.value, this->index));
	}
	else
	{
		return CGResult(cs->irb.ExtractValue(res.value, { this->index }));
	}
}



CGResult cgn::CodegenState::getStructFieldImplicitly(std::string name)
{
	fir::Value* self = this->getMethodSelf();
	auto ty = self->getType();

	auto dothing = [this, name, self](auto sty) -> auto {

		if(sty->hasElementWithName(name))
		{
			// ok -- return directly from here.
			fir::Value* ptr = this->irb.GetStructMember(self, name);
			return CGResult(ptr);
		}
		else
		{
			error(this->loc(), "type '%s' has no field named '%s'", sty->getTypeName().str(), name);
		}
	};

	if(ty->isStructType())
		return dothing(ty->toStructType());

	else if(ty->isClassType())
		return dothing(ty->toClassType());

	else
		error(this->loc(), "invalid self type '%s' for field named '%s'", ty, name);
}
