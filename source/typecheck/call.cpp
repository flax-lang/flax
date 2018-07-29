// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include <set>

namespace sst
{
	int TypecheckState::getCastDistance(fir::Type* from, fir::Type* to)
	{
		if(from == to) return 0;

		if(from->isConstantNumberType() && to->isPrimitiveType())
		{
			auto cty = from->toConstantNumberType();
			if(!cty->isFloating() && to->isIntegerType())
				return 0;

			else if(!cty->isFloating() && to->isFloatingPointType())
				return 1;

			else if(cty->isFloating())      // not isint means isfloat, so if we're doing float -> float the cost is 0
				return 0;

			else                            // if we reach here, we're trying to do float -> int, which is a no-go.
				return -1;
		}
		else if(from->isIntegerType() && to->isIntegerType())
		{
			if(from->isSignedIntType() == to->isSignedIntType())
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
			else
			{
				// only allow casting unsigned things to signed things... maybe??
				// TODO: investigate whether we want such loose casting.

				//? for now, no.
				return -1;
			}
		}
		else if(from->isDynamicArrayType() && to->isArraySliceType() && from->getArrayElementType() == to->getArrayElementType())
		{
			return 2;
		}
		else if(from->isDynamicArrayType() && from->getArrayElementType()->isVoidType() && (to->isDynamicArrayType() || to->isArraySliceType() || to->isArrayType()))
		{
			return 2;
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
		else if(from->isVariadicArrayType() && to->isArraySliceType() && from->getArrayElementType() == to->getArrayElementType())
		{
			// allow implicit casting from variadic slices to their normal counterparts.
			return 4;
		}
		else if(from->isArraySliceType() && to->isArraySliceType() && (from->getArrayElementType() == to->getArrayElementType())
			&& from->toArraySliceType()->isMutable() && !to->toArraySliceType()->isMutable() && !from->isVariadicArrayType() && !to->isVariadicArrayType())
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
		else if(to->isAnyType())
		{
			// lol. completely arbitrary.
			return 15;
		}

		return -1;
	}

	using Param = FunctionDecl::Param;

	//* we take Param for both because we want to be able to call without having an Expr*
	//* so we stick with the types.
	static std::pair<int, SpanError> computeOverloadDistance(TypecheckState* fs, Locatable* fnLoc, const std::vector<FunctionDecl::Param>& target,
		const std::vector<FunctionDecl::Param>& args, bool cvararg, Defn* candidate)
	{
		using Span = SpanError::Span;

		if(target.empty() && args.empty())
			return { 0, SpanError() };

		bool fvararg = (target.size() > 0 && target.back().type->isVariadicArrayType());
		bool anyvararg = cvararg || fvararg;

		if((!anyvararg && target.size() != args.size()) || (anyvararg && args.size() < (fvararg ? target.size() - 1 : target.size())))
		{
			return { -1, SpanError().add(Span(fnLoc->loc, strprintf("Mismatched number of arguments; expected %d, got %d instead",
				target.size(), args.size())))
			};
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
		else if(idx == (size_t) -1)
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
					// errs->add(fnLoc, { k.loc, strprintf("Positional arguments cannot appear after named arguments in a function call") });
					error(k.loc, "how did this happen?? Positional arguments cannot appear after named arguments in a function call");
					return { -1, SpanError() };
				}
				else if(candidate && dcast(sst::FunctionDefn, candidate) && dcast(sst::FunctionDefn, candidate)->parentTypeForMethod && k.name == "self")
				{
					// errs->add(fnLoc, { k.loc, strprintf("Self pointer cannot be specified in a method call") });
					error(k.loc, "how did this happen?? Self pointer cannot be specified in a method call");
					return { -1, SpanError() };
				}
				else if(names.find(k.name) != names.end())
				{
					// errs->add(fnLoc, { k.loc, strprintf("Duplicate named argument '%s'", k.name) });
					error(k.loc, "how did this happen?? Duplicate named argument '%s'", k.name);
					return { -1, SpanError() };
				}
				else if(nameToIndex.find(k.name) == nameToIndex.end())
				{
					// errs->add(fnLoc, { k.loc, strprintf("Function does not have a parameter named '%s'", k.name) });
					error(k.loc, "how did this happen?? Function does not have a parameter named '%s'", k.name);
					return { -1, SpanError() };
				}

				names.insert(k.name);
			}
		}


