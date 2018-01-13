// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include <set>

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
		else if(from->isDynamicArrayType() && to->isArraySliceType() && from->getArrayElementType() == to->getArrayElementType())
		{
			return 2;
		}
		else if(from->isDynamicArrayType() && from->getArrayElementType()->isVoidType() && (to->isDynamicArrayType() || to->isArraySliceType() || to->isArrayType()))
		{
			return 0;
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

	//* we take Param for both because we want to be able to call without having an Expr*
	//* so we stick with the types.
	static int computeOverloadDistance(const Location& fnLoc, std::vector<FunctionDecl::Param> target,
		std::vector<FunctionDecl::Param> args, bool cvararg, Location* loc, std::string* estr, Defn* candidate)
	{
		iceAssert(estr);
		if(target.empty() && args.empty())
			return 0;

		bool anyvararg = cvararg || (target.size() > 0 && target.back().type->isVariadicArrayType());

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

		// find the first named argument -- get the index of the first named argument.
		auto idx = util::indexOf(args, [](Param p) -> bool { return p.name != ""; });

		std::vector<Param> positional;
		std::vector<Param> named;

		std::unordered_map<std::string, size_t> nameToIndex;
		for(size_t i = 0; i < target.size(); i++)
			nameToIndex[target[i].name] = i;

		if(idx == 0)
		{
			// all named
			named = args;
		}
		else if(idx == -1)
		{
			// all positional
			positional = args;
		}
		else
		{
			positional = std::vector<Param>(args.begin(), args.begin() + idx - 1);
			named = std::vector<Param>(args.begin() + idx, args.end());
		}


		//* note: sanity checking
		{
			// make sure there are no positional arguments in 'named'
			std::set<std::string> names;
			for(auto k : named)
			{
				if(k.name == "")
				{
					*estr = strprintf("Positional arguments cannot appear after named arguments in a function call");
					*loc = k.loc;

					return -1;
				}
				else if(candidate && dcast(sst::FunctionDefn, candidate) && dcast(sst::FunctionDefn, candidate)->parentTypeForMethod && k.name == "self")
				{
					*estr = strprintf("Self pointer cannot be specified in a method call");
					*loc = k.loc;

					return -1;
				}
				else if(names.find(k.name) != names.end())
				{
					*estr = strprintf("Duplicate named argument '%s'", k.name);
					*loc = k.loc;

					return -1;
				}
				else if(nameToIndex.find(k.name) == nameToIndex.end())
				{
					*estr = strprintf("Function does not have a parameter named '%s'", k.name);
					*loc = k.loc;

					return -1;
				}

				names.insert(k.name);
			}
		}


		int distance = 0;

		// handle the positional arguments
		for(size_t i = 0; i < std::min(target.size(), positional.size()); i++)
		{
			auto d = getCastDistance(args[i].type, target[i].type);
			if(d == -1)
			{
				*estr = strprintf("Mismatched argument type in argument %zu: no valid cast from given type '%s' to expected type '%s'",
					i, args[i].type, target[i].type);
				*loc = args[i].loc;

				return -1;
			}
			else
			{
				distance += d;
			}
		}

		// check the named ones.
		for(auto narg : named)
		{
			auto ind = nameToIndex[narg.name];
			if(!positional.empty() && ind <= positional.size())
			{
				*estr = strprintf("Duplicate argument '%s', was already specified as a positional argument", narg.name);
				*loc = narg.loc;

				return -1;
			}

			int d = getCastDistance(narg.type, target[ind].type);
			if(d == -1)
			{
				*estr = strprintf("Mismatched argument type in named argument '%s': no valid cast from given type '%s' to expected type '%s'",
					narg.name, narg.type, target[ind].type);
				*loc = narg.loc;

				return -1;
			}

			distance += d;
		}



		// means we're a flax-variadic function
		// thus we need to actually check the types.
		if(anyvararg && !cvararg)
		{
			// first, check if we can do a direct-passthrough
			if(args.size() == target.size() && (args.back().type->isVariadicArrayType() || args.back().type->isDynamicArrayType()))
			{
				// yes we can
				// TODO: if we take any[...], then passing an array should *not* cast to a single 'any' (duh)
				auto a = args.back().type->getArrayElementType();
				auto t = target.back().type->getArrayElementType();

				if(a != t)
				{
					*estr = strprintf("Mismatched element type in variadic array passthrough; expected '%s', got '%s'",
						t, a);
					*loc = target.back().loc;
					return -1;
				}
				else
				{
					distance += 0;
				}
			}
			else
			{
				auto elmTy = target.back().type->getArrayElementType();
				for(size_t i = target.size(); i < args.size(); i++)
				{
					auto ty = args[i].type;
					auto dist = getCastDistance(ty, elmTy);
					if(dist == -1)
					{
						*estr = strprintf("Mismatched type in variadic argument; no valid cast from given type '%s' to expected type '%s' (ie. element type of variadic parameter list)", ty, elmTy);
						*loc = args.back().loc;
						return -1;
					}

					distance += dist;
				}
			}
		}

		return distance;
	}



	int TypecheckState::getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b)
	{
		Location eloc;
		std::string estr;

		using Param = FunctionDefn::Param;

		return computeOverloadDistance(this->loc(), util::map(a, [](fir::Type* t) -> Param { return Param { "", Location(), t }; }),
			util::map(b, [](fir::Type* t) -> Param { return Param { "", Location(), t }; }), false, &eloc, &estr, 0);
	}

	int TypecheckState::getOverloadDistance(const std::vector<FunctionDecl::Param>& a, const std::vector<FunctionDecl::Param>& b)
	{
		return this->getOverloadDistance(util::map(a, [](Param p) { return p.type; }), util::map(b, [](Param p) { return p.type; }));
	}


	bool TypecheckState::isDuplicateOverload(const std::vector<FunctionDecl::Param>& a, const std::vector<FunctionDecl::Param>& b)
	{
		return this->getOverloadDistance(a, b) == 0;
	}

	bool TypecheckState::isDuplicateOverload(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b)
	{
		return this->getOverloadDistance(a, b) == 0;
	}





	Defn* TypecheckState::resolveFunctionFromCandidates(const std::vector<Defn*>& cands, const std::vector<Param>& arguments,
		PrettyError* errs, bool allowImplicitSelf)
	{
		if(cands.empty()) return 0;

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
				// make a copy, i guess.
				auto args = arguments;

				if(auto def = dcast(FunctionDefn, fn); def && def->parentTypeForMethod != 0 && allowImplicitSelf)
					args.insert(args.begin(), Param { "", Location(), def->parentTypeForMethod->getPointerTo() });

				dist = computeOverloadDistance(cand->loc, fn->params,
					args, fn->isVarArg, &fails[fn].first, &fails[fn].second, cand);
			}
			else if(auto vr = dcast(VarDefn, cand))
			{
				iceAssert(vr->type->isFunctionType());
				auto ft = vr->type->toFunctionType();

				// check if have any names
				for(auto p : arguments)
				{
					if(p.name != "")
					{
						errs->errorStr += strbold("Function values cannot be called with named arguments");
						errs->infoStrs.push_back({ vr->loc, strinfo(vr->loc, "'%s' was defined here:", vr->id.name) });

						return 0;
					}
				}

				auto prms = ft->getArgumentTypes();
				dist = computeOverloadDistance(cand->loc, util::map(prms, [](fir::Type* t) -> auto { return Param { "", Location(), t }; }),
					arguments, false, &fails[vr].first, &fails[vr].second, cand);
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
			std::vector<fir::Type*> tmp = util::map(arguments, [](Param p) -> auto { return p.type; });

			errs->errorStr += strbold("No overload of function '%s' matching given argument types '%s' amongst %zu candidate%s",
				cands[0]->id.name, fir::Type::typeListToString(tmp), fails.size(), fails.size() == 1 ? "" : "s");

			for(auto f : fails)
				errs->infoStrs.push_back({ f.first->loc, strinfo(f.first->loc, "Candidate not viable: %s", f.second.second) });

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

	Defn* TypecheckState::resolveFunction(const std::string& name, const std::vector<Param>& arguments, PrettyError* errs, bool travUp)
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
			auto defs = tree->getDefinitionsWithName(name);
			for(auto def : defs)
			{
				if(auto fn = dcast(FunctionDecl, def))
				{
					fns.push_back(fn);
				}
				else if((dcast(VarDefn, def) || dcast(ArgumentDefn, def)) && def->type->isFunctionType())
				{
					// ok, we'll check it later i guess.
					if(!didVar)
						fns.push_back(def);

					didVar = true;
				}
				else if(auto typedf = dcast(TypeDefn, def))
				{
					// ok, then.
					//* note: no need to specify 'travUp', because we already resolved the type here.
					return this->resolveConstructorCall(typedf, arguments, errs);
				}
				else
				{
					didVar = true;
					exitless_error(this->loc(), "'%s' cannot be called as a function; it was defined with type '%s' in the current scope",
						name, def->type);

					info(def, "Previously defined here:");

					doTheExit();
				}
			}

			if(travUp && fns.empty())
				tree = tree->parent;

			else
				break;
		}

		if(fns.empty())
			error(this->loc(), "No such function named '%s' (in scope '%s')", name, this->serialiseCurrentScope());

		return this->resolveFunctionFromCandidates(fns, arguments, errs, travUp);
	}



	TypeDefn* TypecheckState::resolveConstructorCall(TypeDefn* typedf, const std::vector<FunctionDecl::Param>& arguments, PrettyError* errs)
	{
		iceAssert(errs);

		if(auto str = dcast(StructDefn, typedf))
		{
			// ok, structs get named arguments, and no un-named arguments.
			// we just loop through each argument, ensure that (1) every arg has a name; (2) every name exists in the struct

			//* note that structs don't have inline member initialisers, so there's no trouble with this approach (in the codegeneration)
			//* of inserting missing arguments as just '0' or whatever their default value is

			//* in the case of classes, they will have inline initialisers, so the constructor calling must handle such things.
			//* but then class constructors are handled like regular functions, so it should be fine.

			std::set<std::string> fieldNames;
			for(auto f : str->fields)
				fieldNames.insert(f->id.name);

			bool useNames = false;
			bool firstName = true;

			std::set<std::string> seenNames;
			for(auto arg : arguments)
			{
				if(arg.name.empty() && useNames)
				{
					error(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor");
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
					firstName = false;
				}
				else if(!arg.name.empty() && !useNames && !firstName)
				{
					error(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor");
				}
				else if(useNames && fieldNames.find(arg.name) == fieldNames.end())
				{
					exitless_error(arg.loc, "Field '%s' does not exist in struct '%s'", arg.name, str->id.name);
					info(str, "Struct was defined here:");

					doTheExit();
				}
				else if(useNames && seenNames.find(arg.name) != seenNames.end())
				{
					error(arg.loc, "Duplicate argument for field '%s' in constructor call to struct '%s'", arg.name, str->id.name);
				}

				seenNames.insert(arg.name);
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames && arguments.size() != fieldNames.size() && arguments.size() > 0)
			{
				exitless_error(this->loc(),
					"Mismatched number of arguments in constructor call to type '%s'; expected %d arguments, found %d arguments instead",
					str->id.name, fieldNames.size(), arguments.size());

				info("All arguments are mandatory when using positional arguments");
				doTheExit();
			}

			// in actual fact we just return the thing here. sigh.
			return str;
		}
		else
		{
			exitless_error(this->loc(), "Unsupported constructor call on type '%s'", typedf->id.name);
			info(typedf, "Type was defined here:");

			doTheExit();
		}
	}
}




sst::Expr* ast::FunctionCall::typecheckWithArguments(TCS* fs, const std::vector<FnCallArgument>& arguments)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;


	// resolve the function call here
	sst::TypecheckState::PrettyError errs;
	std::vector<Param> ts = util::map(arguments, [](auto e) -> Param { return Param { e.name, e.loc, e.value->type }; });

	auto target = fs->resolveFunction(this->name, ts, &errs, this->traverseUpwards);
	if(!errs.errorStr.empty())
	{
		exitless_error(this, "%s", errs.errorStr);
		for(auto inf : errs.infoStrs)
			fprintf(stderr, "%s", inf.second.c_str());

		doTheExit();
	}

	iceAssert(target);

	if(auto typedf = dcast(sst::TypeDefn, target))
	{
		auto ret = new sst::ConstructorCall(this->loc, typedf->type);

		ret->target = typedf;
		ret->arguments = arguments;

		return ret;
	}
	else
	{
		iceAssert(target->type->isFunctionType());

		auto ret = new sst::FunctionCall(this->loc, target->type->toFunctionType()->getReturnType());
		ret->name = this->name;
		ret->target = target;
		ret->arguments = arguments;

		// check if it's a method call
		// if so, indicate. here, we set 'isImplicitMethodCall' to true, as an assumption.
		// in DotOp's typecheck, *after* calling this typecheck(), we set it back to false

		// so, if it was really an implicit call, it remains set
		// if it was a dot-op call, it gets set back to false by the dotop checking.

		if(auto fd = dcast(sst::FunctionDefn, target); fd && fd->parentTypeForMethod)
			ret->isImplicitMethodCall = true;

		return ret;
	}
}










