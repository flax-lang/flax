// type.h
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

#include "errors.h"

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

namespace fir
{
	struct Type;
	struct Module;
}

namespace fir
{
	// NOTE: i don't really want to deal with inheritance stuff right now,
	// so Type will encapsulate everything.
	// we shouldn't be making any copies anyway, so space/performance is a negligible concern

	struct Type;
	struct PrimitiveType;
	struct FunctionType;
	struct PointerType;
	struct StructType;
	struct ArrayType;

	struct FTContext
	{
		// primitives
		// NOTE: map is ordered by bit width.
		// floats + ints here too.
		std::unordered_map<size_t, std::vector<PrimitiveType*>> primitiveTypes;

		// special little thing.
		PrimitiveType* voidType = 0;

		// fir::LLVMContext* llvmContext = 0;
		fir::Module* module = 0;

		// keyed by number of indirections
		std::unordered_map<size_t, std::vector<Type*>> typeCache;
		Type* normaliseType(Type* type);
	};

	FTContext* createFTContext();
	FTContext* getDefaultFTContext();
	void setDefaultFTContext(FTContext* tc);

	enum class FTypeKind
	{
		Invalid,

		Void,
		Pointer,

		NamedStruct,
		LiteralStruct,

		Integer,
		Floating,

		Array,
		Function,
	};




	struct Type
	{
		// aquaintances
		friend struct FTContext;
		friend FTContext* createFTContext();

		// stuff
		static Type* fromBuiltin(std::string builtin, FTContext* tc = 0);
		static Type* fromLlvmType(fir::Type* ltype, std::deque<bool> signage);

		static bool areTypesEqual(Type* a, Type* b);

		// various
		virtual std::string str() = 0;
		virtual bool isTypeEqual(Type* other) = 0;
		virtual fir::Type* getLlvmType(FTContext* tc = 0) = 0;



		Type* getPointerTo(FTContext* tc = 0);
		Type* getPointerElementType(FTContext* tc = 0);


		PrimitiveType* toPrimitiveType();
		FunctionType* toFunctionType();
		PointerType* toPointerType();
		StructType* toStructType();
		ArrayType* toArrayType();


		// bool isPointerTo(Type* other, FTContext* tc = 0);
		// bool isArrayElementOf(Type* other, FTContext* tc = 0);
		// bool isPointerElementOf(Type* other, FTContext* tc = 0);


		bool isStructType();
		bool isNamedStruct();
		bool isLiteralStruct();
		bool isPackedStruct();


		bool isArrayType();
		bool isIntegerType();
		bool isFloatingPointType();

		bool isPointerType();
		bool isVoid();

		Type* getIndirectedType(ssize_t times, FTContext* tc = 0);

		protected:
		Type(FTypeKind baseType)
		{
			static size_t __id = 0;
			this->id = __id++;

			this->typeKind = baseType;
		}

		virtual ~Type() { }

		// base things
		size_t id = 0;
		fir::Type* llvmType = 0;

		FTypeKind typeKind = FTypeKind::Invalid;

		bool isTypeVoid = 0;

		static Type* getOrCreateFloatingTypeWithConstraints(FTContext* tc, size_t inds, size_t bits);
		static Type* getOrCreateIntegerTypeWithConstraints(FTContext* tc, size_t inds, bool issigned, size_t bits);
		static Type* getOrCreateArrayTypeWithConstraints(FTContext* tc, size_t inds, size_t arrsize, Type* elm);
		static Type* getOrCreateStructTypeWithConstraints(FTContext* tc, size_t inds, bool islit, std::string name,
			std::deque<Type*> mems);

		static Type* getOrCreateFunctionTypeWithConstraints(FTContext* tc, size_t inds, bool isva, std::deque<Type*> args,
			Type* ret);

		static std::string typeListToString(std::deque<Type*> types);
		static bool areTypeListsEqual(std::deque<Type*> a, std::deque<Type*> b);
	};






















	struct PrimitiveType : Type
	{
		friend struct Type;

		friend struct FTContext;
		friend FTContext* createFTContext();

		// methods
		bool isSigned();
		size_t getIntegerBitWidth();
		size_t getFloatingPointBitWidth();

		// protected constructor
		protected:
		PrimitiveType(size_t bits, FTypeKind kind);
		virtual ~PrimitiveType() override { }
		virtual std::string str() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual fir::Type* getLlvmType(FTContext* tc = 0) override;


		// fields (protected)
		bool isTypeSigned = 0;
		size_t bitWidth = 0;


		// static funcs
		protected:

		static PrimitiveType* getIntWithBitWidthAndSignage(FTContext* tc, size_t bits, bool issigned);
		static PrimitiveType* getFloatWithBitWidth(FTContext* tc, size_t bits);


		public:

