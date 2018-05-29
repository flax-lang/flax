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
		*/

		for(const auto& gv : this->genericVersions)
		{
			if(gv.second == fs->getCurrentGenericContextStack())
				return { true, gv.first };
		}

		//? note: if we call with an empty map, then this is just a non-generic type/function/thing. Even for such things,
		//? the genericVersions list will have 1 entry which is just the type itself.
		return { true, 0 };
	}
}


namespace sst
{
	std::vector<TypeParamMap_t> TypecheckState::getCurrentGenericContextStack()
	{
		return this->genericTypeContextStack;
	}

	void TypecheckState::pushGenericTypeContext()
	{
		this->genericTypeContextStack.push_back({ });
	}

	void TypecheckState::addGenericTypeMapping(const std::string& name, fir::Type* ty)
	{
		iceAssert(this->genericTypeContextStack.size() > 0);
		if(auto it = this->genericTypeContextStack.back().find(name); it != this->genericTypeContextStack.back().end())
			error(this->loc(), "Mapping for type parameter '%s' already exists in current context (is currently '%s')", name, it->second);

		this->genericTypeContextStack.back()[name] = ty;
	}

	void TypecheckState::removeGenericTypeMapping(const std::string& name)
	{
		iceAssert(this->genericTypeContextStack.size() > 0);
		if(auto it = this->genericTypeContextStack.back().find(name); it == this->genericTypeContextStack.back().end())
			error(this->loc(), "No mapping for type parameter '%s' exists in current context, cannot remove", name);

		else
			this->genericTypeContextStack.back().erase(it);
	}

	void TypecheckState::popGenericTypeContext()
	{
		iceAssert(this->genericTypeContextStack.size() > 0);
		this->genericTypeContextStack.pop_back();
	}


	fir::Type* TypecheckState::findGenericTypeMapping(const std::string& name, bool allowFail)
	{
		// look upwards.
		for(auto it = this->genericTypeContextStack.rbegin(); it != this->genericTypeContextStack.rend(); it++)
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








	static std::vector<std::string> isSolutionComplete(const std::unordered_map<std::string, TypeConstraints_t>& needed, const TypeParamMap_t& solution)
	{
		std::vector<std::string> missing;
		for(const auto& [ name, constr ] : needed)
		{
			(void) constr;

			if(solution.find(name) == solution.end())
				missing.push_back(name);
		}

		return missing;
	}


	//* gets an generic type in the AST form and returns a concrete SST node from it, given the mappings.
	TCResult TypecheckState::instantiateGenericEntity(ast::Parameterisable* thing, const TypeParamMap_t& mappings)
	{
		iceAssert(thing);
		iceAssert(!thing->generics.empty());

		this->pushGenericTypeContext();
		defer(this->popGenericTypeContext());

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
			this->addGenericTypeMapping(map.first, map.second);
		}



		// check if we provided all the required mappings.
		{
			// TODO: make an elegant early-out for this situation?
			if(auto missing = isSolutionComplete(thing->generics, mappings); missing.size() > 0)
			{
				std::string mstr;
				if(missing.size() == 1)
				{
					mstr = strprintf("'%s'", missing[0]);
				}
				else if(missing.size() == 2)
				{
					mstr = strprintf("'%s' and '%s'", missing[0], missing[1]);
				}
				else
				{
					for(size_t i = 0; i < missing.size() - 1; i++)
						mstr += strprintf("'%s', ", missing[i]);

					// oxford comma is important.
					mstr += strprintf("and '%s'", missing.back());
				}

				return TCResult(
					SimpleError::make(MsgType::Note, this->loc(), "Instantiation of parametric entity '%s' is missing type %s for %s",
					thing->name, util::plural("argument", missing.size()), mstr)
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
		return fs->attemptToDisambiguateGenericReference(thing->name, { thing }, copy, 0, false, fake).first.defn();
	}


	std::pair<TCResult, TypeParamMap_t> TypecheckState::attemptToDisambiguateGenericReference(const std::string& name,
		const std::vector<ast::Parameterisable*>& gdefs, const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall,
		std::vector<FnCallArgument>& args)
	{
		// make a copy
		TypeParamMap_t gmaps = _gmaps;

		iceAssert(gdefs.size() > 0);

		//? now if we have multiple things then we need to try them all, which can get real slow real quick.
		//? unfortunately I see no better way to do this.
		// TODO: find a better way to do this??

		std::vector<sst::Defn*> pots;
		std::vector<std::pair<ast::Parameterisable*, BareError>> failures;

		for(const auto& gdef : gdefs)
		{
			iceAssert(gdef->name == name);

			//* ok, here's the thing -- if we pass in a function reference, (ie. it's an ast::FuncDefn), but 'isFnCall' is false,
			//* then we want to fill in the 'solution' with placeholder types, so we can actually do an inference thing.

			BareError err;
			std::unordered_map<fir::Type*, fir::Type*> substitutions;
			std::tie(gmaps, substitutions, err) = this->inferTypesForGenericEntity(gdef, args, gmaps, infer);

			//* note: if we manage to instantite without error, that means that we have a complete solution.
			auto d = this->instantiateGenericEntity(gdef, gmaps);

			if(d.isDefn() && (infer ? d.defn()->type == infer : true))
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
			else
			{
				if(auto fd = dcast(ast::FuncDefn, gdef); fd && !isFnCall)
				{
					pots.push_back(fillGenericTypeWithPlaceholders(this, gdef, gmaps));
				}
				else
				{
					iceAssert(d.isError());
					if(err.hasErrors())
						d.error() = err;

					failures.push_back(std::make_pair(gdef, BareError().append(d.error())));
				}
			}
		}

		if(!pots.empty())
		{
			if(pots.size() > 1)
			{
				auto errs = SimpleError::make(this->loc(), "Ambiguous reference to parametric entity '%s'", name);
				for(auto g : pots)
					errs.append(SimpleError(g->loc, "Potential target here:", MsgType::Note));

				return { TCResult(errs), gmaps };
			}
			else
			{
				// ok, great. just return that shit.
				iceAssert(pots[0]);
				return { TCResult(pots[0]), gmaps };
			}
		}
		else
		{
			iceAssert(failures.size() > 0);

			auto errs = OverloadError(SimpleError::make(this->loc(), "No viable candidates in attempted instantiation of parametric entity '%s' amongst %d candidates", name, failures.size()));

			for(const auto& [ f, e ] : failures)
				errs.addCand(f, e);

			return { TCResult(errs), gmaps };
		}
	}



}






