sst::Expr* ast::ExprCall::typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& arguments)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;

	std::vector<Param> ts = util::map(arguments, [](auto e) -> Param { return Param { e.name, e.loc, e.value->type }; });

	auto target = this->callee->typecheck(fs);
	iceAssert(target);

	if(!target->type->isFunctionType())
		error(this->callee, "Expression with non-function-type '%s' cannot be called");

	Location eloc;
	std::string estr;

	auto ft = target->type->toFunctionType();
	int dist = sst::computeOverloadDistance(this->loc, util::map(ft->getArgumentTypes(), [](fir::Type* t) -> auto {
		return Param { "", Location(), t }; }), ts, false, &eloc, &estr, 0);

	if(!estr.empty() || dist == -1)
		error(eloc, "%s", estr);

	auto ret = new sst::ExprCall(this->loc, target->type->toFunctionType()->getReturnType());
	ret->callee = target;
	ret->arguments = util::map(arguments, [](auto e) -> sst::Expr* { return e.value; });

	return ret;
}

static std::vector<FnCallArgument> _typecheckArguments(sst::TypecheckState* fs, const std::vector<std::pair<std::string, ast::Expr*>>& args)
{
	std::vector<FnCallArgument> ret;

	for(auto arg : args)
	{
		if(auto splat = dcast(ast::SplatOp, arg.second))
		{
			if(!arg.first.empty())
				error(arg.second->loc, "Splatted tuples cannot be passed as a named argument");

			// get the type of the thing inside.
			//* note: theoretically, this should be the only place in the compiler where we deal with splats explitily
			//* and it should remain this way.

			// TODO: handle splatting of arrays for varargs calls.

			auto tuple = splat->expr->typecheck(fs);
			if(!tuple->type->isTupleType())
				error(arg.second->loc, "Splatting in a function is currently only supported for tuples, have type '%s'", tuple->type);

			auto tty = tuple->type->toTupleType();
			for(size_t i = 0; i < tty->getElementCount(); i++)
			{
				auto tdo = new sst::TupleDotOp(arg.second->loc, tty->getElementN(i));
				tdo->index = i;
				tdo->lhs = tuple;

				auto fca = FnCallArgument(arg.second->loc, arg.first, tdo);
				ret.push_back(fca);
			}
		}
		else
		{
			ret.push_back(FnCallArgument(arg.second->loc, arg.first, arg.second->typecheck(fs)));
		}
	}

	return ret;
}


sst::Expr* ast::ExprCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return this->typecheckWithArguments(fs, _typecheckArguments(fs, this->args));
}

sst::Expr* ast::FunctionCall::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return this->typecheckWithArguments(fs, _typecheckArguments(fs, this->args));
}








