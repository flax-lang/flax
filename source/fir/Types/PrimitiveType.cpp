// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	PrimitiveType::PrimitiveType(size_t bits, bool issigned, Kind kind) : Type(TypeKind::Primitive)
	{
		this->bitWidth = bits;
		this->primKind = kind;
		this->isTypeSigned = issigned;
	}










	static util::hash_map<size_t, std::vector<PrimitiveType*>> primitiveTypeCache;
	PrimitiveType* PrimitiveType::getIntWithBitWidthAndSignage(size_t bits, bool issigned)
	{
		std::vector<PrimitiveType*>& types = primitiveTypeCache[bits];

		for(auto t : types)
		{
			iceAssert(t->bitWidth == bits);
			if(t->isIntegerType() && !t->isFloatingPointType() && (t->isSigned() == issigned))
				return t;
		}


		return *types.insert(types.end(), new PrimitiveType(bits, issigned, Kind::Integer));
	}

	PrimitiveType* PrimitiveType::getFloatWithBitWidth(size_t bits)
	{
		std::vector<PrimitiveType*>& types = primitiveTypeCache[bits];

		for(auto t : types)
		{
			iceAssert(t->bitWidth == bits);
			if(t->isFloatingPointType())
				return t;
		}

		return *types.insert(types.end(), new PrimitiveType(bits, false, Kind::Floating));
	}

	PrimitiveType* PrimitiveType::getIntN(size_t bits)
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(bits, true);
	}

	PrimitiveType* PrimitiveType::getUintN(size_t bits)
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(bits, false);
	}





	PrimitiveType* PrimitiveType::getInt8()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(8, true);
	}

	PrimitiveType* PrimitiveType::getInt16()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(16, true);
	}

	PrimitiveType* PrimitiveType::getInt32()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(32, true);
	}

	PrimitiveType* PrimitiveType::getInt64()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(64, true);
	}

	PrimitiveType* PrimitiveType::getInt128()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(128, true);
	}





	PrimitiveType* PrimitiveType::getUint8()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(8, false);
	}

	PrimitiveType* PrimitiveType::getUint16()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(16, false);
	}

	PrimitiveType* PrimitiveType::getUint32()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(32, false);
	}

	PrimitiveType* PrimitiveType::getUint64()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(64, false);
	}

	PrimitiveType* PrimitiveType::getUint128()
	{
		return PrimitiveType::getIntWithBitWidthAndSignage(128, false);
	}




	PrimitiveType* PrimitiveType::getFloat32()
	{
		return PrimitiveType::getFloatWithBitWidth(32);
	}

	PrimitiveType* PrimitiveType::getFloat64()
	{
		return PrimitiveType::getFloatWithBitWidth(64);
	}

	PrimitiveType* PrimitiveType::getFloat128()
	{
		return PrimitiveType::getFloatWithBitWidth(128);
	}









	// various
	std::string PrimitiveType::str()
	{
		// is primitive.
		std::string ret;

		if(this->primKind == Kind::Integer)
		{
			if(this->isSigned())    ret = "i";
			else                    ret = "u";

			ret += std::to_string(this->getIntegerBitWidth());
		}
		else if(this->primKind == Kind::Floating)
		{
			ret = "f" + std::to_string(this->getFloatingPointBitWidth());
		}
		else
		{
			iceAssert(0);
		}

		return ret;
	}

	std::string PrimitiveType::encodedStr()
	{
		return this->str();
	}


	bool PrimitiveType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Primitive)
			return false;

		auto op = other->toPrimitiveType();
		return (this->primKind == op->primKind) && (this->bitWidth == op->bitWidth) && (this->isTypeSigned == op->isTypeSigned);
	}




	bool PrimitiveType::isSigned()
	{
		iceAssert(this->primKind == Kind::Integer && "not integer type");
		return this->isTypeSigned;
	}

	size_t PrimitiveType::getIntegerBitWidth()
	{
		iceAssert(this->primKind == Kind::Integer && "not integer type");
		return this->bitWidth;
	}


	// float stuff
	size_t PrimitiveType::getFloatingPointBitWidth()
	{
		iceAssert(this->primKind == Kind::Floating && "not floating point type");
		return this->bitWidth;
	}

	PrimitiveType* PrimitiveType::getOppositeSignedType()
	{
		if(this == Type::getInt8())
		{
			return Type::getUint8();
		}
		else if(this == Type::getInt16())
		{
			return Type::getUint16();
		}
		else if(this == Type::getInt32())
		{
			return Type::getUint32();
		}
		else if(this == Type::getInt64())
		{
			return Type::getUint64();
		}
		else if(this == Type::getInt128())
		{
			return Type::getUint128();
		}
		else if(this == Type::getUint8())
		{
			return Type::getInt8();
		}
		else if(this == Type::getUint16())
		{
			return Type::getInt16();
		}
		else if(this == Type::getUint32())
		{
			return Type::getInt32();
		}
		else if(this == Type::getUint64())
		{
			return Type::getInt64();
		}
		else if(this == Type::getUint128())
		{
			return Type::getInt128();
		}
		else
		{
			return this;
		}
	}

	fir::Type* PrimitiveType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		return this;
	}
}


