		int distance = 0;

		// handle the positional arguments
		SpanError spanerr;

		for(size_t i = 0; i < std::min((fvararg ? target.size() - 1 : target.size()), positional.size()); i++)
		{
			auto d = fs->getCastDistance(args[i].type, target[i].type);
			if(d == -1)
			{
				spanerr.add(Span(target[i].loc, strprintf("Mismatched argument type in argument %zu: no valid cast from given type '%s' to expected type '%s'", i, args[i].type, target[i].type)));
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
				spanerr.add(Span(target[ind].loc, strprintf("Duplicate argument '%s', was already specified as a positional argument", narg.name)));
			}

			int d = fs->getCastDistance(narg.type, target[ind].type);
			if(d == -1)
			{
				spanerr.add(Span(target[ind].loc, strprintf("Mismatched argument type in named argument '%s': no valid cast from given type '%s' to expected type '%s'", narg.name, narg.type, target[ind].type)));
			}

			distance += d;
		}



		// means we're a flax-variadic function
		// thus we need to actually check the types.
		if(fvararg)
		{
			auto elmTy = target.back().type->getArrayElementType();

			// check if we only have 1 last arg
			if(target.size() == args.size())
			{
				auto lasty = args.back().type;
				if(lasty->isArraySliceType() && args.back().wasSplat)
				{
					if(lasty->getArrayElementType() != elmTy)
					{
						spanerr.add(Span(target.back().loc, strprintf("Mismatched type in parameter pack forwarding: expected element type of '%s', but found '%s' instead", elmTy, lasty->getArrayElementType())));
					}
					else
					{
						distance += 3;
					}
				}
				else
				{
					//! EW GOTO
					goto do_normal;
				}
			}
			else
			{
				do_normal:
				for(size_t i = target.size() - 1; i < args.size(); i++)
				{
					auto ty = args[i].type;
					auto dist = fs->getCastDistance(ty, elmTy);
					if(dist == -1)
					{
						spanerr.add(Span(target.back().loc, strprintf("Mismatched type in variadic argument: no valid cast from given type '%s' to expected type '%s' (ie. element type of variadic parameter list)", ty, elmTy)));
					}

					distance += dist;
				}
			}
		}

