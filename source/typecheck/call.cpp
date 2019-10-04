// call.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"
#include "errors.h"
#include "typecheck.h"

#include "resolver.h"

#include "ir/type.h"

#include "memorypool.h"

sst::Expr* ast::FunctionCall::typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& _arguments, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(auto ty = fs->checkIsBuiltinConstructorCall(this->name, _arguments))
	{
		auto ret = util::pool<sst::ExprCall>(this->loc, ty);
		ret->callee = sst::TypeExpr::make(this->loc, ty);
		ret->arguments = util::map(_arguments, [](const auto& e) -> sst::Expr* { return e.value; });

		return ret;
	}

	// resolve the function call here
	std::vector<FnCallArgument> ts = _arguments;

	auto res = sst::resolver::resolveFunctionCall(fs, this->loc, this->name, &ts, this->mappings, this->traverseUpwards, infer);

	auto target = res.defn();
	iceAssert(target);

	if(auto strdf = dcast(sst::StructDefn, target))
	{
		auto ret = util::pool<sst::StructConstructorCall>(this->loc, strdf->type);

		ret->target = strdf;
		ret->arguments = ts;

		return ret;
	}
	else if(auto uvd = dcast(sst::UnionVariantDefn, target))
	{
		auto unn = uvd->parentUnion;
		iceAssert(unn);

		auto ret = util::pool<sst::UnionVariantConstructor>(this->loc, unn->type);

		ret->variantId = unn->type->toUnionType()->getIdOfVariant(uvd->variantName);
		ret->parentUnion = unn;
		ret->args = ts;

		return ret;
	}
	else
	{
		iceAssert(target->type->isFunctionType());

		//* note: we check for this->name != "init" because when we explicitly call an init function, we don't want the extra stuff that
		//* comes with that -- we'll just treat it as a normal function call.
		if(auto fnd = dcast(sst::FunctionDefn, target); this->name != "init" && fnd && fnd->id.name == "init" && fnd->parentTypeForMethod && fnd->parentTypeForMethod->isClassType())
		{
			// ok, great... I guess?
			auto ret = util::pool<sst::ClassConstructorCall>(this->loc, fnd->parentTypeForMethod);

			ret->target = fnd;
			ret->arguments = ts;
			ret->classty = dcast(sst::ClassDefn, fs->typeDefnMap[fnd->parentTypeForMethod]);

			iceAssert(ret->target);

			return ret;
		}



		auto call = util::pool<sst::FunctionCall>(this->loc, target->type->toFunctionType()->getReturnType());
		call->name = this->name;
		call->target = target;
		call->arguments = ts;

		if(auto fd = dcast(sst::FunctionDefn, target); fd && fd->parentTypeForMethod)
		{
			// check if it's a method call
			// if so, indicate it. here, we set 'isImplicitMethodCall' to true, as an assumption.
			// in DotOp's typecheck, *after* calling this typecheck(), we set it back to false

			// so, if it was really an implicit call, it remains set
			// if it was a dot-op call, it gets set back to false by the dotop checking.

			call->isImplicitMethodCall = true;
		}

		return call;
	}
}










sst::Expr* ast::ExprCall::typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& arguments, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());


	auto target = this->callee->typecheck(fs).expr();
	iceAssert(target);

	if(!target->type->isFunctionType())
		error(this->callee, "expression with non-function-type '%s' cannot be called", target->type);

	auto ft = target->type->toFunctionType();
	auto [ dist, errs ] = sst::resolver::computeOverloadDistance(this->loc, util::map(ft->getArgumentTypes(), [](fir::Type* t) -> auto {
		return fir::LocatedType(t, Location());
	}), util::map(arguments, [](const FnCallArgument& fca) -> fir::LocatedType {
		return fir::LocatedType(fca.value->type, fca.loc);
	}), target->type->toFunctionType()->isCStyleVarArg(), this->loc);

	if(errs != nullptr || dist == -1)
	{
		auto x = SimpleError::make(this->loc, "mismatched types in call to function pointer");
		if(errs)    errs->prepend(x);
		else        errs = x;

		errs->postAndQuit();
	}

	auto ret = util::pool<sst::ExprCall>(this->loc, target->type->toFunctionType()->getReturnType());
	ret->callee = target;
	ret->arguments = util::map(arguments, [](const auto& e) -> sst::Expr* { return e.value; });

	return ret;
}


TCResult ast::ExprCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return TCResult(this->typecheckWithArguments(fs, sst::resolver::misc::typecheckCallArguments(fs, this->args), infer));
}

TCResult ast::FunctionCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return TCResult(this->typecheckWithArguments(fs, sst::resolver::misc::typecheckCallArguments(fs, this->args), infer));
}


