// instantiator.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"
#include "ir/type.h"

#include "typecheck.h"
#include "polymorph.h"
#include "polymorph_internal.h"

namespace sst {
namespace poly
{
	std::vector<std::string> getMissingSolutions(const std::unordered_map<std::string, TypeConstraints_t>& needed, const TypeParamMap_t& solution,
		bool allowPlaceholders)
	{
		std::vector<std::string> missing;
		for(const auto& [ name, constr ] : needed)
		{
			(void) constr;

			if(auto it = solution.find(name); it == solution.end())
				missing.push_back(name);

			else if(!allowPlaceholders && it->second->containsPlaceholders())
				missing.push_back(name);
		}

		return missing;
	}

	static SimpleError complainAboutMissingSolutions(const Location& l, ast::Parameterisable* thing, const std::vector<std::string>& missing)
	{
		auto mstr = util::listToEnglish(missing);
		return SimpleError::make(l, "Type %s %s could not be inferred", util::plural("parameter", missing.size()), mstr);
	}


	static std::pair<TCResult, Solution_t> solvePolymorphWithPlaceholders(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& partial)
	{
		TypeParamMap_t copy = partial;

		int session = getNextSessionId();
		for(const auto& p : thing->generics)
		{
			if(copy.find(p.first) == copy.end())
				copy[p.first] = fir::PolyPlaceholderType::get(p.first, session);
		}

		return attemptToInstantiatePolymorph(fs, thing, copy, /* infer: */ 0, /* isFnCall: */ false, /* args: */ { }, /* fillplaceholders: */ false);
	}


	std::pair<TCResult, Solution_t> attemptToInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& _gmaps,
		fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>* args, bool fillplaceholders)
	{
		if(!isFnCall && infer == 0 && fillplaceholders)
			return solvePolymorphWithPlaceholders(fs, thing, _gmaps);


		// intentionally do not check the error here. we will just try to instantiate the thing regardless.
		auto [ soln, err ] = inferTypesForPolymorph(fs, thing, *args, _gmaps, infer, isFnCall);
		auto d = fullyInstantiatePolymorph(fs, thing, soln.solutions);

		if(d.isDefn())
		{
			if(isFnCall)
			{
				if(auto missing = getMissingSolutions(thing->generics, soln.solutions, /* allowPlaceholders: */ false); missing.size() > 0)
				{
					auto se = SpanError().set(complainAboutMissingSolutions(fs->loc(), thing, missing));
					se.top.loc = thing->loc;

					return std::make_pair(
						TCResult(err.append(se).append(SimpleError::make(fs->loc(), "partial solution: %s",
							util::listToString(util::map(util::pairs(soln.solutions), [](const std::pair<std::string, fir::Type*>& p) -> std::string {
								return strprintf("%s = %s", p.first, p.second);
							}), [](const std::string& s) -> std::string {
								return s;
							}
						)))), soln
					);
				}
				else
				{
					for(FnCallArgument& arg : *args)
					{
						//! ACHTUNG !
						//* here, we modify the input appropriately. i don't see a better way to do it.

						if(arg.orig)
						{
							arg.value = arg.orig->typecheck(fs, arg.value->type->substitutePlaceholders(soln.substitutions)).expr();
						}
					}
				}
			}

			return std::make_pair(TCResult(d.defn()), soln);
		}
		else
		{
			return std::make_pair(TCResult(err.append(d.error())), soln);
		}
	}






	//* gets an generic type in the AST form and returns a concrete SST node from it, given the mappings.
	TCResult fullyInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& mappings)
	{
		iceAssert(thing);
		iceAssert(!thing->generics.empty());

		// try to see if we already have a generic version.
		if(auto [ found, def ] = thing->checkForExistingDeclaration(fs, mappings); thing && def)
			return TCResult(def);


		fs->pushGenericContext();
		defer(fs->popGenericContext());

		//* allowFail is only allowed to forgive a failure when we're checking for type conformance to protocols or something like that.
		//* we generally don't look into type or function bodies when checking stuff, and it'd be hard to check for something like this (eg.
		//* T passes all the checks, but causes some kind of type-checking failure when substituted in)
		// TODO: ??? do we want this to be the behaviour ???
		for(auto map : mappings)
		{
			int ptrs = 0;
			{
				auto t = map.second;

				while(t->isPointerType())
					t = t->getPointerElementType(), ptrs++;
			}

			if(thing->generics.find(map.first) != thing->generics.end() && ptrs < thing->generics[map.first].pointerDegree)
			{
				return TCResult(
					SimpleError::make(fs->loc(), "Cannot map type '%s' to type parameter '%s' in instantiation of generic type '%s'",
						map.second, map.first, thing->name)
					.append(SimpleError::make(MsgType::Note, thing, "replacement type has pointer degree %d, which is less than the required %d",
						ptrs, thing->generics[map.first].pointerDegree))
				);
			}

			// TODO: check if the type conforms to the protocols specified.
			//* check if it satisfies the protocols.

			// ok, push the thing.
			fs->addGenericMapping(map.first, map.second);
		}


		// check if we provided all the required mappings.
		{
			// TODO: make an elegant early-out for this situation?
			if(auto missing = getMissingSolutions(thing->generics, mappings, /* allowPlaceholders: */ true); missing.size() > 0)
			{
				return TCResult(complainAboutMissingSolutions(fs->loc(), thing, missing)
					.append(SimpleError::make(MsgType::Note, thing, "'%s' was defined here:", thing->name))
				);
			}

			// TODO: pretty lame, but look for things that don't exist.
			for(const auto& [ name, ty ] : mappings)
			{
				(void) ty;
				if(thing->generics.find(name) == thing->generics.end())
				{
					return TCResult(
						SimpleError::make(fs->loc(), "Parametric entity '%s' does not have an argument '%s'", thing->name, name)
						.append(SimpleError::make(MsgType::Note, thing, "'%s' was defined here:", thing->name))
					);
				}
			}
		}

		auto ret = dcast(Defn, thing->typecheck(fs, 0, mappings).defn());
		iceAssert(ret);

		return TCResult(ret);
	}
}
}





































