// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)

namespace sst
{
	static int getCastDistance(fir::Type* from, fir::Type* to)
	{
		if(from == to) return 0;

		if(from->isConstantNumberType())
		{
			auto num = from->toConstantNumberType()->getValue();
			if(mpfr::isint(num) && to->isIntegerType())
				return 0;

			else if(mpfr::isint(num) && to->isFloatingPointType())
				return 1;

			else
				return 1;
		}
		else if(from->isIntegerType() && to->isIntegerType())
		{
			auto bitdiff = abs((int) from->toPrimitiveType()->getIntegerBitWidth() - (int) to->toPrimitiveType()->getIntegerBitWidth());

			switch(bitdiff)
			{
				case 0:		return 0;	// same
				case 8:		return 1;	// i16 - i8
				case 16:	return 1;	// i32 - i16
				case 32:	return 1;	// i64 - i32

				case 24:	return 2;	// i32 - i8
				case 48:	return 2;	// i64 - i16

				case 56:	return 3;	// i64 - i8
				default:	iceAssert(0);
			}
		}
		else if(from->isFloatingPointType() && to->isFloatingPointType())
		{
			return 1;
		}
		else if(from->isStringType() && to == fir::Type::getInt8Ptr())
		{
			return 4;
		}

		return -1;
	}

	using Param = FunctionDecl::Param;
	static int computeOverloadDistance(const Location& fnLoc, std::vector<fir::Type*> target, std::vector<Location> targetLocs,
		std::vector<fir::Type*> args, bool cvararg, Location* loc, std::string* estr)
	{
		iceAssert(estr);
		if(target.empty() && args.empty())
			return 0;

		bool anyvararg = cvararg || (target.size() > 0 && target.back()->isVariadicArrayType());

		if(!anyvararg && target.size() != args.size())
		{
			*estr = strprintf("Mismatched number of arguments; expected %zu, got %zu", target.size(), args.size());
			*loc = fnLoc;
			return -1;
		}
		else if(anyvararg && args.size() < target.size())
		{
			*estr = strprintf("Too few arguments; need at least %zu even if variadic arguments are empty", target.size());
			*loc = fnLoc;
			return -1;
		}

		int distance = 0;
		for(size_t i = 0; i < target.size(); i++)
		{
			auto d = getCastDistance(args[i], target[i]);
			if(d == -1)
			{
				*estr = strprintf("Mismatched argument type in argument %zu: no valid cast from given type '%s' to expected type '%s'",
					i, args[i]->str(), target[i]->str());
				*loc = targetLocs[i];

				return -1;
			}
			else
			{
				distance += d;
			}
		}

		// means we're a flax-variadic function
		// thus we need to actually check the types.
		if(anyvararg && !cvararg)
		{
			// first, check if we can do a direct-passthrough
			if(args.size() == target.size() && (args.back()->isVariadicArrayType() || args.back()->isDynamicArrayType()))
			{
				// yes we can
				auto a = args.back()->getArrayElementType();
				auto t = target.back()->getArrayElementType();

				if(a != t)
				{
					*estr = strprintf("Mismatched element type in variadic array passthrough; expected '%s', got '%s'",
						t->str(), a->str());
					*loc = targetLocs.back();
					return -1;
				}
				else
				{
					distance += 0;
				}
			}
			else
			{
				auto elmTy = target.back()->getArrayElementType();
				for(size_t i = target.size(); i < args.size(); i++)
				{
					auto ty = args[i];
					auto dist = getCastDistance(ty, elmTy);
					if(dist == -1)
					{
						*estr = strprintf("Mismatched type in variadic argument; no valid cast from given type '%s' to expected type '%s' (ie. element type of variadic parameter list)", ty->str(), elmTy->str());
						*loc = targetLocs.back();
						return -1;
					}

					distance += dist;
				}
			}
		}

		return distance;
	}

	Defn* TypecheckState::resolveFunctionFromCandidates(std::vector<Defn*> cands, std::vector<Param> arguments,
		PrettyError* errs)
	{
		iceAssert(errs);

		using Param = FunctionDefn::Param;
		iceAssert(cands.size() > 0);

		int bestDist = INT_MAX;
		std::vector<Defn*> finals;
		std::map<Defn*, std::pair<Location, std::string>> fails;

		for(auto cand : cands)
		{
			int dist = 0;

			if(auto fn = dcast(FunctionDecl, cand))
			{
				auto args = util::map(arguments, [](Param p) { return p.type; });

				if(auto def = dcast(FunctionDefn, fn); def && def->parentTypeForMethod != 0)
					args.insert(args.begin(), def->parentTypeForMethod->getPointerTo());

				dist = computeOverloadDistance(cand->loc, util::map(fn->params, [](Param p) { return p.type; }),
					util::map(fn->params, [](Param p) { return p.loc; }),
					args, fn->isVarArg, &fails[fn].first, &fails[fn].second);
			}
			else if(auto vr = dcast(VarDefn, cand))
			{
				iceAssert(vr->type->isFunctionType());
				auto ft = vr->type->toFunctionType();

				auto prms = ft->getArgumentTypes();
				dist = computeOverloadDistance(cand->loc, prms, std::vector<Location>(prms.size(), vr->loc),
					util::map(arguments, [](Param p) { return p.type; }), false, &fails[vr].first, &fails[vr].second);
			}

			if(dist == -1)
				continue;

			else if(dist < bestDist)
				finals.clear(), finals.push_back(cand), bestDist = dist;

			else if(dist == bestDist)
				finals.push_back(cand);
		}

		if(finals.empty())
		{
			std::vector<fir::Type*> tmp;
			std::transform(arguments.begin(), arguments.end(), std::back_inserter(tmp), [](Param p) -> auto { return p.type; });

			errs->errorStr += strbold("No overload of function '%s' matching given argument types '%s' amongst %zu candidate%s",
				cands[0]->id.name, fir::Type::typeListToString(tmp), fails.size(), fails.size() == 1 ? "" : "s");

			for(auto f : fails)
				errs->infoStrs.push_back({ f.first->loc, strinfo(f.second.first, "Candidate not viable: %s", f.second.second) });

			return 0;
		}
		else if(finals.size() > 1)
		{
			errs->errorStr += strbold("Ambiguous call to function '%s', have %zu candidates:", cands[0]->id.name, finals.size());

			for(auto f : finals)
				errs->infoStrs.push_back({ f->loc, strinfo(f, "Possible target") });

			return 0;
		}
		else
		{
			return finals[0];
		}
	}