		static PrimitiveType* getBool(FTContext* tc = 0);
		static PrimitiveType* getVoid(FTContext* tc = 0);
		static PrimitiveType* getInt8(FTContext* tc = 0);
		static PrimitiveType* getInt16(FTContext* tc = 0);
		static PrimitiveType* getInt32(FTContext* tc = 0);
		static PrimitiveType* getInt64(FTContext* tc = 0);
		static PrimitiveType* getUint8(FTContext* tc = 0);
		static PrimitiveType* getUint16(FTContext* tc = 0);
		static PrimitiveType* getUint32(FTContext* tc = 0);
		static PrimitiveType* getUint64(FTContext* tc = 0);
		static PrimitiveType* getFloat32(FTContext* tc = 0);
		static PrimitiveType* getFloat64(FTContext* tc = 0);
	};



	struct PointerType : Type
	{
		friend struct Type;

		friend struct FTContext;
		friend FTContext* createFTContext();

		// methods
		size_t getIndirections();

		// protected constructor
		protected:
		PointerType(size_t inds, Type* base);
		virtual ~PointerType() override { }
		virtual std::string str() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual fir::Type* getLlvmType(FTContext* tc = 0) override;


		size_t indirections = 0;
		Type* baseType = 0;

		// static funcs
		public:

		static PointerType* getInt8Ptr(FTContext* tc = 0);
		static PointerType* getInt16Ptr(FTContext* tc = 0);
		static PointerType* getInt32Ptr(FTContext* tc = 0);
		static PointerType* getInt64Ptr(FTContext* tc = 0);
		static PointerType* getUint8Ptr(FTContext* tc = 0);
		static PointerType* getUint16Ptr(FTContext* tc = 0);
		static PointerType* getUint32Ptr(FTContext* tc = 0);
		static PointerType* getUint64Ptr(FTContext* tc = 0);
		static PointerType* getFloat32Ptr(FTContext* tc = 0);
		static PointerType* getFloat64Ptr(FTContext* tc = 0);
	};


	struct StructType : Type
	{
		friend struct Type;

		// methods
		std::string getStructName();
		size_t getElementCount();
		Type* getElementN(size_t n);
		std::vector<Type*> getElements();

		void setBody(std::initializer_list<Type*> members);
		void setBody(std::vector<Type*> members);
		void setBody(std::deque<Type*> members);

		void deleteType(FTContext* tc = 0);

		// protected constructor
		protected:
		StructType(std::string name, std::deque<Type*> mems, bool islit, bool ispacked);
		virtual ~StructType() override { }
		virtual std::string str() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual fir::Type* getLlvmType(FTContext* tc = 0) override;

		// fields (protected)
		bool isTypePacked;
		std::string structName;
		std::deque<Type*> structMembers;


		// static funcs
		public:
		static StructType* getOrCreateNamedStruct(std::string name, std::initializer_list<Type*> members,
			FTContext* tc = 0, bool isPacked = false);

		static StructType* getOrCreateNamedStruct(std::string name, std::deque<Type*> members,
			FTContext* tc = 0, bool isPacked = false);

		static StructType* getOrCreateNamedStruct(std::string name, std::vector<Type*> members,
			FTContext* tc = 0, bool isPacked = false);

		static StructType* getLiteralStruct(std::initializer_list<Type*> members, FTContext* tc = 0, bool isPacked = false);
		static StructType* getLiteralStruct(std::deque<Type*> members, FTContext* tc = 0, bool isPacked = false);
		static StructType* getLiteralStruct(std::vector<Type*> members, FTContext* tc = 0, bool isPacked = false);
	};

	struct ArrayType : Type
	{
		friend struct Type;

		// methods
		Type* getElementType();
		size_t getArraySize();

		// protected constructor
		protected:
		ArrayType(Type* elmType, size_t sz);
		virtual ~ArrayType() override { }
		virtual std::string str() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual fir::Type* getLlvmType(FTContext* tc = 0) override;

		// fields (protected)
		size_t arraySize;
		Type* arrayElementType;

		// static funcs
		public:
		static ArrayType* getArray(Type* elementType, size_t num, FTContext* tc = 0);
	};


	struct FunctionType : Type
	{
		friend struct Type;

		// methods
		std::deque<Type*> getArgumentTypes();
		Type* getArgumentN(size_t n);
		Type* getReturnType();
		bool isVarArg();

		// protected constructor
		protected:
		FunctionType(std::deque<Type*> args, Type* ret, bool isva);
		virtual ~FunctionType() override { }
		virtual std::string str() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual fir::Type* getLlvmType(FTContext* tc = 0) override;

		// fields (protected)
		bool isFnVarArg;
		std::deque<Type*> functionParams;
		Type* functionRetType;

		// static funcs
		public:
		static FunctionType* getFunction(std::deque<Type*> args, Type* ret, bool isVarArg, FTContext* tc = 0);
		static FunctionType* getFunction(std::vector<Type*> args, Type* ret, bool isVarArg, FTContext* tc = 0);
		static FunctionType* getFunction(std::initializer_list<Type*> args, Type* ret, bool isVarArg, FTContext* tc = 0);
	};
}




























