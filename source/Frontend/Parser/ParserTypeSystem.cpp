// ParserTypeSystem.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
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
	static std::pair<Type*, TypeTransformer> getTransformer(Type* t)
	{
		using TrfType = TypeTransformer::Type;
		if(t->isNamedType() || t->isTupleType() || t->isFunctionType())
		{
			return { t, TypeTransformer(TrfType::None, 0) };
		}
		else if(t->isPointerType())
		{
			return { t->toPointerType()->base, TypeTransformer(TrfType::Pointer, 0) };
		}
		else if(t->isVariadicArrayType())
		{
			return { t->toVariadicArrayType()->base, TypeTransformer(TrfType::VariadicArray, 0) };
		}
		else if(t->isDynamicArrayType())
		{
			return { t->toDynamicArrayType()->base, TypeTransformer(TrfType::DynamicArray, 0) };
		}
		else if(t->isFixedArrayType())
		{
			return { t->toFixedArrayType()->base, TypeTransformer(TrfType::FixedArray, t->toFixedArrayType()->size) };
		}
		else
		{
			_error_and_exit("????");
		}
	}


	static std::pair<fir::Type*, TypeTransformer> getTransformer(fir::Type* t)
	{
		using TrfType = TypeTransformer::Type;
		if(t->isPointerType())
		{
			return { t->getPointerElementType(), TypeTransformer(TrfType::Pointer, 0) };
		}
		else if(t->isDynamicArrayType())
		{
			return { t->toDynamicArrayType()->getElementType(), TypeTransformer(TrfType::DynamicArray, 0) };
		}
		else if(t->isArrayType())
		{
			auto k = t->toArrayType();
			return { k->getElementType(), TypeTransformer(TrfType::FixedArray, k->getArraySize()) };
		}
		else
		{
			// parametric types, functions, tuples, named types
			return { t, TypeTransformer(TrfType::None, 0) };
		}
	}

	std::pair<Type*, std::vector<TypeTransformer>> decomposeTypeIntoBaseTypeWithTransformations(Type* type)
	{
		// great.
		iceAssert(type);


		std::vector<TypeTransformer> trfs;
		while(!type->isNamedType())
		{
			TypeTransformer trf(TypeTransformer::Type::None, 0);
			Type* t = 0;
			std::tie(t, trf) = getTransformer(type);

			// we've reached the bottom.
			if(t == type)
			{
				iceAssert(trf.type == TypeTransformer::Type::None);
				return { type, trfs };
			}

			type = t;
			trfs.insert(trfs.begin(), trf);
		}

		return { type, trfs };
	}


	std::pair<fir::Type*, std::vector<TypeTransformer>> decomposeFIRTypeIntoBaseTypeWithTransformations(fir::Type* type)
	{
		// great.
		iceAssert(type);

		std::vector<TypeTransformer> trfs;
		while(true)
		{
			TypeTransformer trf(TypeTransformer::Type::None, 0);
			fir::Type* t = 0;
			std::tie(t, trf) = getTransformer(type);

			// we've reached the bottom.
			if(t == type)
			{
				iceAssert(trf.type == TypeTransformer::Type::None);
				return { type, trfs };
			}

			type = t;
			trfs.insert(trfs.begin(), trf);
		}

		return { type, trfs };
	}





	fir::Type* applyTransformationsOnType(fir::Type* base, std::vector<TypeTransformer> trfs)
	{
		using TrfType = TypeTransformer::Type;
		for(auto trf : trfs)
		{
			switch(trf.type)
			{
				case TrfType::None:
					break;

				case TrfType::Pointer:
					base = base->getPointerTo();
					break;

				case TrfType::DynamicArray:
					base = fir::DynamicArrayType::get(base);
					break;

				case TrfType::VariadicArray:
					base = fir::DynamicArrayType::getVariadic(base);
					break;

				case TrfType::FixedArray:
					base = fir::ArrayType::get(base, trf.data);
					break;

				default:
					iceAssert(0);
			}
		}

		return base;
	}




	// in this case, ats is the master, bts is the one that has to conform
	bool areTransformationsCompatible(std::vector<TypeTransformer> ats, std::vector<TypeTransformer> bts)
	{
		// we must have at least as many transformations as the master
		if(bts.size() < ats.size()) return false;

		// make some fake types
		{
			fir::Type* a = fir::ParametricType::get("_X_");
			fir::Type* b = fir::ParametricType::get("_X_");

			// please?
			if(applyTransformationsOnType(a, ats) == applyTransformationsOnType(b, bts))
				return true;
		}

		// here's how this works:
		// we start from the back.
		// as long as the transformations match, we continue
		// if ats is T*[]*[]*** and bts is T********
		// we'd end up with ats = T*[]*[] and bts = T*****
		// the next transformtion doesn't match, so we quit.

		// if we have ats = T*[]*[]*** and bts = T***[]*[]***
		// we'd have a = T*[]*[] and b = T***[]*[]
		// then we keep reducing, ending up with
		// a = T and b = T**
		// this works, so we return true.

		// not so easy.
		// note that we stop at ats.size, because A must be reduced all the way to a base type.
		for(size_t i = 0; i < ats.size(); i++)
		{
			TypeTransformer at = ats[ats.size() - i - 1];
			TypeTransformer bt = bts[bts.size() - i - 1];

			if(at != bt)
				return false;
		}

		return true;
	}


	// what the fuck does this function DO?
	// it appears to take an input type that is fully transformed, and use the transformations in 'ats'
	// to reverse the type, into a base type.
	fir::Type* reduceMaximallyWithSubset(fir::Type* type, std::vector<TypeTransformer> ats, std::vector<TypeTransformer> bts)
	{
		using TrfType = TypeTransformer::Type;
		iceAssert(areTransformationsCompatible(ats, bts));

		// note that we stop at ats.size, because A must be reduced all the way to a base type.
		for(size_t i = 0; i < ats.size(); i++)
		{
			TypeTransformer at = ats[ats.size() - i - 1];
			TypeTransformer bt = bts[bts.size() - i - 1];


			// a must be reduced all the way
			// so we must match
			iceAssert(at == bt);


			switch(at.type)
			{
				case TrfType::None:
					break;

				case TrfType::Pointer:
					iceAssert(type->isPointerType());
					type = type->getPointerElementType();
					break;

				case TrfType::DynamicArray:
					iceAssert(type->isDynamicArrayType());
					type = type->toDynamicArrayType()->getElementType();
					break;

				case TrfType::VariadicArray:
					iceAssert(type->isDynamicArrayType() && type->toDynamicArrayType()->isFunctionVariadic());
					type = type->toDynamicArrayType()->getElementType();
					break;

				case TrfType::FixedArray:
					iceAssert(type->isArrayType());
					type = type->toArrayType()->getElementType();
					break;

				default:
					iceAssert(0);
			}
		}

		return type;
	}















	static pts::Type* recursivelyParseTuple(std::string str, int* used)
	{
		iceAssert(str.length() > 0);
		iceAssert(str[0] == '(');


		size_t origLength = str.length();


		str = str.substr(1);
		char front = str.front();
		if(front == ')')
			return new pts::TupleType({ });

		std::vector<pts::Type*> types;

		for(size_t i = 0, nest = 0; i < str.size();)
		{
			if(str[i] == '(')
			{
				nest++;
			}
			else if(str[i] == ',' && nest == 0)
			{
				types.push_back(parseType(str.substr(0, i)));
				str = str.substr(i + 1);
				i = 0;

				// skip the increment.
				continue;
			}
			else if(str[i] == ')')
			{
				if(nest == 0)
				{
					types.push_back(parseType(str.substr(0, i)));
					str = str.substr(i);

					break;
				}
				else
				{
					nest--;
				}
			}

			 i++;
		}

		iceAssert(str.front() == ')');
		str.substr(1);

		*used = origLength - str.length();


		(*used)++;
		return new pts::TupleType(types);
	}



	static pts::Type* parseTypeUsingBase(pts::Type* base, std::string type)
	{
		if(type.length() > 0)
		{
			if(type[0] == '*')
			{
				iceAssert(base);
				size_t i = 0;
				for(; i < type.size() && type[i] == '*'; i++)
					base = new pts::PointerType(base);

				type = type.substr(i);
				return parseTypeUsingBase(base, type);
			}
			else if(type[0] == '(')
			{
				// parse a tuple.
				int used = 0;
				pts::Type* parsed = recursivelyParseTuple(type, &used);

				type = type.substr(used);
				return parseTypeUsingBase(parsed, type);
			}
			else if(type[0] == '{')
			{
				// see if we have generic parts
				std::map<std::string, TypeConstraints_t> genericTypes;
				if(type[1] == '<')
				{
					size_t i = 2;
					for(; i < type.size(); i++)
					{
						std::string name;
						while(i < type.size() && type[i] != ':' && type[i] != '>' && type[i] != '&' && type[i] != ',')
							name += type[i], i++;

						// constraints
						std::vector<std::string> prots;
						if(type[i] == ':')
						{
							i++;

							prots.push_back("");

							again:
							while(type[i] != '&' && type[i] != ',' && type[i] != '>')
								prots.back() += type[i], i++;

							// todo(goto): lol
							// mostly because i'm lazy to structure it nicely
							// and it's completely clear and easy to understand what's going on here.

							if(type[i] == '&')
							{
								i++;
								prots.push_back("");

								goto again;
							}
						}

						genericTypes[name].protocols = prots;

						if(type[i] == '>')
							break;
					}

					iceAssert(type[i] == '>');
					i++;

					type = type.substr(i);
				}
				else
				{
					type = type.substr(1);
				}

				// ok, time for the param list.
				// note: the reason these are asserts is because the input (std::string type) comes from the parser,
				// and is generated, not user-inputted.
				// the parser should already error on malformed things.
				iceAssert(type[0] == '(');
				type = type.substr(1);


				std::vector<pts::Type*> argTypes;
				if(type[0] != ')')
				{
					for(size_t i = 0, nest = 0; i < type.size();)
					{
						// we need to isolate the string of the next type.
						// if we encounter a '(', then nest; if a ')', un-nest.
						// if nest == 0 and we get a ',' or a ')', former means parse and continue, latter means break and continue.


						if(type[i] == '(')
						{
							nest++;
						}
						else if(type[i] == ',' && nest == 0)
						{
							argTypes.push_back(parseType(type.substr(0, i)));
							type = type.substr(i + 1);
							i = 0;

							continue;
						}
						else if(type[i] == ')')
						{
							if(nest == 0)
							{
								argTypes.push_back(parseType(type.substr(0, i)));
								type = type.substr(i);

								break;
							}
							else
							{
								nest--;
							}
						}

						i++;
					}
				}

				iceAssert(type.compare(0, 3, ")->") == 0);
				type = type.substr(3);

				// parse the type next. the rearmost '}' should be ours.
				// (note: the function can return a function, we need to handle that, hence this.)

				size_t k = type.find_last_of("}");
				std::string retty = type.substr(0, k);

				pts::Type* rtype = parseType(retty);

				pts::FunctionType* ft = new pts::FunctionType(argTypes, rtype);
				ft->genericTypes = genericTypes;


				// remove the } too.
				type = type.substr(k + 1);
				return parseTypeUsingBase(ft, type);
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
							// x: int[]
							sizes.push_back(0);
							arr = arr.substr(1);
						}
						else if(arr.find(":") == 0)
						{
							// array slice
							// x: int[:]
							sizes.push_back(-2);
							arr = arr.substr(1);

							iceAssert(arr.find("]") == 0);
							arr = arr.substr(1);
						}
						else if(arr.find("...") == 0)
						{
							sizes.push_back(-1);
							iceAssert(arr.find("]") == 3);

							arr = arr.substr(4);

							// variadic must be last
							if(arr.length() > 0 && arr.front() == '[')
								iceAssert(0 && "variadic array must be last dimension");
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
						else if(i == -2)
						{
							base = new pts::ArraySliceType(base);
						}
						else if(i == -1)
						{
							base = new pts::VariadicArrayType(base);
						}
						else
						{
							base = new pts::DynamicArrayType(base);
						}
					}


					// check if we have more
					if(arr.size() > 0)
					{
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
			debuglog(">> %s\n", type.c_str());
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
		return this->base->str() + "[]";
	}

	std::string VariadicArrayType::str()
	{
		return this->base->str() + "[...]";
	}

	std::string ArraySliceType::str()
	{
		return this->base->str() + "[:]";
	}

	std::string FunctionType::str()
	{
		std::string ret = "(";

		for(auto g : this->genericTypes)
		{
			ret += g.first;
			if(g.second.protocols.size() > 0) ret += ":";

			for(auto p : g.second.protocols)
				ret += p + "&";

			if(ret.back() == '&')
				ret.pop_back();

			ret += ",";
		}

		if(ret.back() == ',')
			ret.pop_back();


		for(auto t : this->argTypes)
			ret += t->str() + ", ";

		if(ret.size() > 2)
		{
			ret.pop_back();
			ret.pop_back();
		}

		return ret + ") -> " + this->returnType->str();
	}
}





