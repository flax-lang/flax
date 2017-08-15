// FunctionType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "ir/function.h"

namespace Codegen
{
	std::string unwrapPointerType(std::string, int*);
}

namespace fir
{
	FunctionType::FunctionType(std::vector<Type*> args, Type* ret, bool iscva)
	{
		this->functionParams = args;
		this->functionRetType = ret;

		this->isFnCStyleVarArg = iscva;
	}






	// functions
	FunctionType* FunctionType::getCVariadicFunc(std::vector<Type*> args, Type* ret, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, true);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::getCVariadicFunc(std::initializer_list<Type*> args, Type* ret, FTContext* tc)
	{
		std::vector<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return getCVariadicFunc(dargs, ret, tc);
	}






	FunctionType* FunctionType::get(std::vector<Type*> args, Type* ret, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, false);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::get(std::initializer_list<Type*> args, Type* ret, FTContext* tc)
	{
		std::vector<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return get(dargs, ret, tc);
	}







	// various
	std::string FunctionType::str()
	{
		std::string ret;
		for(auto p : this->functionParams)
			ret += p->str() + ", ";

		if(ret.length() > 0)
			ret = ret.substr(0, ret.length() - 2); // remove extra comma

		iceAssert(this->functionRetType);
		return "(" + ret + ") -> " + this->functionRetType->str();
	}

	std::string FunctionType::encodedStr()
	{
		return this->str();
	}





	// function stuff
	std::vector<Type*> FunctionType::getArgumentTypes()
	{
		return this->functionParams;
	}

	Type* FunctionType::getArgumentN(size_t n)
	{
		return this->functionParams[n];
	}

	Type* FunctionType::getReturnType()
	{
		return this->functionRetType;
	}

	bool FunctionType::isCStyleVarArg()
	{
		return this->isFnCStyleVarArg;
	}

	bool FunctionType::isVariadicFunc()
	{
		return this->functionParams.size() > 0 && this->functionParams.back()->isDynamicArrayType()
			&& this->functionParams.back()->toDynamicArrayType()->isFunctionVariadic();
	}


	bool FunctionType::isTypeEqual(Type* other)
	{
		FunctionType* of = dynamic_cast<FunctionType*>(other);
		if(!of) return false;
		if(this->isFnCStyleVarArg != of->isFnCStyleVarArg) return false;
		if(this->functionParams.size() != of->functionParams.size()) return false;
		if(!this->functionRetType->isTypeEqual(of->functionRetType)) return false;

		for(size_t i = 0; i < this->functionParams.size(); i++)
		{
			if(!this->functionParams[i]->isTypeEqual(of->functionParams[i]))
				return false;
		}

		return true;
	}
}





















