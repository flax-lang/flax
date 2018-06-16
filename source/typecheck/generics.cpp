// generics.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

//* helper method that abstracts away the common error-checking
std::pair<bool, sst::Defn*> ast::Parameterisable::checkForExistingDeclaration(sst::TypecheckState* fs, const TypeParamMap_t& gmaps)
{
	if(this->generics.size() > 0 && gmaps.empty())
	{
		if(const auto& tys = fs->stree->unresolvedGenericDefs[this->name]; std::find(tys.begin(), tys.end(), this) == tys.end())
			fs->stree->unresolvedGenericDefs[this->name].push_back(this);

		return { false, 0 };
	}
	else
	{
		//! ACHTUNG !
		//* IMPORTANT *
		/*
			? the reason we match the *ENTIRE* generic context stack when checking for an existing definition is because of nesting.
			* if we only checked the current map, then for methods of generic types and/or nested, non-generic types inside generic types,
			* we'd match an existing definition even though all the generic types are probably completely different.

			* so, pretty much the only way to make sure we're absolutely certain it's the same context is to compare the entire type stack.

			? given that a given definition cannot 'move' to another scope, there cannot be circumstances where we can (by chance or otherwise)
			? be typechecking the current definition in another, completely different context, and somehow mistake it for our own -- even if all
			? the generic types match in the stack.

			* note: bug fix: what we should really be checking for is that the stored generic map is a strict child (ie. the last N elements match
			* our stored state, while the preceding ones don't matter). (this is why we use reverse iterators for std::equal)
		*/

		{
			auto doRootsMatch = [](const std::vector<TypeParamMap_t>& expected, const std::vector<TypeParamMap_t>& given) -> bool {
				if(given.size() < expected.size())
					return false;

				return std::equal(expected.rbegin(), expected.rend(), given.rbegin());
			};

			for(const auto& gv : this->genericVersions)
			{
				if(doRootsMatch(gv.second, fs->getGenericContextStack()))
					return { true, gv.first };
			}

			//? note: if we call with an empty map, then this is just a non-generic type/function/thing. Even for such things,
			//? the genericVersions list will have 1 entry which is just the type itself.
			return { true, 0 };
		}
	}
}


namespace sst
{
	std::vector<TypeParamMap_t> TypecheckState::getGenericContextStack()
	{
		return this->genericContextStack;
	}

	void TypecheckState::pushGenericContext()
	{
		this->genericContextStack.push_back({ });
	}

	void TypecheckState::addGenericMapping(const std::string& name, fir::Type* ty)
	{
		iceAssert(this->genericContextStack.size() > 0);
		if(auto it = this->genericContextStack.back().find(name); it != this->genericContextStack.back().end())
			error(this->loc(), "Mapping for type parameter '%s' already exists in current context (is currently '%s')", name, it->second);

		this->genericContextStack.back()[name] = ty;
	}

	void TypecheckState::removeGenericMapping(const std::string& name)
	{
		iceAssert(this->genericContextStack.size() > 0);
		if(auto it = this->genericContextStack.back().find(name); it == this->genericContextStack.back().end())
			error(this->loc(), "No mapping for type parameter '%s' exists in current context, cannot remove", name);

		else
			this->genericContextStack.back().erase(it);
	}

	void TypecheckState::popGenericContext()
	{
		iceAssert(this->genericContextStack.size() > 0);
		this->genericContextStack.pop_back();
	}


	fir::Type* TypecheckState::findGenericMapping(const std::string& name, bool allowFail)
	{
		// look upwards.
		for(auto it = this->genericContextStack.rbegin(); it != this->genericContextStack.rend(); it++)
			if(auto iit = it->find(name); iit != it->end())
				return iit->second;

		if(allowFail)   return 0;
		else            error(this->loc(), "No mapping for type parameter '%s'", name);
	}


	TypeParamMap_t TypecheckState::convertParserTypeArgsToFIR(const std::unordered_map<std::string, pts::Type*>& gmaps, bool allowFailure)
	{
		TypeParamMap_t ret;
		for(const auto& [ name, type ] : gmaps)
			ret[name] = this->convertParserTypeToFIR(type, allowFailure);

		return ret;
	}









	static std::string listToEnglish(const std::vector<std::string>& list)
	{
		std::string mstr;
		if(list.size() == 1)
		{
			mstr = strprintf("'%s'", list[0]);
		}
		else if(list.size() == 2)
		{
			mstr = strprintf("'%s' and '%s'", list[0], list[1]);
		}
		else
		{
			for(size_t i = 0; i < list.size() - 1; i++)
				mstr += strprintf("'%s', ", list[i]);

			// oxford comma is important.
			mstr += strprintf("and '%s'", list.back());
		}

		return mstr;
	}

