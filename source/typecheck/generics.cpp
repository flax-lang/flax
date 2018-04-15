// generics.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

namespace sst
{
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














	//* gets an generic type in the AST form and returns a concrete SST node from it, given the mappings.
	TypeDefn* TypecheckState::instantiateGenericType(ast::TypeDefn* type, const TypeParamMap_t& mappings)
	{
		iceAssert(type);
		iceAssert(!type->generics.empty());

		this->pushGenericTypeContext();


		for(auto map : mappings)
		{
			int ptrs = 0;
			{
				auto t = map.second;

				while(t->isPointerType())
					t = t->getPointerElementType(), ptrs++;
			}

			if(ptrs < type->generics[map.first].pointerDegree)
			{
				error(this->loc(), "Cannot map type '%s' to type parameter '%s' in instantiation of generic type '%s': replacement type has pointer degree %d, which is less than the required %d", map.second, map.first, type->name, ptrs, type->generics[map.first].pointerDegree);
			}

			// TODO: check if the type conforms to the protocols specified.
			//* check if it satisfies the protocols.

			// ok, push the thing.
			this->addGenericTypeMapping(map.first, map.second);
		}

		auto oldname = type->name;

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
		type->name = oldname + "<" + mapToString() + ">";

		//* we **MUST** first call ::generateDeclaration if we're doing a generic thing.
		//* with the mappings that we're using to instantiate it.
		type->generateDeclaration(this, 0, mappings);

		// now it is 'safe' to call typecheck.
		auto ret = dcast(TypeDefn, type->typecheck(this, 0, mappings));
		iceAssert(ret);

		type->name = oldname;


		this->popGenericTypeContext();
		return ret;
	}
}




//* these are just helper methods that abstract away the common error-checking
std::pair<bool, sst::Defn*> ast::Declarable::checkForExistingDeclaration(sst::TypecheckState* fs, const TypeParamMap_t& gmaps)
{
	if(this->generics.size() > 0 && gmaps.empty())
	{
		fs->stree->unresolvedGenericDefs[this->name].push_back(this);
		return { false, 0 };
	}
	else
	{
		//! ACHTUNG !
		//* NOTE: this implies that we **MUST** call ::generateDeclaration with the appropriate maps *before* calling typecheck on the AST
		//* if we're a generic thing.

		// check whether we've done this before
		for(const auto& gv : this->genericVersions)
		{
			if(gv.second == gmaps)
				return { true, gv.first };
		}

		//? note: if we call with an empty map, then this is just a non-generic type/function/thing. Even for such things,
		//? the genericVersions list will have 1 entry which is just the type itself.
		return { true, 0 };
	}
}

sst::Defn* ast::Declarable::getOrCreateDeclForTypechecking(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	auto defn = this->generateDeclaration(fs, infer, gmaps);
	if(!defn)
	{
		if(this->generics.empty())  error(this, "Failed to generate declaration for entity '%s'???", this->name);
		else if(gmaps.empty())      error(this, "Function '%s' cannot be referenced without providing type parameters", this->name);
		else                        error(this, "what???");
	}

	return defn;
}































