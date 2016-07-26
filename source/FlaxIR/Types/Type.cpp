// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace Codegen
{
	std::string unwrapPointerType(std::string, int*);
}

namespace fir
{
	// NOTE: some global state.
	// structs

	Type* FTContext::normaliseType(Type* type)
	{
		if(Type::areTypesEqual(type, this->voidType))
			return this->voidType;

		size_t ind = 0;
		if(type->isPointerType())
			ind = type->toPointerType()->getIndirections();

		std::vector<Type*>& list = this->typeCache[ind];

		// find in the list.
		// todo: make this more efficient
		for(auto t : list)
		{
			if(Type::areTypesEqual(t, type))
				return t;
		}

		list.push_back(type);
		return type;
	}


	static FTContext* defaultFTContext = 0;
	void setDefaultFTContext(FTContext* tc)
	{
		iceAssert(tc && "Tried to set null type context as default");
		defaultFTContext = tc;
	}

	FTContext* getDefaultFTContext()
	{
		if(defaultFTContext == 0)
		{
			// iceAssert(0);
			defaultFTContext = createFTContext();
		}

		return defaultFTContext;
	}

	FTContext* createFTContext()
	{
		FTContext* tc = new FTContext();

		// tc->llvmContext = &fir::getGlobalContext();

		// fill in primitives

		// void.
		tc->voidType = new PrimitiveType(0, FTypeKind::Void);
		tc->voidType->isTypeVoid = true;


		// bool
		{
			PrimitiveType* t = new PrimitiveType(1, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[1].push_back(t);
			tc->typeCache[0].push_back(t);
		}




		// int8
		{
			PrimitiveType* t = new PrimitiveType(8, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// int16
		{
			PrimitiveType* t = new PrimitiveType(16, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// int32
		{
			PrimitiveType* t = new PrimitiveType(32, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// int64
		{
			PrimitiveType* t = new PrimitiveType(64, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].push_back(t);
		}




		// uint8
		{
			PrimitiveType* t = new PrimitiveType(8, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// uint16
		{
			PrimitiveType* t = new PrimitiveType(16, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// uint32
		{
			PrimitiveType* t = new PrimitiveType(32, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// uint64
		{
			PrimitiveType* t = new PrimitiveType(64, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].push_back(t);
		}


		// float32
		{
			PrimitiveType* t = new PrimitiveType(32, FTypeKind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// float64
		{
			PrimitiveType* t = new PrimitiveType(64, FTypeKind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].push_back(t);
		}

		return tc;
	}






	std::string Type::typeListToString(std::initializer_list<Type*> types)
	{
		return typeListToString(std::vector<Type*>(types.begin(), types.end()));
	}

	std::string Type::typeListToString(std::deque<Type*> types)
	{
		return typeListToString(std::vector<Type*>(types.begin(), types.end()));
	}

	std::string Type::typeListToString(std::vector<Type*> types)
	{
		// print types
		std::string str = "{ ";
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + " }";
	}


	bool Type::areTypeListsEqual(std::vector<Type*> a, std::vector<Type*> b)
	{
		if(a.size() != b.size()) return false;
		if(a.size() == 0 && b.size() == 0) return true;

		for(size_t i = 0; i < a.size(); i++)
		{
			if(!Type::areTypesEqual(a[i], b[i]))
				return false;
		}

		return true;
	}

	bool Type::areTypeListsEqual(std::deque<Type*> a, std::deque<Type*> b)
	{
		return areTypeListsEqual(std::vector<Type*>(a.begin(), a.end()), std::vector<Type*>(b.begin(), b.end()));
	}

	bool Type::areTypeListsEqual(std::initializer_list<Type*> a, std::initializer_list<Type*> b)
	{
		return areTypeListsEqual(std::vector<Type*>(a.begin(), a.end()), std::vector<Type*>(b.begin(), b.end()));
	}



	Type* Type::getPointerTo(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		size_t inds = 0;
		if(this->toPointerType())
			inds = this->toPointerType()->indirections;

		PointerType* newType = new PointerType(inds + 1, this);

		// get or create.
		newType = dynamic_cast<PointerType*>(tc->normaliseType(newType));
		iceAssert(newType);
		return newType;
	}

	Type* Type::getPointerElementType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(!this->isPointerType())
			error("type is not a pointer (%s)", this->str().c_str());


		PointerType* ptrthis = dynamic_cast<PointerType*>(this);
		iceAssert(ptrthis);

		Type* newType = ptrthis->baseType;
		newType = tc->normaliseType(newType);

		// iceAssert(newType->indirections == this->indirections - 1);
		// printf("POINTER ELM: %p\n", newType);
		return newType;
	}

	bool Type::areTypesEqual(Type* a, Type* b)
	{
		if(a->typeKind != b->typeKind) return false;

		return a->isTypeEqual(b);
	}



	Type* Type::getIndirectedType(ssize_t times, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		Type* ret = this;
		if(times > 0)
		{
			for(ssize_t i = 0; i < times; i++)
			{
				// auto old = ret;
				ret = ret->getPointerTo();

				// printf("ret: %s // %s\n", old->str().c_str(), ret->str().c_str());
			}
		}
		else if(times < 0)
		{
			for(ssize_t i = 0; i < -times; i++)
				ret = ret->getPointerElementType();
		}

		ret = tc->normaliseType(ret);
		return ret;
	}


	Type* Type::fromBuiltin(std::string builtin, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		int indirections = 0;
		builtin = Codegen::unwrapPointerType(builtin, &indirections);

		Type* real = 0;

		if(builtin == INT8_TYPE_STRING)				real = PrimitiveType::getInt8(tc);
		else if(builtin == INT16_TYPE_STRING)		real = PrimitiveType::getInt16(tc);
		else if(builtin == INT32_TYPE_STRING)		real = PrimitiveType::getInt32(tc);
		else if(builtin == INT64_TYPE_STRING)		real = PrimitiveType::getInt64(tc);
		else if(builtin == INTUNSPEC_TYPE_STRING)	real = PrimitiveType::getInt64(tc);

		else if(builtin == UINT8_TYPE_STRING)		real = PrimitiveType::getUint8(tc);
		else if(builtin == UINT16_TYPE_STRING)		real = PrimitiveType::getUint16(tc);
		else if(builtin == UINT32_TYPE_STRING)		real = PrimitiveType::getUint32(tc);
		else if(builtin == UINT64_TYPE_STRING)		real = PrimitiveType::getUint64(tc);
		else if(builtin == UINTUNSPEC_TYPE_STRING)	real = PrimitiveType::getUint64(tc);

		else if(builtin == FLOAT32_TYPE_STRING)		real = PrimitiveType::getFloat32(tc);

		// float is implicit double.
		else if(builtin == FLOAT64_TYPE_STRING)		real = PrimitiveType::getFloat64(tc);
		else if(builtin == FLOAT_TYPE_STRING)		real = PrimitiveType::getFloat64(tc);

		else if(builtin == BOOL_TYPE_STRING)		real = PrimitiveType::getBool(tc);
		else if(builtin == VOID_TYPE_STRING)		real = PrimitiveType::getVoid(tc);
		else return 0;

		iceAssert(real);

		real = real->getIndirectedType(indirections);
		return real;
	}


	Type* Type::fromLlvmType(fir::Type* ltype, std::deque<bool> signage)
	{
		iceAssert(0);
		return PrimitiveType::getVoid();
	}







	bool Type::isPointerTo(Type* other)
	{
		return other->getPointerTo() == this;
	}

	bool Type::isPointerElementOf(Type* other)
	{
		return this->getPointerTo() == other;
	}

	PrimitiveType* Type::toPrimitiveType()
	{
		if(this->typeKind != FTypeKind::Integer && this->typeKind != FTypeKind::Floating) return 0;
		return dynamic_cast<PrimitiveType*>(this);
	}

	FunctionType* Type::toFunctionType()
	{
		if(this->typeKind != FTypeKind::Function) return 0;
		return dynamic_cast<FunctionType*>(this);
	}

	PointerType* Type::toPointerType()
	{
		if(this->typeKind != FTypeKind::Pointer) return 0;
		return dynamic_cast<PointerType*>(this);
	}

	StructType* Type::toStructType()
	{
		if(this->typeKind != FTypeKind::Struct) return 0;
		return dynamic_cast<StructType*>(this);
	}

	TupleType* Type::toTupleType()
	{
		if(this->typeKind != FTypeKind::Tuple) return 0;
		return dynamic_cast<TupleType*>(this);
	}

	ArrayType* Type::toArrayType()
	{
		if(this->typeKind != FTypeKind::Array) return 0;
		return dynamic_cast<ArrayType*>(this);
	}

	LLVariableArrayType* Type::toLLVariableArray()
	{
		if(this->typeKind != FTypeKind::LowLevelVariableArray) return 0;
		return dynamic_cast<LLVariableArrayType*>(this);
	}



















	bool Type::isStructType()
	{
		return this->toStructType() != 0;
	}

	bool Type::isTupleType()
	{
		return this->toTupleType() != 0
			&& (this->typeKind == FTypeKind::Tuple);
	}

	bool Type::isPackedStruct()
	{
		return this->toStructType() != 0
			&& (this->typeKind == FTypeKind::Struct)
			&& (this->toStructType()->isTypePacked);
	}

	bool Type::isArrayType()
	{
		return this->toArrayType() != 0;
	}

	bool Type::isFloatingPointType()
	{
		return this->typeKind == FTypeKind::Floating
			&& this->toPrimitiveType() != 0;
	}

	bool Type::isIntegerType()
	{
		return this->typeKind == FTypeKind::Integer
			&& this->toPrimitiveType() != 0;
	}

	bool Type::isSignedIntType()
	{
		return this->typeKind == FTypeKind::Integer
			&& this->toPrimitiveType() != 0
			&& this->toPrimitiveType()->isSigned();
	}

	bool Type::isFunctionType()
	{
		return this->typeKind == FTypeKind::Function
			&& this->toFunctionType() != 0;
	}

	bool Type::isPrimitiveType()
	{
		return this->toPrimitiveType() != 0;
	}

	bool Type::isPointerType()
	{
		return this->toPointerType() != 0;
	}

	bool Type::isVoidType()
	{
		return this->isTypeVoid;
	}

	bool Type::isLLVariableArrayType()
	{
		return this->typeKind == FTypeKind::LowLevelVariableArray;
	}

	bool Type::isNullPointer()
	{
		return this == PrimitiveType::getVoid()->getPointerTo();
	}
}


















