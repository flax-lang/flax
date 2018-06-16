// pts.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"

namespace pts
{
	std::string unwrapPointerType(const std::string& type, int* _indirections)
	{
		std::string sptr = "*";
		size_t ptrStrLength = sptr.length();

		int tmp = 0;
		if(!_indirections)
			_indirections = &tmp;

		std::string actualType = type;
		if(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
		{
			int& indirections = *_indirections;

			while(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
				actualType = actualType.substr(0, actualType.length() - ptrStrLength), indirections++;
		}

		return actualType;
	}




	NamedType* Type::toNamedType()
	{
		return dynamic_cast<NamedType*>(this);
	}

	PointerType* Type::toPointerType()
	{
		return dynamic_cast<PointerType*>(this);
	}

	TupleType* Type::toTupleType()
	{
		return dynamic_cast<TupleType*>(this);
	}

	FixedArrayType* Type::toFixedArrayType()
	{
		return dynamic_cast<FixedArrayType*>(this);
	}

	DynamicArrayType* Type::toDynamicArrayType()
	{
		return dynamic_cast<DynamicArrayType*>(this);
	}

	VariadicArrayType* Type::toVariadicArrayType()
	{
		return dynamic_cast<VariadicArrayType*>(this);
	}

	FunctionType* Type::toFunctionType()
	{
		return dynamic_cast<FunctionType*>(this);
	}

	ArraySliceType* Type::toArraySliceType()
	{
		return dynamic_cast<ArraySliceType*>(this);
	}


	bool Type::isNamedType()
	{
		return dynamic_cast<NamedType*>(this) != 0;
	}

	bool Type::isPointerType()
	{
		return dynamic_cast<PointerType*>(this) != 0;
	}

	bool Type::isTupleType()
	{
		return dynamic_cast<TupleType*>(this) != 0;
	}

	bool Type::isFixedArrayType()
	{
		return dynamic_cast<FixedArrayType*>(this) != 0;
	}

	bool Type::isDynamicArrayType()
	{
		return dynamic_cast<DynamicArrayType*>(this) != 0;
	}

	bool Type::isVariadicArrayType()
	{
		return dynamic_cast<VariadicArrayType*>(this) != 0;
	}

	bool Type::isFunctionType()
	{
		return dynamic_cast<FunctionType*>(this) != 0;
	}

	bool Type::isArraySliceType()
	{
		return dynamic_cast<ArraySliceType*>(this) != 0;
	}




	std::string Type::str()
	{
		return "??";
	}


	static InferredType* it = 0;
	InferredType* InferredType::get()
	{
		if(it) return it;

		return (it = new InferredType());
	}

	static std::map<std::pair<std::string, std::map<std::string, Type*>>, NamedType*> map;
	NamedType* NamedType::create(const std::string& s)
	{
		return NamedType::create(s, { });
	}

	NamedType* NamedType::create(const std::string& s, const std::map<std::string, Type*>& tm)
	{
		if(map.find({ s, tm }) != map.end())
			return map[{ s, tm }];

		auto ret = new NamedType(s);
		for(const auto& p : tm)
			ret->genericMapping[p.first] = p.second;

		return (map[{ s, tm }] = ret);
	}



	// these shits are supposed to be reversibly-parse-able, so omit any "beautification" spaces and whatnot.
	std::string NamedType::str()
	{
		auto ret = this->name;

		if(!this->genericMapping.empty())
		{
			std::string m;
			for(auto p : this->genericMapping)
				m += p.first + ": " + p.second->str() + ", ";

			if(this->genericMapping.size() > 0)
				m.pop_back(), m.pop_back();

			ret += "<" + m + ">";
		}

		return ret;
	}

	std::string PointerType::str()
	{
		return (this->isMutable ? "&mut " : "&") + this->base->str();
	}

	std::string TupleType::str()
	{
		std::string ret = "(";
		for(auto t : this->types)
			ret += t->str() + ", ";

		if(this->types.size() > 0)
			ret.pop_back(), ret.pop_back();

		return ret + ")";
	}

	std::string FixedArrayType::str()
	{
		std::string b = this->base->str();
		if(this->base->isFunctionType())
			b = "(" + b + ")";

		return strprintf("[%s: %d]", b, std::to_string(this->size));
	}

	std::string DynamicArrayType::str()
	{
		std::string b = this->base->str();
		if(this->base->isFunctionType())
			b = "(" + b + ")";

		return strprintf("[%s]", b);
	}

	std::string VariadicArrayType::str()
	{
		std::string b = this->base->str();
		if(this->base->isFunctionType())
			b = "(" + b + ")";

		return strprintf("[%s: ...]", b);
	}

	std::string ArraySliceType::str()
	{
		std::string b = this->base->str();
		if(this->base->isFunctionType())
			b = "(" + b + ")";

		return strprintf("[%s:]", b);
	}

	std::string FunctionType::str()
	{
		std::string ret = "fn(";

		for(auto t : this->argTypes)
			ret += t->str() + ", ";

		if(this->argTypes.size() > 0)
			ret.pop_back(), ret.pop_back();

		return ret + ") -> " + this->returnType->str();
	}
}




