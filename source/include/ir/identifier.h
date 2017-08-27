// identifier.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "precompile.h"

namespace fir
{
	struct Type;

	std::string mangleGenericTypes(std::map<std::string, fir::Type*> tm);
}

// enum class IdKind
// {
// 	Invalid,
// 	Name,
// 	Variable,
// 	Function,
// 	Method,
// 	Getter,
// 	Setter,
// 	Operator,
// 	AutoGenFunc,
// 	ModuleConstructor,
// 	Struct,
// };

// struct Identifier
// {
// 	std::string name;
// 	std::vector<std::string> scope;
// 	IdKind kind = IdKind::Invalid;

// 	std::vector<fir::Type*> functionArguments;

// 	// defined in CodegenUtils.cpp
// 	bool operator == (const Identifier& other) const;
// 	bool operator != (const Identifier& other) const { return !(*this == other); }

// 	std::string str() const;
// 	std::string mangled() const;

// 	Identifier() { }
// 	Identifier(std::string _name, IdKind _kind) : name(_name), scope({ }), kind(_kind) { }
// 	Identifier(std::string _name, std::vector<std::string> _scope, IdKind _kind) : name(_name), scope(_scope), kind(_kind) { }
// };

