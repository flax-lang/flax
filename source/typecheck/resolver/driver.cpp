// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "polymorph.h"
#include "resolver.h"

#include "ir/type.h"

#include <set>

namespace sst
{
	using Param = FunctionDecl::Param;

	TCResult TypecheckState::resolveFunctionCallFromCandidates(const std::vector<Defn*>& cands, std::vector<FnCallArgument>* args,
		const TypeParamMap_t& gmaps, bool allowImplicitSelf)
	{
		auto cds = util::map(cands, [&args](auto c) -> std::pair<Defn*, std::vector<FnCallArgument>> { return { c, *args }; });
		auto [ ret, new_args ] = resolver::resolveFunctionCallFromCandidates(this, this->loc(), cds, gmaps, allowImplicitSelf, nullptr);

		*args = new_args;
		return ret;
	}

	TCResult TypecheckState::resolveFunctionCall(const std::string& name, std::vector<FnCallArgument>* arguments, const TypeParamMap_t& gmaps, bool travUp,
		fir::Type* return_infer)
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
		std::vector<std::tuple<Defn*, std::vector<FnCallArgument>, poly::Solution_t>> fns;
		while(tree)
		{
			// unify the handling of generic and non-generic stuff.
			// if we provided mappings, don't bother searching normal functions.
			if(gmaps.empty())
			{
				auto defs = tree->getDefinitionsWithName(name);
				for(auto d : defs)
					fns.push_back({ d, *arguments, poly::Solution_t() });
			}

			if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(name); gdefs.size() > 0)
			{
				didGeneric = true;
				auto argcopy = *arguments;

				auto pots = poly::findPolymorphReferences(this, name, gdefs, gmaps, /* return_infer: */ return_infer,
					/* type_infer: */ 0, /* isFnCall: */ true, &argcopy);

				for(const auto& pot : pots)
				{
					if(!pot.first.isDefn())
					{
						iceAssert(pot.first.isError());
						errs.append(pot.first.error());
					}
					else
					{
						auto def = pot.first.defn();
						if(def->type->containsPlaceholders())
							error("wtf??? '%s'", def->type);

						fns.push_back({ def, argcopy, pot.second });
					}
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


		std::vector<std::pair<sst::Defn*, std::vector<FnCallArgument>>> cands;
		for(const auto& [ def, args, soln ] : fns)
		{
			auto ts = args; // copy it.

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

		auto [ res, new_args ] = resolver::resolveFunctionCallFromCandidates(this, this->loc(), cands, gmaps, travUp, return_infer);
		if(res.isDefn())
		{
			*arguments = new_args;

			return res;
		}
		else
		{
			return TCResult(errs.append(res.error()));
		}
	}




	TCResult TypecheckState::resolveConstructorCall(TypeDefn* typedf, const std::vector<FnCallArgument>& arguments,
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

			auto copy = arguments;

			//! SELF HANDLING
			copy.push_back(FnCallArgument(cls->loc, "self", new TypeExpr(cls->loc, cls->type->getMutablePointerTo()), nullptr));

			auto cand = this->resolveFunctionCallFromCandidates(util::map(cls->initialisers, [](auto e) -> auto {
				return dcast(sst::Defn, e);
			}), &copy, gmaps, true);

			// TODO: support re-eval of constructor args!
			// TODO: support re-eval of constructor args!
			// TODO: support re-eval of constructor args!

			// if(copy != arguments)
			// 	error("args changed for constructor call -- fixme!!!");

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
			std::set<std::string> fieldNames;
			for(auto f : str->fields)
				fieldNames.insert(f->id.name);

			auto err = resolver::verifyStructConstructorArguments(this->loc(), str->id.name, fieldNames, util::map(arguments, [](auto fca) -> auto {
				return Param(fca);
			}));

			if(err.second.hasErrors())
				return TCResult(err.second);

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

			auto [ dist, errs ] = resolver::computeOverloadDistance(unn->loc, target, util::map(arguments, [](auto fca) -> auto {
				return Param(fca);
			}), false);

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
						else if(auto t2 = arguments[1].value->type; fir::getCastDistance(t2, fir::Type::getInt64()) < 0)
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

















