// stcommon.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace sst
{
	struct VarDefn;
	struct StateTree;

	struct Scope
	{
		Scope() { }
		Scope(StateTree* st);

		StateTree* stree = 0;
		const Scope* prev = 0;

		std::string string() const;
		std::vector<std::string> getStrings() const;
		const Scope& appending(const std::string& name) const;
	};
}

namespace ast
{
	struct Expr;
}

namespace attr
{
	using FlagTy = uint32_t;

	constexpr FlagTy NONE               = 0x0;
	constexpr FlagTy FN_ENTRYPOINT      = 0x1;
	constexpr FlagTy NO_MANGLE          = 0x2;
	constexpr FlagTy RAW                = 0x4;
	constexpr FlagTy PACKED             = 0x8;
}

struct AttribSet
{
	using FlagTy = attr::FlagTy;

	struct UserAttrib
	{
		UserAttrib(const std::string& name, const std::vector<std::string>& args) : name(name), args(args) { }

		std::string name;
		std::vector<std::string> args;
	};



	bool has(const std::string& uaName)
	{
		return std::find_if(this->userAttribs.begin(), this->userAttribs.end(), [uaName](const UserAttrib& ua) -> bool {
			return ua.name == uaName;
		}) != this->userAttribs.end();
	}

	UserAttrib get(const std::string& uaName)
	{
		auto it = std::find_if(this->userAttribs.begin(), this->userAttribs.end(), [uaName](const UserAttrib& ua) -> bool {
			return ua.name == uaName;
		});

		// would use optionals or something but lazy
		if(it != this->userAttribs.end())   return *it;
		else                                return UserAttrib("", {});
	}

	void set(FlagTy x) { this->flags |= x; }
	bool has(FlagTy x) const { return (this->flags & x); }
	bool hasAny(FlagTy x) const { return (this->flags & x); }
	bool hasAll(FlagTy x) const { return (this->flags & x); }

	template <typename... Args> void set(FlagTy x, Args... xs) { this->flags |= x; set(xs...); }
	template <typename... Args> bool hasAny(FlagTy x, Args... xs) const { return (this->flags & x) || hasAny(xs...); }
	template <typename... Args> bool hasAll(FlagTy x, Args... xs) const { return (this->flags & x) && hasAll(xs...); }

	void add(const UserAttrib& ua)
	{
		this->userAttribs.push_back(ua);
	}

	static AttribSet of(FlagTy flags, const std::vector<UserAttrib>& attribs = {})
	{
		return AttribSet {
			.flags = flags,
			.userAttribs = attribs
		};
	}

	FlagTy flags = attr::NONE;
	std::vector<UserAttrib> userAttribs;
};


struct DecompMapping
{
	Location loc;
	std::string name;
	bool ref = false;
	bool array = false;

	sst::VarDefn* createdDefn = 0;

	std::vector<DecompMapping> inner;

	// for array decompositions, this will hold the rest.
	std::string restName;
	bool restRef = false;
	sst::VarDefn* restDefn = 0;
};

struct FnCallArgument
{
	FnCallArgument() { }
	FnCallArgument(const Location& l, const std::string& n, sst::Expr* v, ast::Expr* o) : loc(l), name(n), value(v), orig(o) { }

	Location loc;
	std::string name;
	sst::Expr* value = 0;

	ast::Expr* orig = 0;
	bool ignoreName = false;

	static FnCallArgument make(const Location& l, const std::string& n, fir::Type* t, bool ignoreName = false);

	bool operator == (const FnCallArgument& other) const
	{
		return this->loc == other.loc && this->name == other.name && this->value == other.value && this->orig == other.orig;
	}
};

struct FnParam
{
	FnParam() { }
	FnParam(const Location& l, const std::string& n, fir::Type* t) : loc(l), name(n), type(t) { }

	Location loc;
	std::string name;
	fir::Type* type = 0;
	sst::Expr* defaultVal = 0;
};




















