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
	TypeDefn* TypecheckState::instantiateGenericType(ast::TypeDefn* type, const std::unordered_map<std::string, fir::Type*>& mappings)
	{
		iceAssert(type);
		iceAssert(!type->generics.empty());

		this->pushGenericTypeContext();


		// TODO: this is not re-entrant, clearly. should we have a cleaner way of doing this?
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

		auto restore = type->generics;
		type->generics.clear();

		auto oldname = type->name;

		auto mapToString = [&mappings]() -> std::string {
			std::string ret;
			for(auto m : mappings)
				ret += (m.first + ":" + m.second->encodedStr()) + ",";

			// shouldn't be empty.
			iceAssert(ret.size() > 0);
			return ret.substr(0, ret.length() - 1);
		};

		// we mangle the name so that we can't inadvertantly 'find' the most-recently-instantiated generic type simply by giving the name without the
		// type parameters.
		// fear not, we won't be using name-mangling-based lookup (unlike the previous compiler version, ewwww)
		type->name = oldname + "<" + mapToString() + ">";


		auto ret = dcast(TypeDefn, type->typecheck(this));
		iceAssert(ret);

		type->generics = restore;
		type->name = oldname;

		type->genericVersions[mappings] = ret;

		this->popGenericTypeContext();
		return ret;
	}
}

















