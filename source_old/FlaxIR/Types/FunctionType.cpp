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
	FunctionType::FunctionType(std::vector<Type*> args, Type* ret, bool isvariadic, bool iscva)
	{
		this->functionParams = args;
		this->functionRetType = ret;
		this->isFnVariadic = isvariadic;

		this->isFnCStyleVarArg = iscva;
	}






	// functions
	FunctionType* FunctionType::getCVariadicFunc(std::vector<Type*> args, Type* ret, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, false, true);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::getCVariadicFunc(std::initializer_list<Type*> args, Type* ret, FTContext* tc)
	{
		std::vector<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return getCVariadicFunc(dargs, ret, tc);
	}






	FunctionType* FunctionType::get(std::vector<Type*> args, Type* ret, bool isVarArg, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, isVarArg, false);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::get(std::initializer_list<Type*> args, Type* ret, bool isVarArg, FTContext* tc)
	{
		std::vector<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return get(dargs, ret, isVarArg, tc);
	}




	FunctionType* FunctionType::getWithTypeParameters(std::vector<Type*> args, Type* ret, bool isVarArg, std::vector<ParametricType*> tparams, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		FunctionType* type = new FunctionType(args, ret, isVarArg, false);
		type->addTypeParameters(tparams);
		return dynamic_cast<FunctionType*>(tc->normaliseType(type));
	}

	FunctionType* FunctionType::getWithTypeParameters(std::initializer_list<Type*> args, Type* ret, bool isVarArg, std::vector<ParametricType*> tparams, FTContext* tc)
	{
		std::vector<Type*> dargs;
		for(auto a : args)
			dargs.push_back(a);

		return getWithTypeParameters(dargs, ret, isVarArg, tparams, tc);
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
		if(this->typeParameters != of->typeParameters) return false;

		for(size_t i = 0; i < this->functionParams.size(); i++)
		{
			if(!this->functionParams[i]->isTypeEqual(of->functionParams[i]))
				return false;
		}

		return true;
	}

	bool FunctionType::isGenericFunction()
	{
		return this->typeParameters.size() > 0;
	}






	std::vector<ParametricType*> FunctionType::getTypeParameters()
	{
		return this->typeParameters;
	}

	void FunctionType::addTypeParameter(ParametricType* t)
	{
		for(auto p : this->typeParameters)
		{
			if(p->getName() == t->getName())
				error("Type parameter '%s' already exists", p->getName().c_str());
		}

		this->typeParameters.push_back(t);
	}

	void FunctionType::addTypeParameters(std::vector<ParametricType*> ts)
	{
		for(auto t : ts)
			this->addTypeParameter(t);
	}






	FunctionType* FunctionType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->isCStyleVarArg())
			_error_and_exit("cannot reify (in fact, should not be parametric) C FFI function");

		std::vector<Type*> reified;
		Type* reifiedReturn = 0;

		for(auto mem : this->functionParams)
		{
			fir::Type* rfd = mem->reify(reals);
			reified.push_back(rfd);
		}

		auto rfd = this->functionRetType->reify(reals);
		if(rfd->isParametricType())
		{
			_error_and_exit("Failed to reify type '%s', no type found for '%s'", this->str().c_str(),
				this->functionRetType->toParametricType()->getName().c_str());
		}

		reifiedReturn = rfd;

		return FunctionType::get(reified, reifiedReturn, this->isVariadicFunc());
	}
}


















