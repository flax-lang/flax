// pts.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "memorypool.h"

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
		return dcast(NamedType, this);
	}

	PointerType* Type::toPointerType()
	{
		return dcast(PointerType, this);
	}

	TupleType* Type::toTupleType()
	{
		return dcast(TupleType, this);
	}

	FixedArrayType* Type::toFixedArrayType()
	{
		return dcast(FixedArrayType, this);
	}

	DynamicArrayType* Type::toDynamicArrayType()
	{
		return dcast(DynamicArrayType, this);
	}

	VariadicArrayType* Type::toVariadicArrayType()
	{
		return dcast(VariadicArrayType, this);
	}

	FunctionType* Type::toFunctionType()
	{
		return dcast(FunctionType, this);
	}

	ArraySliceType* Type::toArraySliceType()
	{
		return dcast(ArraySliceType, this);
	}


	bool Type::isNamedType()
	{
		return dcast(NamedType, this) != 0;
	}

	bool Type::isPointerType()
	{
		return dcast(PointerType, this) != 0;
	}

	bool Type::isTupleType()
	{
		return dcast(TupleType, this) != 0;
	}

	bool Type::isFixedArrayType()
	{
		return dcast(FixedArrayType, this) != 0;
	}

	bool Type::isDynamicArrayType()
	{
		return dcast(DynamicArrayType, this) != 0;
	}

	bool Type::isVariadicArrayType()
	{
		return dcast(VariadicArrayType, this) != 0;
	}

	bool Type::isFunctionType()
	{
		return dcast(FunctionType, this) != 0;
	}

	bool Type::isArraySliceType()
	{
		return dcast(ArraySliceType, this) != 0;
	}




	std::string Type::str()
	{
		return "??";
	}


	static InferredType* it = 0;
	InferredType* InferredType::get()
	{
		if(it) return it;
		return (it = util::pool<InferredType>(Location()));
	}

	NamedType* NamedType::create(const Location& l, const std::string& s)
	{
		return NamedType::create(l, s, { });
	}

	NamedType* NamedType::create(const Location& l, const std::string& s, const PolyArgMapping_t& tm)
	{
		auto ret = util::pool<NamedType>(l, s);
		ret->genericMapping = tm;

		return ret;
	}



	// these shits are supposed to be reversibly-parse-able, so omit any "beautification" spaces and whatnot.
	std::string NamedType::str()
	{
		auto ret = this->name;

		if(!this->genericMapping.empty())
		{
			std::string m;
			for(auto p : this->genericMapping.maps)
				m += strprintf("%s: %s, ", p.name.empty() ? std::to_string(p.index) : p.name, p.type->str());

			if(this->genericMapping.maps.size() > 0)
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




