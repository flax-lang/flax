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

namespace sst
{
	int TypecheckState::getCastDistance(fir::Type* from, fir::Type* to)
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
			return 5;
		}
		else if(from->isStringType() && to->isCharSliceType())
		{
			return 3;
		}
		else if(from->isCharSliceType() && to == fir::Type::getInt8Ptr())
		{
			return 3;
		}
		else if(from->isMutablePointer() && to->isImmutablePointer() && from->getPointerElementType() == to->getPointerElementType())
		{
			// cast from a mutable pointer type to an immutable one can be implicit.
			return 1;
		}
		else if(from->isArraySliceType() && to->isArraySliceType() && (from->getArrayElementType() == to->getArrayElementType())
			&& from->toArraySliceType()->isMutable() && !to->toArraySliceType()->isMutable())
		{
			// same with slices -- cast from mutable slice to immut slice can be implicit.
			return 1;
		}

		//* note: we don't need to check that 'to' is a class type, because if it's not then the parent check will fail anyway.
		else if(from->isPointerType() && to->isPointerType() && from->getPointerElementType()->isClassType()
			&& from->getPointerElementType()->toClassType()->isInParentHierarchy(to->getPointerElementType()))
		{
			// cast from a derived class pointer to a base class pointer
			return 2;
		}
		else if(from->isNullType() && to->isPointerType())
		{
			return 1;
		}
		else if(from->isTupleType() && to->isTupleType() && from->toTupleType()->getElementCount() == to->toTupleType()->getElementCount())
		{
			int sum = 0;

			auto ftt = from->toTupleType();
			auto ttt = to->toTupleType();

			for(size_t i = 0; i < ttt->getElementCount(); i++)
			{
				if(int k = this->getCastDistance(ftt->getElementN(i), ttt->getElementN(i)); k < 0)
					return -1;

				else
					sum += k;
			}

			return sum;
		}

		return -1;
	}

	using Param = FunctionDecl::Param;

	//* we take Param for both because we want to be able to call without having an Expr*
	//* so we stick with the types.
	static int computeOverloadDistance(TypecheckState* fs, const Location& fnLoc, std::vector<FunctionDecl::Param> target,
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
			auto d = fs->getCastDistance(args[i].type, target[i].type);
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

			int d = fs->getCastDistance(narg.type, target[ind].type);
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
					auto dist = fs->getCastDistance(ty, elmTy);
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

		return computeOverloadDistance(this, this->loc(), util::map(a, [](fir::Type* t) -> Param { return Param { "", Location(), t }; }),
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





	TCResult TypecheckState::resolveFunctionFromCandidates(const std::vector<Defn*>& cands, const std::vector<Param>& arguments,
		const TypeParamMap_t& gmaps, bool allowImplicitSelf)
	{
		if(cands.empty()) return TCResult(PrettyError::error(Location(), "No candidates"));

		using Param = FunctionDefn::Param;
		iceAssert(cands.size() > 0);

		PrettyError errors;

		int bestDist = INT_MAX;
		std::vector<Defn*> finals;
		std::map<Defn*, std::pair<Location, std::string>> fails;

		for(auto cand : cands)
		{
			int dist = -1;

			if(auto fn = dcast(FunctionDecl, cand))
			{
				// make a copy, i guess.
				auto args = arguments;

				if(auto def = dcast(FunctionDefn, fn); def && def->parentTypeForMethod != 0 && allowImplicitSelf)
					args.insert(args.begin(), Param { "", Location(), def->parentTypeForMethod->getPointerTo() });

				dist = computeOverloadDistance(this, cand->loc, fn->params,
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
						errors.addError(p.loc, "Function values cannot be called with named arguments");
						errors.addInfo(vr, "'%s' was defined here:", vr->id.name);

						return TCResult(errors);
					}
				}

				auto prms = ft->getArgumentTypes();
				dist = computeOverloadDistance(this, cand->loc, util::map(prms, [](fir::Type* t) -> auto { return Param { "", Location(), t }; }),
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

			errors.addError(this->loc(), "No overload of function '%s' matching given argument types '%s' amongst %zu candidate%s",
				cands[0]->id.name, fir::Type::typeListToString(tmp), fails.size(), fails.size() == 1 ? "" : "s");

			for(auto f : fails)
				errors.addInfo(f.first, "Candidate not viable: %s", f.second.second);

			return TCResult(errors);
		}
		else if(finals.size() > 1)
		{
			// check if all of the targets we found are virtual, and that they belong to the same class.

			bool virt = true;
			fir::ClassType* self = 0;

			Defn* ret = finals[0];
			for(auto def : finals)
			{
				if(auto fd = dcast(sst::FunctionDefn, def); fd && fd->isVirtual)
				{
					iceAssert(fd->parentTypeForMethod);
					iceAssert(fd->parentTypeForMethod->isClassType());

					if(!self)
					{
						self = fd->parentTypeForMethod->toClassType();
					}
					else
					{
						// check if they're co/contra variant
						auto ty = fd->parentTypeForMethod->toClassType();

						//* here we're just checking that 'ty' and 'self' are part of the same class hierarchy -- we don't really care about the method
						//* that we resolve being at the lowest or highest level of that hierarchy.

						if(!ty->isInParentHierarchy(self) && !self->isInParentHierarchy(ty))
						{
							virt = false;
							break;
						}
					}
				}
				else
				{
					virt = false;
					break;
				}
			}

			if(virt)
			{
				return TCResult(ret);
			}
			else
			{
				errors.addError(this->loc(), "Ambiguous call to function '%s', have %zu candidates:", cands[0]->id.name, finals.size());

				for(auto f : finals)
					errors.addInfo(f, "Possible target:");

				return TCResult(errors);
			}
		}
		else
		{
			return TCResult(finals[0]);
		}
	}




	TCResult TypecheckState::resolveFunction(const std::string& name, const std::vector<Param>& arguments, const TypeParamMap_t& gmaps, bool travUp)
	{
		// we kinda need to check manually, since... we need to give a good error message
		// when a shadowed thing is not a function
		PrettyError errors;
		std::vector<Defn*> fns;
		StateTree* tree = this->stree;

		//* the purpose of this 'didVar' flag (because I was fucking confused reading this)
		//* is so we only consider the innermost (ie. most local) variable, because variables don't participate in overloading.

		//? I can't find any information about this behaviour in languages other than C++, because we need to have a certain set of
		//? features for it to manifest -- 1. user-defined, explicit namespaces; 2. function overloading.

		//* how it works in C++, and for now also in Flax, is that once we match *any* names in the current scope, we stop searching upwards
		//* -- even if it means we will throw an error because of mismatched arguments or whatever.
		bool didVar = false;
		bool didGeneric = false;
		while(tree)
		{
			// unify the handling of generic and non-generic stuff.

			// if we provided mappings, don't bother searching normal functions.
			if(gmaps.empty())
			{
				auto defs = tree->getDefinitionsWithName(name);
				fns.insert(fns.begin(), defs.begin(), defs.end());
			}

			if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(name); gdefs.size() > 0)
			{
				didGeneric = true;
				auto res = this->attemptToDisambiguateGenericReference(name, gdefs, gmaps, (fir::Type*) 0);

				if(res.isDefn())
					fns.push_back(res.defn());

				else
					errors.incorporate(res.error());
			}


			if(travUp && fns.empty())
				tree = tree->parent;

			else
				break;
		}

		if(fns.empty())
		{
			if(!didGeneric)
				errors.addErrorBefore(this->loc(), "No such function named '%s'", name);

			return TCResult(errors);
		}

		std::vector<sst::Defn*> cands;
		for(auto def : fns)
		{
			if(auto fn = dcast(FunctionDecl, def))
			{
				cands.push_back(fn);
			}
			else if((dcast(VarDefn, def) || dcast(ArgumentDefn, def)) && def->type->isFunctionType())
			{
				// ok, we'll check it later i guess.
				if(!didVar)
					cands.push_back(def);

				didVar = true;
			}
			else if(auto typedf = dcast(TypeDefn, def))
			{
				// ok, then.
				//* note: no need to specify 'travUp', because we already resolved the type here.
				return this->resolveConstructorCall(typedf, arguments, gmaps);
			}
			else
			{
				didVar = true;
				errors.addError(this->loc(), "'%s' cannot be called as a function; it was defined with type '%s' in the current scope",
					name, def->type);

				errors.addInfo(def, "Previously defined here:");
				return TCResult(errors);
			}
		}


		return this->resolveFunctionFromCandidates(cands, arguments, gmaps, travUp);
	}



	TCResult TypecheckState::resolveConstructorCall(TypeDefn* typedf, const std::vector<FunctionDecl::Param>& arguments, const TypeParamMap_t& gmaps)
	{
		//! ACHTUNG: DO NOT REARRANGE !
		//* NOTE: ClassDefn inherits from StructDefn *

		if(auto cls = dcast(ClassDefn, typedf))
		{
			// class initialisers must be called with named arguments only.
			for(const auto& arg : arguments)
			{
				if(arg.name.empty())
				{
					return TCResult(PrettyError::error(arg.loc, "Arguments to class initialisers (for class '%s' here) must be named", cls->id.name));
				}
			}


			auto cand = this->resolveFunctionFromCandidates(util::map(cls->initialisers, [](auto e) -> auto {
				return dcast(sst::Defn, e);
			}), arguments, gmaps, true);

			if(cand.isError())
			{
				cand.error().addErrorBefore(this->loc(), "Failed to find matching initialiser for class '%s':", cls->id.name);
				return cand;
			}

			return TCResult(cand);
		}
		else if(auto str = dcast(StructDefn, typedf))
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
					return TCResult(PrettyError::error(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor"));
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
					firstName = false;
				}
				else if(!arg.name.empty() && !useNames && !firstName)
				{
					return TCResult(PrettyError::error(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor"));
				}
				else if(useNames && fieldNames.find(arg.name) == fieldNames.end())
				{
					return TCResult(PrettyError::error(arg.loc, "Field '%s' does not exist in struct '%s'", arg.name, str->id.name));
				}
				else if(useNames && seenNames.find(arg.name) != seenNames.end())
				{
					return TCResult(PrettyError::error(arg.loc, "Duplicate argument for field '%s' in constructor call to struct '%s'",
						arg.name, str->id.name));
				}

				seenNames.insert(arg.name);
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames && arguments.size() != fieldNames.size() && arguments.size() > 0)
			{
				PrettyError errors;
				errors.addError(this->loc(),
					"Mismatched number of arguments in constructor call to type '%s'; expected %d arguments, found %d arguments instead",
					str->id.name, fieldNames.size(), arguments.size());

				errors.addInfo(Location(), "All arguments are mandatory when using positional arguments");
				return TCResult(errors);
			}

			// in actual fact we just return the thing here. sigh.
			return TCResult(str);
		}
		else
		{
			PrettyError errors;
			errors.addError(this->loc(), "Unsupported constructor call on type '%s'", typedf->id.name);
			errors.addInfo(typedf, "Type was defined here:");

			return TCResult(errors);
		}
	}
}












sst::Expr* ast::FunctionCall::typecheckWithArguments(TCS* fs, const std::vector<FnCallArgument>& arguments)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;


	// resolve the function call here
	std::vector<Param> ts = util::map(arguments, [](auto e) -> Param { return Param { e.name, e.loc, e.value->type }; });

	auto gmaps = fs->convertParserTypeArgsToFIR(this->mappings);
	auto res = fs->resolveFunction(this->name, ts, gmaps, this->traverseUpwards);

	auto target = res.defn();
	iceAssert(target);

	if(auto strdf = dcast(sst::StructDefn, target))
	{
		auto ret = new sst::StructConstructorCall(this->loc, strdf->type);

		ret->target = strdf;
		ret->arguments = arguments;

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
			auto ret = new sst::ClassConstructorCall(this->loc, fnd->parentTypeForMethod);

			ret->target = fnd;
			ret->arguments = arguments;
			ret->classty = dcast(sst::ClassDefn, fs->typeDefnMap[fnd->parentTypeForMethod]);

			iceAssert(ret->target);

			return ret;
		}



		auto call = new sst::FunctionCall(this->loc, target->type->toFunctionType()->getReturnType());
		call->name = this->name;
		call->target = target;
		call->arguments = arguments;

		if(auto fd = dcast(sst::FunctionDefn, target); fd && fd->parentTypeForMethod)
		{
			// check if it's a method call
			// if so, indicate. here, we set 'isImplicitMethodCall' to true, as an assumption.
			// in DotOp's typecheck, *after* calling this typecheck(), we set it back to false

			// so, if it was really an implicit call, it remains set
			// if it was a dot-op call, it gets set back to false by the dotop checking.

			call->isImplicitMethodCall = true;
		}

		return call;
	}
}