		// info(fs->loc(), "for this boi:");
		// warn(candidate, "distance %d", distance);
		if(spanerr.hasErrors()) return { -1, spanerr };
		else                    return { distance, SpanError() };
	}



	int TypecheckState::getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b)
	{
		OverloadError errs;
		using Param = FunctionDefn::Param;

		auto l = Locatable(this->loc(), "");
		return computeOverloadDistance(this, &l, util::map(a, [](fir::Type* t) -> Param { return Param(t); }),
			util::map(b, [](fir::Type* t) -> Param { return Param(t); }), false, 0).first;
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





	static TCResult _resolveFunctionCallFromCandidates(TypecheckState* fs, const std::vector<std::pair<Defn*, std::vector<Param>>>& cands,
		const TypeParamMap_t& gmaps, bool allowImplicitSelf)
	{
		if(cands.empty()) return TCResult(BareError("No candidates"));

		using Param = FunctionDefn::Param;
		iceAssert(cands.size() > 0);

		// OverloadError errors;

		int bestDist = INT_MAX;
		std::map<Defn*, SpanError> fails;
		std::vector<std::pair<Defn*, int>> finals;

		for(const auto& [ cand, arguments ] : cands)
		{
			int dist = -1;

			if(auto fn = dcast(FunctionDecl, cand))
			{
				// make a copy, i guess.
				auto args = arguments;

				if(auto def = dcast(FunctionDefn, fn); def && def->parentTypeForMethod != 0 && allowImplicitSelf)
					args.insert(args.begin(), Param(def->parentTypeForMethod->getPointerTo()));

				std::tie(dist, fails[fn]) = computeOverloadDistance(fs, cand, fn->params, args, fn->isVarArg, cand);
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
						return TCResult(SimpleError(p.loc, "Function values cannot be called with named arguments")
							.append(SimpleError(vr->loc, strprintf("'%s' was defined here:", vr->id.name)))
						);
					}
				}

				auto prms = ft->getArgumentTypes();
				std::tie(dist, fails[vr]) = computeOverloadDistance(fs, cand, util::map(prms, [](fir::Type* t) -> auto { return Param(t); }),
					arguments, false, cand);
			}

			if(dist == -1)
				continue;

			else if(dist < bestDist)
				finals.clear(), finals.push_back({ cand, dist }), bestDist = dist;

			else if(dist == bestDist)
				finals.push_back({ cand, dist });
		}

		if(finals.empty())
		{
			// TODO: make this error message better, because right now we assume the arguments are all the same.

			iceAssert(cands.size() == fails.size());
			std::vector<fir::Type*> tmp = util::map(cands[0].second, [](Param p) -> auto { return p.type; });

			auto errs = OverloadError(SimpleError(fs->loc(), strprintf("No overload in call to '%s(%s)' amongst %zu %s",
				cands[0].first->id.name, fir::Type::typeListToString(tmp), fails.size(), util::plural("candidate", fails.size()))));

			for(auto f : fails)
				errs.addCand(f.first, f.second);

			return TCResult(errs);
		}
		else if(finals.size() > 1)
		{
			// check if all of the targets we found are virtual, and that they belong to the same class.

			bool virt = true;
			fir::ClassType* self = 0;

			Defn* ret = finals[0].first;
			for(auto def : finals)
			{
				if(auto fd = dcast(sst::FunctionDefn, def.first); fd && fd->isVirtual)
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
				auto err = SimpleError(fs->loc(), strprintf("Ambiguous call to function '%s', have %zu candidates:",
					cands[0].first->id.name, finals.size()));

				for(auto f : finals)
					err.append(SimpleError(f.first->loc, strprintf("Possible target (overload distance %d):", f.second), MsgType::Note));

				return TCResult(err);
			}
		}
		else
		{
			return TCResult(finals[0].first);
		}
	}

	TCResult TypecheckState::resolveFunctionCallFromCandidates(const std::vector<Defn*>& cands, const std::vector<Param>& args,
		const TypeParamMap_t& gmaps, bool allowImplicitSelf)
	{
		auto cds = util::map(cands, [&args](auto c) -> std::pair<Defn*, std::vector<Param>> { return { c, args }; });
		return _resolveFunctionCallFromCandidates(this, cds, gmaps, allowImplicitSelf);
	}





	TCResult TypecheckState::resolveFunctionCall(const std::string& name, std::vector<FnCallArgument>& arguments, const TypeParamMap_t& gmaps, bool travUp,
		fir::Type* inferredRetType)
	{
		StateTree* tree = this->stree;

		//* the purpose of this 'didVar' flag (because I was fucking confused reading this)
		//* is so we only consider the innermost (ie. most local) variable, because variables don't participate in overloading.

		//? I can't find any information about this behaviour in languages other than C++, because we need to have a certain set of
		//? features for it to manifest -- 1. user-defined, explicit namespaces; 2. function overloading.

		//* how it works in C++, and for now also in Flax, is that once we match *any* names in the current scope, we stop searching upwards
		//* -- even if it means we will throw an error because of mismatched arguments or whatever.
		bool didVar = false;
		bool didGeneric = false;

		SimpleError errs;
		std::vector<std::pair<Defn*, std::vector<FnCallArgument>>> fns;
		while(tree)
		{
			// unify the handling of generic and non-generic stuff.
			// if we provided mappings, don't bother searching normal functions.
			if(gmaps.empty())
			{
				auto defs = tree->getDefinitionsWithName(name);
				for(auto d : defs)
					fns.push_back({ d, arguments });
			}

			if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(name); gdefs.size() > 0)
			{
				didGeneric = true;
				auto argcopy = arguments;
				auto res = this->attemptToDisambiguateGenericReference(name, gdefs, gmaps, inferredRetType, true, argcopy);

				if(!res.isDefn())
				{
					iceAssert(res.isError());
					errs.append(res.error());
				}
				else
				{
					auto def = res.defn();
					fns.push_back({ def, argcopy });
				}
			}

			if(travUp && fns.empty())
				tree = tree->parent;

			else
				break;
		}

		if(fns.empty())
		{
			if(!didGeneric)
			{
				errs.set(this->loc(), strprintf("No such function named '%s'", name));
			}

			return TCResult(errs);
		}


		std::vector<std::pair<sst::Defn*, std::vector<Param>>> cands;
		for(const auto& [ def, args ] : fns)
		{
			auto ts = util::map(args, [](auto p) -> auto { return Param(p); });
			if(auto fn = dcast(FunctionDecl, def))
			{
				cands.push_back({ fn, ts });
			}
			else if(dcast(VarDefn, def) && def->type->isFunctionType())
			{
				// ok, we'll check it later i guess.
				if(!didVar)
					cands.push_back({ def, ts });

				didVar = true;
			}
			else if(auto typedf = dcast(TypeDefn, def))
			{
				// ok, then.
				//* note: no need to specify 'travUp', because we already resolved the type here.
				return this->resolveConstructorCall(typedf, ts, gmaps);
			}
			else
			{
				return TCResult(
					SimpleError(this->loc(), strprintf("'%s' cannot be called as a function; it was defined with type '%s' in the current scope",
						name, def->type)).append(SimpleError(def->loc, "Previously defined here:"))
				);
			}
		}

		auto res = _resolveFunctionCallFromCandidates(this, cands, gmaps, travUp);
		if(res.isDefn())
		{
			auto ret = res.defn();
			auto it = std::find_if(fns.begin(), fns.end(), [&ret](const auto& p) -> bool { return p.first == ret; });
			iceAssert(it != fns.end());

			arguments = it->second;
			return res;
		}
		else
		{
			return TCResult(errs.append(res.error()));
		}
	}



	TCResult TypecheckState::resolveConstructorCall(TypeDefn* typedf, const std::vector<Param>& arguments,
		const TypeParamMap_t& gmaps)
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
					return TCResult(SimpleError::make(arg.loc, "Arguments to class initialisers (for class '%s' here) must be named", cls->id.name));
				}
			}


			auto cand = this->resolveFunctionCallFromCandidates(util::map(cls->initialisers, [](auto e) -> auto {
				return dcast(sst::Defn, e);
			}), arguments, gmaps, true);

			if(cand.isError())
			{
				auto err = dynamic_cast<OverloadError&>(cand.error());
				err.set(SimpleError(this->loc(), strprintf("Failed to find matching initialiser for class '%s':", cls->id.name)));

				return TCResult(err);
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
					return TCResult(SimpleError::make(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor"));
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
					firstName = false;
				}
				else if(!arg.name.empty() && !useNames && !firstName)
				{
					return TCResult(SimpleError::make(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor"));
				}
				else if(useNames && fieldNames.find(arg.name) == fieldNames.end())
				{
					return TCResult(SimpleError::make(arg.loc, "Field '%s' does not exist in struct '%s'", arg.name, str->id.name));
				}
				else if(useNames && seenNames.find(arg.name) != seenNames.end())
				{
					return TCResult(SimpleError::make(arg.loc, "Duplicate argument for field '%s' in constructor call to struct '%s'",
						arg.name, str->id.name));
				}

				seenNames.insert(arg.name);
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames && arguments.size() != fieldNames.size() && arguments.size() > 0)
			{
				return TCResult(
					SimpleError::make(this->loc(), "Mismatched number of arguments in constructor call to type '%s'; expected %d arguments, found %d arguments instead", str->id.name, fieldNames.size(), arguments.size())
						.append(BareError("All arguments are mandatory when using positional arguments", MsgType::Note))
				);
			}

			// in actual fact we just return the thing here. sigh.
			return TCResult(str);
		}
		else if(auto uvd = dcast(sst::UnionVariantDefn, typedf))
		{
			auto name = uvd->id.name;

			auto unn = uvd->parentUnion;
			iceAssert(unn);

			auto unt = unn->type->toUnionType();

			iceAssert(unn->variants.find(name) != unn->variants.end());
			auto uvl = unn->variants[name];

			// ok, then. check the type + arguments.
			std::vector<Param> target;
			if(unt->getVariant(name)->getInteriorType()->isTupleType())
			{
				for(auto t : unt->getVariant(name)->getInteriorType()->toTupleType()->getElements())
					target.push_back(Param("", uvl, t));
			}
			else if(!unt->getVariant(name)->getInteriorType()->isVoidType())
			{
				target.push_back(Param("", uvl, unt->getVariant(name)->getInteriorType()));
			}

			auto [ dist, errs ] = computeOverloadDistance(this, unn, target, arguments, false, 0);
			if(errs.hasErrors() || dist == -1)
			{
				errs.set(SimpleError::make(this->loc(), "Mismatched types in construction of variant '%s' of union '%s'", name,
					unn->id.name)).postAndQuit();
			}

			return TCResult(uvd);
		}
		else
		{
			return TCResult(
				SimpleError::make(this->loc(), "Unsupported constructor call on type '%s'", typedf->id.name)
			    .append(SimpleError(typedf->loc, "Type was defined here:"))
			);
		}
	}

	fir::Type* TypecheckState::checkIsBuiltinConstructorCall(const std::string& name, const std::vector<FnCallArgument>& arguments)
	{
		if(auto type = fir::Type::fromBuiltin(name))
		{
			for(const auto& a : arguments)
			{
				if(!a.name.empty())
					error(a.loc, "Builtin type initialisers do not accept named arguments");
			}

			// all builtin types can be zero-initialised.
			if(arguments.empty())
			{
				return type;
			}
			else if(arguments.size() == 1)
			{
				if(int d = getCastDistance(arguments[0].value->type, type); d >= 0 || (type->isStringType() && arguments[0].value->type->isCharSliceType()))
				{
					return type;
				}
				else
				{
					error(arguments[0].loc, "Type mismatch in initialiser call to builtin type '%s', found type '%s' instead", type,
						arguments[0].value->type);
				}
			}
			else
			{
				if(type->isStringType())
				{
					// either from a slice, or from a ptr + len
					if(arguments.size() == 1)
					{
						if(!arguments[0].value->type->isCharSliceType())
						{
							error(arguments[0].loc, "Single argument to string initialiser must be a slice of char, aka '%s', found '%s' instead",
								fir::Type::getCharSlice(false), arguments[0].value->type);
						}

						return type;
					}
					else if(arguments.size() == 2)
					{
						if(auto t1 = arguments[0].value->type; (t1 != fir::Type::getInt8Ptr() && t1 != fir::Type::getMutInt8Ptr()))
						{
							error(arguments[0].loc, "First argument to two-arg string initialiser (data pointer) must be '%s' or '%s', found '%s' instead",
								fir::Type::getInt8Ptr(), fir::Type::getMutInt8Ptr(), t1);
						}
						else if(auto t2 = arguments[1].value->type; this->getCastDistance(t2, fir::Type::getInt64()) < 0)
						{
							error(arguments[0].loc, "Second argument to two-arg string initialiser (length) must be '%s', found '%s' instead",
								(fir::Type*) fir::Type::getInt64(), t2);
						}
						else
						{
							return type;
						}
					}
					else
					{
						error(arguments[2].loc, "String initialiser only takes 1 (from slice) or 2 (from pointer+length) arguments, found '%ld' instead",
							arguments.size());
					}
				}
				else
				{
					error(arguments[1].loc, "Builtin type '%s' cannot be initialised with more than 1 value", type);
				}
			}
		}

		return 0;
	}
}












