// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	PointerType::PointerType(size_t inds, Type* base)
	{
		this->indirections = inds;
		this->baseType = base;
	}


	PointerType* PointerType::getInt8Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt8(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt16Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt16(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt32Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt32(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt64Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt64(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt128Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt128(tc)->getPointerTo());
	}


	PointerType* PointerType::getUint8Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint8(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint16Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint16(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint32Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint32(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint64Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint64(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint128Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint128(tc)->getPointerTo());
	}







	// various
	std::string PointerType::str()
	{
		return this->baseType->str() + "*";
	}

	std::string PointerType::encodedStr()
	{
		return this->baseType->encodedStr() + "P";
	}


	bool PointerType::isTypeEqual(Type* other)
	{
		PointerType* po = dynamic_cast<PointerType*>(other);
		if(!po) return false;
		if(this->indirections != po->indirections) return false;

		return this->baseType->isTypeEqual(po->baseType);
	}


	size_t PointerType::getIndirections()
	{
		return this->indirections;
	}





	PointerType* PointerType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// basically return a new version of ourselves
		if(!this->baseType->isParametricType())
			return this;


		ParametricType* tp = this->baseType->toParametricType();

		if(reals.find(tp->getName()) != reals.end())
		{
			auto t = reals[tp->getName()];
			if(t->isParametricType())
				_error_and_exit("Cannot reify when the supposed real type of '%s' is still parametric", tp->getName().c_str());

			return tc->normaliseType(new PointerType(this->indirections, t))->toPointerType();
		}
		else
		{
			_error_and_exit("Failed to reify, no type found for '%s'", tp->getName().c_str());
		}
	}
}


