sst::Expr* ast::ExprCall::typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& arguments)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;

	std::vector<Param> ts = util::map(arguments, [](auto e) -> Param { return Param { e.name, e.loc, e.value->type }; });

	auto target = this->callee->typecheck(fs).expr();
	iceAssert(target);

	if(!target->type->isFunctionType())
		error(this->callee, "Expression with non-function-type '%s' cannot be called");

	Location eloc;
	std::string estr;

	auto ft = target->type->toFunctionType();
	int dist = sst::computeOverloadDistance(fs, this->loc, util::map(ft->getArgumentTypes(), [](fir::Type* t) -> auto {
		return Param { "", Location(), t }; }), ts, false, &eloc, &estr, 0);

	if(!estr.empty() || dist == -1)
		error(eloc, "%s", estr);

	auto ret = new sst::ExprCall(this->loc, target->type->toFunctionType()->getReturnType());
	ret->callee = target;
	ret->arguments = util::map(arguments, [](auto e) -> sst::Expr* { return e.value; });

	return ret;
}

std::vector<FnCallArgument> sst::TypecheckState::typecheckCallArguments(const std::vector<std::pair<std::string, ast::Expr*>>& args)
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

			auto tuple = splat->expr->typecheck(this).expr();
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
			ret.push_back(FnCallArgument(arg.second->loc, arg.first, arg.second->typecheck(this).expr()));
		}
	}

	return ret;
}


TCResult ast::ExprCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return TCResult(this->typecheckWithArguments(fs, fs->typecheckCallArguments(this->args)));
}

TCResult ast::FunctionCall::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return TCResult(this->typecheckWithArguments(fs, fs->typecheckCallArguments(this->args)));
}