sst::Expr* ast::FunctionCall::typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& _arguments, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(auto ty = fs->checkIsBuiltinConstructorCall(this->name, _arguments))
	{
		auto ret = new sst::ExprCall(this->loc, ty);
		ret->callee = new sst::TypeExpr(this->loc, ty);
		ret->arguments = util::map(_arguments, [](auto e) -> sst::Expr* { return e.value; });

		return ret;
	}


	// resolve the function call here
	std::vector<FnCallArgument> ts = _arguments;

	auto gmaps = fs->convertParserTypeArgsToFIR(this->mappings);
	auto res = fs->resolveFunctionCall(this->name, ts, gmaps, this->traverseUpwards, infer);

	auto target = res.defn();
	iceAssert(target);

	if(auto strdf = dcast(sst::StructDefn, target))
	{
		auto ret = new sst::StructConstructorCall(this->loc, strdf->type);

		ret->target = strdf;
		ret->arguments = ts;

		return ret;
	}
	else if(auto uvd = dcast(sst::UnionVariantDefn, target))
	{
		auto unn = uvd->parentUnion;
		iceAssert(unn);

		auto ret = new sst::UnionVariantConstructor(this->loc, unn->type);

		ret->variantId = unn->type->toUnionType()->getIdOfVariant(uvd->id.name);
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
			auto ret = new sst::ClassConstructorCall(this->loc, fnd->parentTypeForMethod);

			ret->target = fnd;
			ret->arguments = ts;
			ret->classty = dcast(sst::ClassDefn, fs->typeDefnMap[fnd->parentTypeForMethod]);

			iceAssert(ret->target);

			return ret;
		}



		auto call = new sst::FunctionCall(this->loc, target->type->toFunctionType()->getReturnType());
		call->name = this->name;
		call->target = target;
		call->arguments = ts;

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










