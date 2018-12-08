// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	PointerType::PointerType(Type* base, bool mut) : Type(TypeKind::Pointer)
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
		return Type::getInt8()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getInt16Ptr()
	{
		return Type::getInt16()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getInt32Ptr()
	{
		return Type::getInt32()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getInt64Ptr()
	{
		return Type::getInt64()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getInt128Ptr()
	{
		return Type::getInt128()->getPointerTo()->toPointerType();
	}


	PointerType* PointerType::getUint8Ptr()
	{
		return Type::getUint8()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getUint16Ptr()
	{
		return Type::getUint16()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getUint32Ptr()
	{
		return Type::getUint32()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getUint64Ptr()
	{
		return Type::getUint64()->getPointerTo()->toPointerType();
	}

	PointerType* PointerType::getUint128Ptr()
	{
		return Type::getUint128()->getPointerTo()->toPointerType();
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
		if(other->kind != TypeKind::Pointer)
			return false;

		auto op = other->toPointerType();
		return this->baseType->isTypeEqual(op->baseType) && (this->isPtrMutable == op->isPtrMutable);
	}

	fir::Type* PointerType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		auto base = this->baseType->substitutePlaceholders(subst);
		if(this->isMutable())   return base->getMutablePointerTo();
		else                    return base->getPointerTo();
	}
}


















