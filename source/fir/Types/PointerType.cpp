// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	PointerType::PointerType(Type* base, bool mut)
	{
		this->baseType = base;
		this->isPtrMutable = mut;
	}

	PointerType* PointerType::getMutable(FTContext* tc)
	{
		if(this->isPtrMutable) return this;
		return this->baseType->getMutablePointerTo()->toPointerType();
	}

	PointerType* PointerType::getImmutable(FTContext* tc)
	{
		if(!this->isPtrMutable) return this;
		return this->baseType->getPointerTo()->toPointerType();
	}


	bool PointerType::isMutable()
	{
		return this->isPtrMutable;
	}



	PointerType* PointerType::getInt8Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt8(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getInt16Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt16(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getInt32Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt32(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getInt64Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt64(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getInt128Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getInt128(tc)->getPointerTo(tc));
	}


	PointerType* PointerType::getUint8Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint8(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getUint16Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint16(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getUint32Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint32(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getUint64Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint64(tc)->getPointerTo(tc));
	}

	PointerType* PointerType::getUint128Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(Type::getUint128(tc)->getPointerTo(tc));
	}







	// various
	std::string PointerType::str()
	{
		return (this->isPtrMutable ? "&mut " : "&") + this->baseType->str();
	}

	std::string PointerType::encodedStr()
	{
		return this->baseType->encodedStr() + (this->isPtrMutable ? "MP" : "P");
	}


	bool PointerType::isTypeEqual(Type* other)
	{
		PointerType* po = dynamic_cast<PointerType*>(other);
		if(!po) return false;

		return this->baseType->isTypeEqual(po->baseType) && (this->isPtrMutable == po->isPtrMutable);
	}
}


