	static std::vector<std::string> isSolutionComplete(const std::unordered_map<std::string, TypeConstraints_t>& needed, const TypeParamMap_t& solution,
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

	static SimpleError complainAboutMissingSolutions(TypecheckState* fs, ast::Parameterisable* thing, const std::vector<std::string>& missing)
	{
		auto mstr = listToEnglish(missing);
		return SimpleError::make(fs->loc(), "Type %s %s could not be inferred",
			util::plural("parameter", missing.size()), mstr);
	}



	//* gets an generic type in the AST form and returns a concrete SST node from it, given the mappings.
	TCResult TypecheckState::instantiateGenericEntity(ast::Parameterisable* thing, const TypeParamMap_t& mappings)
	{
		iceAssert(thing);
		iceAssert(!thing->generics.empty());

		this->pushGenericContext();
		defer(this->popGenericContext());

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
					SimpleError::make(this->loc(), "Cannot map type '%s' to type parameter '%s' in instantiation of generic type '%s'",
						map.second, map.first, thing->name)
					.append(SimpleError::make(MsgType::Note, thing, "replacement type has pointer degree %d, which is less than the required %d",
						ptrs, thing->generics[map.first].pointerDegree))
				);
			}

			// TODO: check if the type conforms to the protocols specified.
			//* check if it satisfies the protocols.

