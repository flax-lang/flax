// Type.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <unordered_map>

#include "../include/codegen.h"
#include "../include/compiler.h"
#include "../include/ir/type.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"

namespace fir
{
	PointerType::PointerType(size_t inds, Type* base) : Type(FTypeKind::Pointer)
	{
		this->indirections = inds;
		this->baseType = base;
	}

	PointerType* PointerType::getInt8Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getInt8(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt16Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getInt16(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt32Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getInt32(tc)->getPointerTo());
	}

	PointerType* PointerType::getInt64Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getInt64(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint8Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getUint8(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint16Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getUint16(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint32Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getUint32(tc)->getPointerTo());
	}

	PointerType* PointerType::getUint64Ptr(FTContext* tc)
	{
		return dynamic_cast<PointerType*>(PrimitiveType::getUint64(tc)->getPointerTo());
	}



	// various
	std::string PointerType::str()
	{
		return this->baseType->str() + " *";
	}



	bool PointerType::isTypeEqual(Type* other)
	{
		PointerType* po = dynamic_cast<PointerType*>(other);
		if(!po) return false;
		if(this->indirections != po->indirections) return false;

		return this->baseType->isTypeEqual(po->baseType);
	}

	fir::Type* PointerType::getLlvmType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->llvmType == 0)
		{
			this->llvmType = this->baseType->getLlvmType()->getPointerTo();
		}

		return this->llvmType;
	}

	size_t PointerType::getIndirections()
	{
		return this->indirections;
	}
}


