	Defn* TypecheckState::resolveFunction(std::string name, std::vector<Param> arguments, PrettyError* errs)
	{
		iceAssert(errs);

		// return this->resolveFunctionFromCandidates(fs, arguments, errs);

		// we kinda need to check manually, since... we need to give a good error message
		// when a shadowed thing is not a function

		std::vector<Defn*> fns;
		StateTree* tree = this->stree;

		bool didVar = false;
		while(tree)
		{
			auto defs = tree->definitions[name];
			for(auto def : defs)
			{
				// warn(def, "%p, %s", def->type, def->type->str());
				if(auto fn = dcast(FunctionDecl, def))
				{
					fns.push_back(fn);
				}
				else if(auto vr = dcast(VarDefn, def); vr->type->isFunctionType())
				{
					// ok, we'll check it later i guess.
					if(!didVar)
						fns.push_back(vr);

					didVar = true;
				}
				else
				{
					didVar = true;
					exitless_error(this->loc(), "'%s' cannot be called as a function; it was defined with type '%s' in the current scope",
						name, def->type->str());

					info(def, "Previously defined here:");

					doTheExit();
				}
			}

			tree = tree->parent;
		}

		if(fns.empty())
			error(this->loc(), "No such function named '%s'", name);

		return this->resolveFunctionFromCandidates(fns, arguments, errs);
	}
}





sst::Expr* ast::FunctionCall::typecheck(TCS* fs, fir::Type* inferred)
{
	using Param = sst::FunctionDecl::Param;

	fs->pushLoc(this);
	defer(fs->popLoc());

	sst::Defn* target = 0;
	std::vector<sst::Expr*> arguments;

	for(auto p : this->args)
	{
		auto expr = p->typecheck(fs);
		arguments.push_back(expr);
	}


	// resolve the function call here
	sst::TypecheckState::PrettyError errs;

	std::vector<Param> ts;
	std::transform(arguments.begin(), arguments.end(), std::back_inserter(ts), [](sst::Expr* e) -> auto {
		return Param { .type = e->type, .loc = e->loc };
	});

	target = fs->resolveFunction(this->name, ts, &errs);
	if(!errs.errorStr.empty())
	{
		exitless_error(this, "%s", errs.errorStr);
		for(auto inf : errs.infoStrs)
			fprintf(stderr, "%s", inf.second.c_str());

		doTheExit();
	}

	iceAssert(target);
	iceAssert(target->type->isFunctionType());

	auto ret = new sst::FunctionCall(this->loc, target->type->toFunctionType()->getReturnType());
	ret->name = this->name;
	ret->target = target;
	ret->arguments = arguments;

	return ret;
}










sst::Expr* ast::ExprCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	using Param = sst::FunctionDecl::Param;

	fs->pushLoc(this);
	defer(fs->popLoc());

	std::vector<sst::Expr*> arguments;

	for(auto p : this->args)
	{
		auto expr = p->typecheck(fs);
		arguments.push_back(expr);
	}

	std::vector<Param> ts = util::map(arguments, [](sst::Expr* e) -> auto { return Param { .type = e->type, .loc = e->loc }; });
	std::vector<fir::Type*> tys = util::map(arguments, [](sst::Expr* e) -> auto { return e->type; });

	auto target = this->callee->typecheck(fs);
	iceAssert(target);

	if(!target->type->isFunctionType())
		error(this->callee, "Expression with non-function-type '%s' cannot be called");

	Location eloc;
	std::string estr;

	auto ft = target->type->toFunctionType();
	int dist = sst::computeOverloadDistance(this->loc, ft->getArgumentTypes(), std::vector<Location>(),
		tys, false, &eloc, &estr);

	if(!estr.empty() || dist == -1)
		error(eloc, "%s", estr);


	auto ret = new sst::ExprCall(this->loc, target->type->toFunctionType()->getReturnType());
	ret->callee = target;
	ret->arguments = arguments;

	return ret;
}