			// ok, push the thing.
			this->addGenericMapping(map.first, map.second);
		}



		// check if we provided all the required mappings.
		{
			// TODO: make an elegant early-out for this situation?
			if(auto missing = isSolutionComplete(thing->generics, mappings, /* allowPlaceholders: */ true); missing.size() > 0)
			{
				return TCResult(complainAboutMissingSolutions(this, thing, missing)
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
						SimpleError::make(this->loc(), "Parametric entity '%s' does not have an argument '%s'", thing->name, name)
						.append(SimpleError::make(MsgType::Note, thing, "'%s' was defined here:", thing->name))
					);
				}
			}
		}




		auto mapToString = [&mappings]() -> std::string {
			std::string ret;
			for(auto m : mappings)
				ret += (m.first + ":" + m.second->encodedStr()) + ",";

			// shouldn't be empty.
			iceAssert(ret.size() > 0);
			return ret.substr(0, ret.length() - 1);
		};


		// TODO: this is not re-entrant, clearly. should we have a cleaner way of doing this?
		//* we mangle the name so that we can't inadvertantly 'find' the most-recently-instantiated generic type simply by giving the name without the
		//* type parameters.
		//? fear not, we won't be using name-mangling-based lookup (unlike the previous compiler version, ewwww)

		auto oldname = thing->name;
		thing->name = oldname + "<" + mapToString() + ">";

		//* we **MUST** first call ::generateDeclaration if we're doing a generic thing.
		//* with the mappings that we're using to instantiate it.
		thing->generateDeclaration(this, 0, mappings);


		// now it is 'safe' to call typecheck.
		auto ret = dcast(Defn, thing->typecheck(this, 0, mappings).defn());
		iceAssert(ret);

		thing->name = oldname;
		return TCResult(ret);
	}




	// forward declare this
	static sst::Defn* fillGenericTypeWithPlaceholders(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& partial);

	static std::pair<Defn*, SimpleError> attemptToInstantiateGenericThing(TypecheckState* fs, ast::Parameterisable* thing,
		const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>& args, bool fillplaceholders)
	{
		if(auto fd = dcast(ast::FuncDefn, thing); fd && !isFnCall && infer == 0 && fillplaceholders)
			return { fillGenericTypeWithPlaceholders(fs, thing, _gmaps), SimpleError() };

		auto [ gmaps, substitutions, err ] = fs->inferTypesForGenericEntity(thing, args, _gmaps, infer, isFnCall);

		//* note: if we manage to instantite without error, that means that we have a complete solution.
		auto d = fs->instantiateGenericEntity(thing, gmaps);

		if(d.isDefn())
		{
			if(isFnCall)
			{
				if(auto missing = isSolutionComplete(thing->generics, gmaps, /* allowPlaceholders: */ false); missing.size() > 0)
				{
					auto se = SpanError().set(complainAboutMissingSolutions(fs, thing, missing));
					se.top.loc = thing->loc;

					size_t ctr = 0;
					for(const auto& arg : args)
					{
						if(arg.value->type->containsPlaceholders())
						{
							auto loc = arg.loc;
							if(auto fd = dcast(FunctionDefn, d.defn()))
								loc = fd->params[ctr].loc;

							se.add(SpanError::Span(loc, strprintf("Unresolved inference placeholder(s) in argument type; partially solved as '%s'",
								arg.value->type->substitutePlaceholders(substitutions))));
						}

						ctr++;
					}

					return { nullptr, err.append(se) };
				}
				else
				{
					for(FnCallArgument& arg : args)
					{
						//! ACHTUNG !
						//* here, we modify the input appropriately.
						//* i don't see a better way to do it.

						iceAssert(arg.orig);
						arg.value = arg.orig->typecheck(fs, arg.value->type->substitutePlaceholders(substitutions)).expr();
					}

					return { d.defn(), SimpleError() };
				}
			}
			else
			{
				return { d.defn(), SimpleError() };
			}
		}
		else
		{
			iceAssert(d.isError());
			return { nullptr, err.append(d.error()) };
		}
	}






	static int placeholderGroupID = 0;
	static sst::Defn* fillGenericTypeWithPlaceholders(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& partial)
	{
		// this will just call back into 'attemptToDisamiguateBlaBla' with filled in types for the partial solution.
		TypeParamMap_t copy = partial;

		for(const auto& p : thing->generics)
		{
			if(copy.find(p.first) == copy.end())
				copy[p.first] = fir::PolyPlaceholderType::get(p.first, placeholderGroupID);
		}

		placeholderGroupID++;

		std::vector<FnCallArgument> fake;
		auto def = attemptToInstantiateGenericThing(fs, thing, copy, /* infer: */ 0, /* isFnCall: */ false,
			/* args: */ fake, /* fillplaceholders: */ false).first;

		iceAssert(def);
		return def;
	}


	TCResult TypecheckState::attemptToDisambiguateGenericReference(const std::string& name, const std::vector<ast::Parameterisable*>& gdefs,
		const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>& args)
	{
		// make a copy
		TypeParamMap_t gmaps = _gmaps;

		iceAssert(gdefs.size() > 0);

		//? now if we have multiple things then we need to try them all, which can get real slow real quick.
		//? unfortunately I see no better way to do this.
		// TODO: find a better way to do this??

		std::vector<sst::Defn*> pots;
		std::vector<std::pair<ast::Parameterisable*, SimpleError>> failures;

		for(const auto& gdef : gdefs)
		{
			iceAssert(gdef->name == name);

			auto [ def, err ] = attemptToInstantiateGenericThing(this, gdef, gmaps, /* infer: */ infer, /* isFnCall: */ isFnCall,
				/* args:  */ args, /* fillplaceholders: */ true);

			if(def) pots.push_back(def);
			else    failures.push_back({ gdef, err });


			#if 0
 			BareError err;
			std::unordered_map<fir::Type*, fir::Type*> substitutions;
			std::tie(gmaps, substitutions, err) = this->inferTypesForGenericEntity(gdef, args, gmaps, infer, isFnCall);

			//* note: if we manage to instantite without error, that means that we have a complete solution.
			auto d = this->instantiateGenericEntity(gdef, gmaps);

			if(d.isDefn())
			{
				if(isFnCall)
				{
					if(auto missing = isSolutionComplete(gdef->generics, gmaps, /* allowPlaceholders: */ false); missing.size() > 0)
					{
						auto se = SpanError().set(complainAboutMissingSolutions(this, gdef, missing));
						se.top.loc = gdef->loc;

						size_t ctr = 0;
						for(const auto& arg : args)
						{
							if(arg.value->type->containsPlaceholders())
							{
								auto loc = arg.loc;
								if(auto fd = dcast(FunctionDefn, d.defn()))
									loc = fd->params[ctr].loc;

								se.add(SpanError::Span(loc, strprintf("Unresolved inference placeholder(s) in argument type; partially solved as '%s'",
									arg.value->type->substitutePlaceholders(substitutions))));
							}

							ctr++;
						}

						failures.push_back(std::make_pair(gdef, err.append(se)));
					}
					else
					{
						pots.push_back(d.defn());
						for(FnCallArgument& arg : args)
						{
							//! ACHTUNG !
							//* here, we modify the input appropriately.
							//* i don't see a better way to do it.

							iceAssert(arg.orig);
							arg.value = arg.orig->typecheck(this, arg.value->type->substitutePlaceholders(substitutions)).expr();
						}
					}
				}
				else
				{
					pots.push_back(d.defn());
				}
			}
			else
			{
				{
					iceAssert(d.isError());
					if(err.hasErrors())
						d.error() = err;

					failures.push_back(std::make_pair(gdef, BareError().append(d.error())));
				}
			}
			#endif
		}

		if(!pots.empty())
		{
			if(pots.size() > 1)
			{
				auto errs = SimpleError::make(this->loc(), "Ambiguous reference to parametric entity '%s'", name);
				for(auto g : pots)
					errs.append(SimpleError(g->loc, "Potential target here:", MsgType::Note));

				return TCResult(errs);
			}
			else
			{
				// ok, great. just return that shit.
				iceAssert(pots[0]);
				return TCResult(pots[0]);
			}
		}
		else
		{
			iceAssert(failures.size() > 0);

			auto errs = OverloadError(SimpleError::make(this->loc(), "No viable candidates in attempted instantiation of parametric entity '%s' amongst %d %s", name, failures.size(), util::plural("candidate", failures.size())));

			for(const auto& [ f, e ] : failures)
				errs.addCand(f, e);

			return TCResult(errs);
		}
	}



}






















