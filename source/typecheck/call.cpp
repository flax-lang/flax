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

		if(from->isIntegerType() && to->isIntegerType())
		{
			if(from->isConstantIntType())
				return 0;

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
	static int computeOverloadDistance(std::vector<Param> target, std::vector<Param> args, bool cvararg, Location* loc,
		std::string* estr)
	{
		iceAssert(estr);
		if(target.empty() && args.empty())
			return 0;

		bool anyvararg = cvararg || (target.size() > 0 && target.back().type->isVariadicArrayType());

		if(!anyvararg && target.size() != args.size())
		{
			*estr = strprintf("Mismatched number of arguments; expected %zu, got %zu", target.size(), args.size());
			return -1;
		}
		else if(anyvararg && args.size() < target.size())
		{
			*estr = strprintf("Too few arguments; need at least %zu even if variadic arguments are empty", target.size());
			return -1;
		}

		int distance = 0;
		for(size_t i = 0; i < target.size(); i++)
		{
			auto d = getCastDistance(args[i].type, target[i].type);
			if(d == -1)
			{
				*estr = strprintf("Mismatched argument type in argument %zu: no valid cast from given type '%s' to expected type '%s'",
					i, args[i].type->str(), target[i].type->str());
				*loc = target[i].loc;

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
			if(args.size() == target.size() && (args.back().type->isVariadicArrayType() || args.back().type->isDynamicArrayType()))
			{
				// yes we can
				auto a = args.back().type->toDynamicArrayType()->getElementType();
				auto t = target.back().type->toDynamicArrayType()->getElementType();

				if(a != t)
				{
					*estr = strprintf("Mismatched element type in variadic array passthrough; expected '%s', got '%s'",
						t->str(), a->str());
					return -1;
				}
				else
				{
					distance += 0;
				}
			}
			else
			{
				auto elmTy = target.back().type->toDynamicArrayType()->getElementType();
				for(size_t i = target.size(); i < args.size(); i++)
				{
					auto ty = args[i].type;
					auto dist = getCastDistance(ty, elmTy);
					if(dist == -1)
					{
						*estr = strprintf("Mismatched type in variadic argument; no valid cast from given type '%s' to expected type '%s' (ie. element type of variadic parameter list)", ty->str(), elmTy->str());
						return -1;
					}

					distance += dist;
				}
			}
		}

		return distance;
	}

	FunctionDecl* TypecheckState::resolveFunctionFromCandidates(std::vector<FunctionDecl*> cands, std::vector<Param> arguments,
		PrettyError* errs)
	{
		iceAssert(errs);

		using Param = FunctionDefn::Param;
		iceAssert(cands.size() >= 0);

		int bestDist = INT_MAX;
		std::vector<FunctionDecl*> finals;
		std::map<FunctionDecl*, std::pair<Location, std::string>> fails;

		for(auto fn : cands)
		{

			auto dist = computeOverloadDistance(fn->params, arguments, fn->isVarArg, &fails[fn].first, &fails[fn].second);
			if(dist == -1)
				continue;

			else if(dist < bestDist)
				finals.clear(), finals.push_back(fn), bestDist = dist;

			else if(dist == bestDist)
				finals.push_back(fn);
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

	FunctionDecl* TypecheckState::resolveFunction(std::string name, std::vector<Param> arguments, PrettyError* errs)
	{
		iceAssert(errs);

		auto fs = this->getFunctionDeclsWithName(name);
		if(fs.empty()) error(this->loc(), "No such function named '%s'", name.c_str());

		return this->resolveFunctionFromCandidates(fs, arguments, errs);
	}
}





sst::Stmt* ast::FunctionCall::typecheck(TCS* fs, fir::Type* inferred)
{
	using Param = sst::FunctionDecl::Param;

	fs->pushLoc(this);
	defer(fs->popLoc());

	auto call = new sst::FunctionCall(this->loc);
	for(auto p : this->args)
	{
		auto st = p->typecheck(fs);
		auto expr = dcast(sst::Expr, st);

		if(!expr)
			error(this->loc, "Statement cannot be used as an expression");

		call->arguments.push_back(expr);
	}


	// resolve the function call here
	sst::TypecheckState::PrettyError errs;

	std::vector<Param> ts;
	std::transform(call->arguments.begin(), call->arguments.end(), std::back_inserter(ts), [](sst::Expr* e) -> auto {
		return Param { .type = e->type, .loc = e->loc };
	});

	call->target = fs->resolveFunction(this->name, ts, &errs);
	if(!errs.errorStr.empty())
	{
		exitless_error(this, "%s", errs.errorStr);
		for(auto inf : errs.infoStrs)
			fprintf(stderr, "%s", inf.second.c_str());

		doTheExit();
	}

	iceAssert(call->target);
	call->type = call->target->returnType;
	call->name = this->name;
	iceAssert(call->type);

	return call;
}