sst::Expr* ast::ExprCall::typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& arguments, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;

	std::vector<Param> ts = util::map(arguments, [](auto e) -> Param { return Param(e); });

	auto target = this->callee->typecheck(fs).expr();
	iceAssert(target);

	if(!target->type->isFunctionType())
		error(this->callee, "Expression with non-function-type '%s' cannot be called");

	auto ft = target->type->toFunctionType();
	auto [ dist, errs ] = sst::computeOverloadDistance(fs, this, util::map(ft->getArgumentTypes(), [](fir::Type* t) -> auto { return Param(t); }),
		ts, false, 0);

	if(errs.hasErrors() || dist == -1)
		errs.set(SimpleError(this->loc, "Mismatched types in call to function pointer")).postAndQuit();

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
			//* note: theoretically, this should be the only place in the compiler where we deal with splats explitily
			//* and it should remain this way.

			//? actually we also deal with splats when destructuring stuff.

			auto thing = splat->expr->typecheck(this).expr();

			if(thing->type->isTupleType())
			{
				if(!arg.first.empty())
					error(thing->loc, "Splatted tuples cannot be passed as a named argument");

				auto tty = thing->type->toTupleType();
				for(size_t i = 0; i < tty->getElementCount(); i++)
				{
					auto tdo = new sst::TupleDotOp(thing->loc, tty->getElementN(i));
					tdo->index = i;
					tdo->lhs = thing;

					ret.push_back(FnCallArgument(thing->loc, arg.first, tdo, arg.second));
				}
			}
			else if(thing->type->isArraySliceType())
			{
				auto fca = FnCallArgument(splat->expr->loc, arg.first, thing, arg.second);
				fca.wasSplat = true;

				ret.push_back(fca);
			}
			else
			{
				error(thing->loc, "Only tuples and slices can be splatted in a function call, have type '%s'", thing->type);
			}
		}
		else
		{
			ret.push_back(FnCallArgument(arg.second->loc, arg.first, arg.second->typecheck(this).expr(), arg.second));
		}
	}

	return ret;
}


TCResult ast::ExprCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return TCResult(this->typecheckWithArguments(fs, fs->typecheckCallArguments(this->args), infer));
}

TCResult ast::FunctionCall::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	return TCResult(this->typecheckWithArguments(fs, fs->typecheckCallArguments(this->args), infer));
}








