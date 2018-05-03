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

	PointerType* PointerType::getMutable()
	{
		if(this->isPtrMutable) return this;
		return this->baseType->getMutablePointerTo()->toPointerType();
	}

	PointerType* PointerType::getImmutable()
	{
		if(!this->isPtrMutable) return this;
		return this->baseType->getPointerTo()->toPointerType();
	}


	bool PointerType::isMutable()
	{
		return this->isPtrMutable;
	}



	PointerType* PointerType::getInt8Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getInt8()->getPointerTo());
	}

	PointerType* PointerType::getInt16Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getInt16()->getPointerTo());
	}

	PointerType* PointerType::getInt32Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getInt32()->getPointerTo());
	}

	PointerType* PointerType::getInt64Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getInt64()->getPointerTo());
	}

	PointerType* PointerType::getInt128Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getInt128()->getPointerTo());
	}


	PointerType* PointerType::getUint8Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getUint8()->getPointerTo());
	}

	PointerType* PointerType::getUint16Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getUint16()->getPointerTo());
	}

	PointerType* PointerType::getUint32Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getUint32()->getPointerTo());
	}

	PointerType* PointerType::getUint64Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getUint64()->getPointerTo());
	}

	PointerType* PointerType::getUint128Ptr()
	{
		return dynamic_cast<PointerType*>(Type::getUint128()->getPointerTo());
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


















