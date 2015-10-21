// FunctionType.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <unordered_map>

#include "codegen.h"
#include "compiler.h"
#include "ir/type.h"

namespace Codegen
{
	std::string unwrapPointerType(std::string, int*);
}

namespace fir
{
	FunctionType::FunctionType(std::deque<Type*> args, Type* ret, bool isva) : Type(FTypeKind::Function)
	{
		this->functionParams = args;
		this->functionRetType = ret;
		this->isFnVarArg = isva;
	}

	// functions
	FunctionType* FunctionType::get(std::deque<Type*> args, Type* ret, bool isVarArg, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, isVarArg);
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

	bool FunctionType::isVarArg()
	{
		return this->isFnVarArg;
	}


	bool FunctionType::isTypeEqual(Type* other)
	{
		FunctionType* of = dynamic_cast<FunctionType*>(other);
		if(!of) return false;
		if(this->isFnVarArg != of->isFnVarArg) return false;
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


















