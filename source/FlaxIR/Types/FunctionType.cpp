// FunctionType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace Codegen
{
	std::string unwrapPointerType(std::string, int*);
}

namespace fir
{
	FunctionType::FunctionType(std::deque<Type*> args, Type* ret, bool isvariadic, bool iscva) : Type(FTypeKind::Function)
	{
		this->functionParams = args;
		this->functionRetType = ret;
		this->isFnVariadic = isvariadic;

		this->isFnCStyleVarArg = iscva;
	}






	// functions
	FunctionType* FunctionType::getCVariadicFunc(std::deque<Type*> args, Type* ret, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, false, true);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::getCVariadicFunc(std::vector<Type*> args, Type* ret, FTContext* tc)
	{
		std::deque<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return getCVariadicFunc(dargs, ret, tc);
	}

	FunctionType* FunctionType::getCVariadicFunc(std::initializer_list<Type*> args, Type* ret, FTContext* tc)
	{
		std::deque<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return getCVariadicFunc(dargs, ret, tc);
	}






	FunctionType* FunctionType::get(std::deque<Type*> args, Type* ret, bool isVarArg, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, isVarArg, false);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::get(std::vector<Type*> args, Type* ret, bool isVarArg, FTContext* tc)
	{
		std::deque<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return get(dargs, ret, isVarArg, tc);
	}

	FunctionType* FunctionType::get(std::initializer_list<Type*> args, Type* ret, bool isVarArg, FTContext* tc)
	{
		std::deque<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return get(dargs, ret, isVarArg, tc);
	}




	// various
	std::string FunctionType::str()
	{
		std::string ret;
		for(auto p : this->functionParams)
			ret += p->str() + ", ";

		if(ret.length() > 0)
			ret = ret.substr(0, ret.length() - 2); // remove extra comma

		ret = "(" + ret + ") -> " + this->functionRetType->str();

		return ret;
	}

	std::string FunctionType::encodedStr()
	{
		return this->str();
	}





	// function stuff
	std::deque<Type*> FunctionType::getArgumentTypes()
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
		return this->isFnVariadic;
	}


	bool FunctionType::isTypeEqual(Type* other)
	{
		FunctionType* of = dynamic_cast<FunctionType*>(other);
		if(!of) return false;
		if(this->isFnCStyleVarArg != of->isFnCStyleVarArg) return false;
		if(this->isFnVariadic != of->isFnVariadic) return false;
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


















