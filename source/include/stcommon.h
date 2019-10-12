// stcommon.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace sst
{
	struct VarDefn;
}

namespace ast
{
	struct Expr;
}

namespace attr
{
	constexpr uint32_t FN_ENTRYPOINT    = 0x1;
	constexpr uint32_t NO_MANGLE        = 0x2;
	constexpr uint32_t RAW              = 0x4;
	constexpr uint32_t COMPILER_SUPPORT = 0x8;
}

struct AttributeSet
{
	struct UserAttrib
	{
		std::string name;
		std::vector<std::string> args;
	};

	std::vector<UserAttrib> userAttribs;

	template <typename T>
	bool has(T x) const { return (this->flags & x); }

	template <typename T>
	bool hasAny(T x) const { return (this->flags & x); }

	template <typename T, typename... Args>
	bool hasAny(T x, Args... xs) const { return (this->flags & x) || hasAny(xs...); }

	template <typename T>
	bool hasAll(T x) const { return (this->flags & x); }

	template <typename T, typename... Args>
	bool hasAll(T x, Args... xs) const { return (this->flags & x) && hasAll(xs...); }

	template <typename T>
	void set(T x) { this->flags |= x; }

	template <typename T, typename... Args>
	void set(T x, Args... xs) { this->flags |= x; set(xs...); }



	private:
	uint32_t flags;
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




















