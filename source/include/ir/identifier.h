// identifier.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <vector>

#include "iceassert.h"

namespace fir
{
	struct Type;
}

enum class IdKind
{
	Invalid,
	Name,
	Variable,
	Function,
	Method,
	Getter,
	Setter,
	Operator,
	AutoGenFunc,
	ModuleConstructor,
	Struct,
};

struct Identifier
{
	std::string name;
	std::vector<std::string> scope;
	IdKind kind = IdKind::Invalid;

	std::vector<fir::Type*> functionArguments;

	// defined in CodegenUtils.cpp
	bool operator == (const Identifier& other) const;
	bool operator != (const Identifier& other) const { return !(*this == other); }

	std::string str() const;
	std::string mangled() const;

	Identifier() { }
	Identifier(std::string _name, IdKind _kind) : name(_name), scope({ }), kind(_kind) { }
	Identifier(std::string _name, std::vector<std::string> _scope, IdKind _kind) : name(_name), scope(_scope), kind(_kind) { }
};

namespace std
{
	template<>
	struct hash<Identifier>
	{
		std::size_t operator()(const Identifier& k) const
		{
			using std::size_t;
			using std::hash;
			using std::string;

			// Compute individual hash values for first,
			// second and third and combine them using XOR
			// and bit shifting:

			// return ((hash<string>()(k.name) ^ (hash<std::vector<std::string>>()(k.scope) << 1)) >> 1) ^ (hash<int>()(k.third) << 1);
			return hash<string>()(k.str());
		}
	};
}

