// instantiator.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"
#include "ir/type.h"

#include "typecheck.h"
#include "polymorph.h"
#include "resolver.h"

namespace sst {
namespace poly
{
	namespace internal
	{
		static SimpleError* complainAboutMissingSolutions(const Location& l, ast::Parameterisable* thing,
			const std::vector<fir::PolyPlaceholderType*>& missing)
		{
			auto strs = util::map(missing, [](fir::PolyPlaceholderType* p) -> std::string {
				return p->str().substr(1);
			});

			auto mstr = util::listToEnglish(strs);
			return SimpleError::make(l, "type %s %s could not be inferred", util::plural("parameter", strs.size()), mstr);
		}

		std::vector<std::string> getMissingSolutions(const ProblemSpace_t& needed, const TypeParamMap_t& solution,
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


		std::pair<TCResult, Solution_t> solvePolymorphWithPlaceholders(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
			const TypeParamMap_t& partial)
		{
			TypeParamMap_t copy = partial;

			int session = getNextSessionId();
			for(const auto& p : thing->generics)
			{
				if(copy.find(p.first) == copy.end())
					copy[p.first] = fir::PolyPlaceholderType::get(p.first, session);
			}

			return attemptToInstantiatePolymorph(fs, thing, name, copy, /* return_infer: */ 0, /* type_infer: */ 0,
				/* isFnCall: */ false, /* args: */ { }, /* fillplaceholders: */ false);
		}
	}






	std::pair<TCResult, Solution_t> attemptToInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
		const TypeParamMap_t& _gmaps, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall, std::vector<FnCallArgument>* args,
		bool fillplaceholders, fir::Type* problem_infer)
	{
		if(!isFnCall && type_infer == 0 && fillplaceholders)
			return internal::solvePolymorphWithPlaceholders(fs, thing, name, _gmaps);

		// used below.
		util::hash_map<std::string, size_t> origParamOrder;
		auto [ soln, err ] = internal::inferTypesForPolymorph(fs, thing, name, thing->generics, *args, _gmaps, return_infer, type_infer, isFnCall,
			problem_infer, &origParamOrder);

		if(err) return { TCResult(err), soln };

		if(auto d = fullyInstantiatePolymorph(fs, thing, soln.solutions); d.isDefn())
		{
			if(isFnCall)
			{
				// if(auto missing = internal::getMissingSolutions(thing->generics, soln.solutions, /* allowPlaceholders: */ false); missing.size() > 0)
				if(auto missing = d.defn()->type->getContainedPlaceholders(); missing.size() > 0)
				{
					auto se = SpanError::make(internal::complainAboutMissingSolutions(thing->loc, thing, missing));

					return std::make_pair(
						TCResult(err->append(se)->append(SimpleError::make(fs->loc(), "partial solution: %s",
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
					size_t counter = 0;
					for(auto& arg : *args)
					{
						if(arg.value->type->containsPlaceholders() && arg.orig)
						{
							//! ACHTUNG !
							/*
								* note *
								the implication here is that by calling 'inferTypesForPolymorph', which itself calls 'solveTypeList',
								we will have weeded out all of the function candidates that don't match (ie. overload distance == -1)

								therefore, we can operate under the assumption that the _parameter_ type of the function will be fully
								substituted and not have any placeholders, and that it will be a valid infer target for the _argument_.

								using the newly-gained arg_infer information, we can re-typecheck the argument with concrete types.

								- zhiayang
								- 06/10/18/2318
							*/

							auto arg_infer = d.defn()->type->toFunctionType()->getArgumentN(arg.name.empty() ? counter : origParamOrder[arg.name]);

							auto tc = arg.orig->typecheck(fs, arg_infer);
							arg.value = tc.expr();
						}

						counter += 1;
					}
				}
			}

			return std::make_pair(TCResult(d.defn()), soln);
		}
		else
		{
			iceAssert(d.isError());
			return std::make_pair(TCResult(d.error()), soln);
		}
	}






	//* gets an generic type in the AST form and returns a concrete SST node from it, given the mappings.
	TCResult fullyInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& mappings)
	{
		iceAssert(thing);
		// iceAssert(!thing->generics.empty());

		// try to see if we already have a generic version.
		if(auto [ found, def ] = thing->checkForExistingDeclaration(fs, mappings); found && def)
			return TCResult(def);


		fs->pushGenericContext();
		defer(fs->popGenericContext());

		for(auto map : mappings)
		{
			int ptrs = 0;
			{
				auto t = map.second;

				while(t->isPointerType())
					t = t->getPointerElementType(), ptrs++;
			}

			if(auto it = std::find_if(thing->generics.begin(), thing->generics.end(), [&map](const auto& p) -> bool {
				return map.first == p.first;
			}); it != thing->generics.end() && ptrs < it->second.pointerDegree)
			{
				return TCResult(
					SimpleError::make(fs->loc(), "cannot map type '%s' to type parameter '%s' in instantiation of generic type '%s'",
						map.second, map.first, thing->name)->append(
							SimpleError::make(MsgType::Note, thing->loc,
								"replacement type has pointer degree %d, which is less than the required %d", ptrs, it->second.pointerDegree)
							)
				);
			}

			// TODO: check if the type conforms to the protocols specified.
			//* check if it satisfies the protocols.

			// ok, push the thing.
			fs->addGenericMapping(map.first, map.second);
		}

		fir::Type* self_infer = 0;
		if(thing->parentType)
		{
			// instantiate that as well, I guess.
			auto res = fullyInstantiatePolymorph(fs, thing->parentType, /* mappings */ {});
			if(res.isError()) return TCResult(res);

			self_infer = res.defn()->type;
		}

		if(auto missing = internal::getMissingSolutions(thing->generics, mappings, true); missing.size() > 0)
		{
			auto mstr = util::listToEnglish(missing);
			return TCResult(SimpleError::make(fs->loc(), "type %s %s could not be inferred", util::plural("parameter", missing.size()), mstr));
		}

		return thing->typecheck(fs, self_infer, mappings);
	}
}
}





































