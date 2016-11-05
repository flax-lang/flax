// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	PointerType::PointerType(Type* base)
	{
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
		return this->baseType->isTypeEqual(po->baseType);
	}





	PointerType* PointerType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return dynamic_cast<PointerType*>(this->baseType->reify(reals)->getPointerTo());
	}
}


















