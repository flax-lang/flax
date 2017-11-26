// identifier.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "ir/type.h"

std::string Identifier::str() const
{
	std::string ret;
	for(auto s : this->scope)
		ret += s + ".";

	ret += this->name;

	if(this->kind == IdKind::Function)
	{
		ret += "(";
		for(auto p : this->params)
			ret += p->str() + ", ";

		if(this->params.size() > 0)
			ret.pop_back(), ret.pop_back();

		ret += ")";
	}

	return ret;
}



static std::string mangleScopeOnly(const Identifier& id)
{
	std::string ret;
	for(auto s : id.scope)
		ret += std::to_string(s.length()) + s;

	return ret;
}

static inline std::string lentypestr(std::string s)
{
	return std::to_string(s.length()) + s;
}

static std::string mangleScopeName(const Identifier& id)
{
	return mangleScopeOnly(id) + lentypestr(id.name);
}

static std::string mangleType(fir::Type* t)
{
	if(t->isPrimitiveType())
	{
		return lentypestr(t->encodedStr());
	}
	else if(t->isArrayType())
	{
		return "FA" + lentypestr(mangleType(t->getArrayElementType())) + std::to_string(t->toArrayType()->getArraySize());
	}
	else if(t->isDynamicArrayType())
	{
		return (t->toDynamicArrayType()->isFunctionVariadic() ? "PP" : "DA")
			+ lentypestr(mangleType(t->getArrayElementType()));
	}
	else if(t->isArraySliceType())
	{
		return "SL" + lentypestr(mangleType(t->getArrayElementType()));
	}
	else if(t->isVoidType())
	{
		return "v";
	}
	else if(t->isFunctionType())
	{
		std::string ret = "FN" + std::to_string(t->toFunctionType()->getArgumentTypes().size()) + "FA";
		for(auto a : t->toFunctionType()->getArgumentTypes())
		{
			ret += lentypestr(mangleType(a));
		}

		if(t->toFunctionType()->getArgumentTypes().empty())
			ret += "v";

		return ret;
	}
	else if(t->isStructType())
	{
		return lentypestr(mangleScopeName(t->toStructType()->getTypeName()));
	}
	else if(t->isClassType())
	{
		return lentypestr(mangleScopeName(t->toClassType()->getTypeName()));
	}
	else if(t->isTupleType())
	{
		std::string ret = "ST" + std::to_string(t->toTupleType()->getElementCount()) + "SM";
		for(auto m : t->toTupleType()->getElements())
			ret += lentypestr(mangleType(m));

		return ret;
	}
	else if(t->isPointerType())
	{
		return "PT" + lentypestr(mangleType(t->getPointerElementType()));
	}
	else if(t->isStringType())
	{
		return "SR";
	}
	else if(t->isCharType())
	{
		return "CH";
	}
	else if(t->isEnumType())
	{
		return "EN" + lentypestr(mangleType(t->toEnumType()->getCaseType())) + lentypestr(mangleScopeName(t->toEnumType()->getTypeName()));
	}
	else if(t->isAnyType())
	{
		return "AY";
	}
	else
	{
		_error_and_exit("unsupported ir type??? ('%s')", t);
	}
}

static std::string _doMangle(const Identifier& id, bool includeScope)
{
	if(id.kind == IdKind::Name)
		return id.name;

	std::string ret = "_F";

	if(id.kind == IdKind::Function)					ret += "F";
	else if(id.kind == IdKind::Type)					ret += "T";
	else												ret += "U";

	if(includeScope)
		ret += mangleScopeOnly(id);

	ret += lentypestr(id.name);

	if(id.kind == IdKind::Function)
	{
		ret += "_FA";
		for(auto t : id.params)
			ret += "_" + mangleType(t);

		if(id.params.empty())
			ret += "v";
	}

	return ret;
}


std::string Identifier::mangled() const
{
	return _doMangle(*this, false);
}

std::string Identifier::mangledName() const
{
	return _doMangle(*this, false);
}














