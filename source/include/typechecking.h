// typechecking.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include "errors.h"

#include <string>
#include <vector>
#include <deque>


namespace flax
{
	// NOTE: i don't really want to deal with inheritance stuff right now,
	// so flax::Type will encapsulate everything.
	// we shouldn't be making any copies anyway, so space/performance is a negligible concern

	class FTContext;
	FTContext* createFTContext();
	FTContext* getDefaultFTContext();
	void setDefaultFTContext(FTContext* tc);

	enum class FTypeKind
	{
		Invalid,

		Void,

		NamedStruct,
		LiteralStruct,

		Integer,
		Floating,

		Array,
		Function,
	};

	struct Type
	{
		friend class FTContext;
		friend FTContext* createFTContext();

		// structs
		static flax::Type* getOrCreateNamedStruct(std::string name, std::deque<flax::Type*> members, FTContext* tc = 0);
		static flax::Type* getOrCreateNamedStruct(std::string name, std::vector<flax::Type*> members, FTContext* tc = 0);

		static flax::Type* getLiteralStruct(std::deque<flax::Type*> members, FTContext* tc = 0);
		static flax::Type* getLiteralStruct(std::vector<flax::Type*> members, FTContext* tc = 0);


		// arrays
		static flax::Type* getArray(flax::Type* elementType, size_t num, FTContext* tc = 0);


		// functions
		static flax::Type* getFunction(std::deque<flax::Type*> args, flax::Type* ret, bool isVarArg, FTContext* tc = 0);


		// primitives
		static flax::Type* getBool(FTContext* tc = 0);
		static flax::Type* getVoid(FTContext* tc = 0);
		static flax::Type* getInt8(FTContext* tc = 0);
		static flax::Type* getInt16(FTContext* tc = 0);
		static flax::Type* getInt32(FTContext* tc = 0);
		static flax::Type* getInt64(FTContext* tc = 0);
		static flax::Type* getUint8(FTContext* tc = 0);
		static flax::Type* getUint16(FTContext* tc = 0);
		static flax::Type* getUint32(FTContext* tc = 0);
		static flax::Type* getUint64(FTContext* tc = 0);
		static flax::Type* getFloat32(FTContext* tc = 0);
		static flax::Type* getFloat64(FTContext* tc = 0);

		static bool areTypesEqual(flax::Type* a, flax::Type* b);

		static flax::Type* getIntWithBitWidthAndSignage(FTContext* tc, size_t bits, bool issigned);
		static flax::Type* getFloatWithBitWidth(FTContext* tc, size_t bits);


		// various
		std::string str();

		flax::Type* getPointerTo(FTContext* tc = 0);
		flax::Type* getPointerElementType(FTContext* tc = 0);

		llvm::Type* getLlvmType();

		bool isTypeEqual(flax::Type* other);

		bool isPointerTo(flax::Type* other);
		bool isArrayElementOf(flax::Type* other);
		bool isPointerElementOf(flax::Type* other);

		bool isPointer() { return this->indirections > 0; }
		bool isVoid() { return this->isTypeVoid; }



		bool isStructType();
		bool isNamedStruct();
		bool isLiteralStruct();

		bool isArrayType();
		bool isIntegerType();
		bool isFloatingPointType();





		// general stuff
		size_t getPointerIndirections() { return this->indirections; }

		// int stuff
		bool isSigned() { iceAssert(this->typeKind == FTypeKind::Integer); return this->isTypeSigned; }
		size_t getIntegerBitWidth() { iceAssert(this->typeKind == FTypeKind::Integer); return this->bitWidth; }


		// array stuff
		flax::Type* getArrayElementType() { iceAssert(this->typeKind == FTypeKind::Array); return this->arrayElementType; }
		size_t getArraySize() { iceAssert(this->typeKind == FTypeKind::Array); return this->arraySize; }


		// struct stuff
		std::string getStructName() { iceAssert(this->typeKind == FTypeKind::NamedStruct); return this->structName; }


		private:

		Type() :llvmType(nullptr), typeKind(FTypeKind::Invalid),
				isTypeVoid(false), isTypeSigned(false),
				bitWidth(0), arraySize(0), arrayElementType(nullptr)
		{
			static size_t __id = 0;
			this->id = __id++;
		}

		~Type()
		{
			// nothing to delete, actually.
		}

		// base things
		llvm::Type* llvmType = 0;
		size_t id = 0;
		FTypeKind typeKind = FTypeKind::Invalid;
		size_t indirections = 0;

		// primitive things
		bool isTypeVoid = 0;
		bool isTypeSigned = 0;
		size_t bitWidth = 0;


		union
		{
			// array things
			struct
			{
				size_t arraySize;
				flax::Type* arrayElementType;
			};

			// struct things
			struct
			{
				bool isTypeLiteralStruct;
				std::string structName;
				std::deque<flax::Type*> structMembers;
			};

			// function things
			struct
			{
				bool isFnVarArg;
				std::deque<flax::Type*> functionParams;
				flax::Type* functionRetType;
			};
		};

		static flax::Type* getOrCreateIntegerTypeWithConstraints(size_t inds, bool issigned, size_t bits);
		static flax::Type* getOrCreateArrayTypeWithConstraints(size_t inds, size_t arrsize, flax::Type* elm);
		static flax::Type* getOrCreateStructTypeWithConstraints(size_t inds, bool islit, std::string name, std::deque<flax::Type*> mems);
		static flax::Type* getOrCreateFunctionTypeWithConstraints(size_t inds, bool isva, std::deque<flax::Type*> args, flax::Type* ret);
		static flax::Type* cloneType(flax::Type* type);

	};
}




























