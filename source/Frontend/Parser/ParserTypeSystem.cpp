// ParserTypeSystem.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "ir/type.h"

namespace Codegen
{
	std::string unwrapPointerType(std::string type, int* _indirections)
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
}

namespace pts
{

	static pts::Type* recursivelyParseTuple(std::string str, int* used)
	{
		iceAssert(str.length() > 0);
		iceAssert(str[0] == '(');

		str = str.substr(1);
		char front = str.front();
		if(front == ')')
			return new pts::TupleType({ });

		std::deque<pts::Type*> types;
		while(front != ')')
		{
			std::string cur;
			while(front != ',' && front != '(' && front != ')')
			{
				cur += front;
				(*used)++;

				str.erase(str.begin());
				front = str.front();
			}

			if(front == ',' || front == ')')
			{
				bool shouldBreak = (front == ')');
				pts::Type* ty = pts::parseType(cur);
				iceAssert(ty);

				types.push_back(ty);

				str.erase(str.begin());
				front = str.front();

				(*used)++;

				if(shouldBreak)
					break;
			}
			else if(front == '(')
			{
				iceAssert(str.front() == '(');
				types.push_back(recursivelyParseTuple(str, used));

				if(str.front() == ',')
				{
					str.erase(str.begin());
					(*used)++;
				}

				front = str.front();
			}
		}

		(*used)++;
		return new pts::TupleType(types);
	}



	static pts::Type* parseTypeUsingBase(pts::Type* base, std::string type)
	{
		if(type.length() > 0)
		{
			if(type[0] == '(')
			{
				// parse a tuple.
				int used = 0;
				pts::Type* parsed = recursivelyParseTuple(type, &used);

				type = type.substr(used);
				return parseTypeUsingBase(parsed, type);
			}
			else
			{
				pts::Type* ret = 0;

				int indirections = 0;
				std::string actualType = Codegen::unwrapPointerType(type, &indirections);

				if(actualType.find("[") != std::string::npos)
				{
					size_t k = actualType.find("[");
					std::string bstr = actualType.substr(0, k);

					std::string arr = actualType.substr(k);
					if(base == 0)
					{
						base = parseType(bstr);
					}
					else if(bstr.size() > 0)
					{
						// typically only *
						iceAssert(bstr[0] == '*');
						while(bstr.front() == '*')
							base = new pts::PointerType(base), bstr = bstr.substr(1);
					}

					std::vector<int> sizes;
					while(arr.length() > 0 && arr.front() == '[')
					{
						arr = arr.substr(1);

						// get the size
						const char* c = arr.c_str();
						char* final = 0;


						if(arr.find("]") == 0)
						{
							// variable array.
							// int x[];
							sizes.push_back(0);
							iceAssert(arr.find("]") == 1);

							arr = arr.substr(1);
						}
						else if(arr.find("...") == 0)
						{
							sizes.push_back(-1);
							iceAssert(arr.find("]") == 3);

							arr = arr.substr(4);

							// variadic must be last
							if(arr.length() > 0 && arr.front() == '[')
								iceAssert(0 && "variadic array must be last array");
						}
						else
						{
							size_t asize = strtoll(c, &final, 0);
							size_t numlen = final - c;

							arr = arr.substr(numlen);
							sizes.push_back(asize);

							// get the closing.
							iceAssert(arr.length() > 0 && arr.front() == ']');
							arr = arr.substr(1);
						}
					}

					for(auto i : sizes)
					{
						if(i > 0)
						{
							base = new pts::FixedArrayType(base, i);
						}
						else if(i == -1)
						{
							base = new pts::VariadicArrayType(base);
						}
						else
						{
							base = new pts::PointerType(base);
						}
					}


					// check if we have more
					if(arr.size() > 0)
					{
						// fprintf(stderr, "base: %s : %s : %s\n", arr.c_str(), type.c_str(), actualType.c_str());
						base = parseTypeUsingBase(base, arr);
					}

					ret = base;
				}
				else
				{
					ret = pts::NamedType::create(actualType);
				}


				while(indirections > 0)
				{
					ret = new pts::PointerType(ret);
					indirections--;
				}

				return ret;
			}
		}
		else if(base)
		{
			return base;
		}
		else
		{
			iceAssert(0);
		}
	}



	pts::Type* parseType(std::string type)
	{
		return parseTypeUsingBase(0, type);
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






	std::string Type::str()
	{
		iceAssert(resolvedFType);
		return resolvedFType->str();
	}


	static InferredType* it = 0;
	InferredType* InferredType::get()
	{
		if(it) return it;

		return (it = new InferredType());
	}


	std::string NamedType::str()
	{
		return this->name;
	}


	static std::unordered_map<std::string, NamedType*> map;
	NamedType* NamedType::create(std::string s)
	{
		if(map.find(s) != map.end())
			return map[s];

		map[s] = new NamedType(s);
		return map[s];
	}

	std::string PointerType::str()
	{
		return this->base->str() + "*";
	}

	std::string TupleType::str()
	{
		std::string ret = "(";
		for(auto t : this->types)
			ret += t->str() + ", ";

		if(ret.size() > 1)
		{
			ret.pop_back();
			ret.pop_back();
		}

		return ret + ")";
	}

	std::string FixedArrayType::str()
	{
		return this->base->str() + "[" + std::to_string(this->size) + "]";
	}

	std::string DynamicArrayType::str()
	{
		return this->base->str() + "[?]";
	}

	std::string VariadicArrayType::str()
	{
		return this->base->str() + "[...]";
	}

	std::string FunctionType::str()
	{
		std::string ret = "[(";
		for(auto t : this->argTypes)
			ret += t->str() + ", ";

		if(ret.size() > 2)
		{
			ret.pop_back();
			ret.pop_back();
		}

		return ret + ") -> " + this->returnType->str() + "]";
	}
}





