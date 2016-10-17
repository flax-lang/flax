// Identifier.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

std::string Identifier::str() const
{
	std::string ret;

	for(auto s : scope)
		ret += s + ".";

	ret += name;

	if(this->kind == IdKind::Function || this->kind == IdKind::Method || this->kind == IdKind::Getter || this->kind == IdKind::Setter
		|| this->kind == IdKind::AutoGenFunc || this->kind == IdKind::Operator)
	{
		for(auto t : this->functionArguments)
			ret += "_" + t->str();
	}

	return ret;
}

static std::string mangleScopeOnly(Identifier id)
{
	std::string ret;
	for(auto s : id.scope)
		ret += std::to_string(s.length()) + s;

	ret += std::to_string(id.name.length()) + id.name;
	return ret;
}

static std::string mangleType(fir::Type* t)
{
	if(t->isPrimitiveType())
	{
		return t->encodedStr();
	}
	else if(t->isArrayType())
	{
		return "AR" + std::to_string(t->toArrayType()->getArraySize()) + mangleType(t->toArrayType()->getElementType());
	}
	else if(t->isLLVariableArrayType())
	{
		return "VA" + mangleType(t->toLLVariableArrayType()->getElementType());
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
			auto mt = mangleType(a);
			ret += std::to_string(mt.length()) + mt;
		}

		if(t->toFunctionType()->getArgumentTypes().empty())
			ret += "v";

		return ret;
	}
	else if(t->isStructType())
	{
		return mangleScopeOnly(t->toStructType()->getStructName());
	}
	else if(t->isClassType())
	{
		return mangleScopeOnly(t->toClassType()->getClassName());
	}
	else if(t->isTupleType())
	{
		std::string ret = "ST" + std::to_string(t->toStructType()->getElementCount()) + "SM";
		for(auto m : t->toStructType()->getElements())
		{
			auto mt = mangleType(m);
			ret += std::to_string(mt.length()) + mt;
		}

		return ret;
	}
	else if(t->isPointerType())
	{
		return "PT" + mangleType(t->getPointerElementType());
	}
	else if(t->isStringType())
	{
		return "SR";
	}
	else
	{
		iceAssert(0 && "unsupported ir type???");
	}
}

std::string Identifier::mangled() const
{
	if(this->kind == IdKind::Name)
		return this->name;

	std::string ret = "_F";

	if(this->kind == IdKind::Function)					ret += "F";
	else if(this->kind == IdKind::Method)				ret += "M";
	else if(this->kind == IdKind::Getter)				ret += "G";
	else if(this->kind == IdKind::Setter)				ret += "S";
	else if(this->kind == IdKind::Operator)				ret += "O";
	else if(this->kind == IdKind::Struct)				ret += "C";
	else if(this->kind == IdKind::AutoGenFunc)			ret += "B";
	else if(this->kind == IdKind::ModuleConstructor)	ret += "Y";
	else if(this->kind == IdKind::Variable)				ret += "V";
	else												ret += "U";

	ret += mangleScopeOnly(*this);

	if(this->kind == IdKind::Function || this->kind == IdKind::Method || this->kind == IdKind::Getter || this->kind == IdKind::Setter
		|| this->kind == IdKind::AutoGenFunc || this->kind == IdKind::Operator)
	{
		ret += "_FA";
		for(auto t : this->functionArguments)
		{
			ret += "_" + mangleType(t);
		}

		if(this->functionArguments.empty())
			ret += "v";
	}

	return ret;
}

bool Identifier::operator == (const Identifier& other) const
{
	return this->name == other.name
		&& this->scope == other.scope
		&& this->functionArguments == other.functionArguments;
}
















