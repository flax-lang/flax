// FunctionType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "ir/function.h"

namespace Codegen
{
	std::string unwrapPointerType(const std::string&, int*);
}

namespace fir
{
	FunctionType::FunctionType(const std::vector<Type*>& args, Type* ret, bool iscva) : Type(TypeKind::Function)
	{
		this->functionParams = args;
		this->functionRetType = ret;

		this->isFnCStyleVarArg = iscva;
	}





	// functions
	FunctionType* FunctionType::getCVariadicFunc(const std::vector<Type*>& args, Type* ret)
	{
		return TypeCache::get().getOrAddCachedType(new FunctionType(args, ret, true));
	}

	FunctionType* FunctionType::getCVariadicFunc(const std::initializer_list<Type*>& args, Type* ret)
	{
		return FunctionType::getCVariadicFunc(std::vector<Type*>(args.begin(), args.end()), ret);
	}

	FunctionType* FunctionType::get(const std::vector<Type*>& args, Type* ret)
	{
		return TypeCache::get().getOrAddCachedType(new FunctionType(args, ret, false));
	}

	FunctionType* FunctionType::get(const std::initializer_list<Type*>& args, Type* ret)
	{
		return FunctionType::get(std::vector<Type*>(args.begin(), args.end()), ret);
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
		return this->functionParams.size() > 0 && this->functionParams.back()->isArraySliceType()
			&& this->functionParams.back()->toArraySliceType()->isVariadicType();
	}


	bool FunctionType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Function)
			return false;

		auto of = other->toFunctionType();
		auto ret = (this->isFnCStyleVarArg == of->isFnCStyleVarArg) && (this->functionRetType->isTypeEqual(of->functionRetType))
			&& (this->functionParams.size() == of->functionParams.size());

		if(ret)
		{
			for(size_t i = 0; i < this->functionParams.size(); i++)
			{
				if(!this->functionParams[i]->isTypeEqual(of->functionParams[i]))
					return false;
			}

			return true;
		}
		else
		{
			return false;
		}
	}
}





















