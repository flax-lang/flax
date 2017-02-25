// type.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include <map>
#include <unordered_map>
#include "ir/identifier.h"

namespace fir
{
	// NOTE: i don't really want to deal with inheritance stuff right now,
	// so Type will encapsulate everything.
	// we shouldn't be making any copies anyway, so space/performance is a negligible concern

	struct Type;
	struct Module;
	struct AnyType;
	struct VoidType;
	struct CharType;
	struct EnumType;
	struct ArrayType;
	struct TupleType;
	struct ClassType;
	struct StructType;
	struct StringType;
	struct PointerType;
	struct FunctionType;
	struct PrimitiveType;
	struct ParametricType;
	struct ArraySliceType;
	struct UnicodeCharType;
	struct DynamicArrayType;
	struct UnicodeStringType;

	struct ConstantValue;
	struct ConstantArray;
	struct Function;

	struct FTContext
	{
		// primitives
		// NOTE: map is ordered by bit width.
		// floats + ints here too.
		std::unordered_map<size_t, std::vector<PrimitiveType*>> primitiveTypes;

		// special little thing.
		VoidType* voidType = 0;

		// fir::LLVMContext* llvmContext = 0;
		fir::Module* module = 0;

		std::vector<Type*> typeCache;
		Type* normaliseType(Type* type);

		void dumpTypeIDs();
	};

	FTContext* createFTContext();
	FTContext* getDefaultFTContext();
	void setDefaultFTContext(FTContext* tc);




	struct Type
	{
		// aquaintances
		friend struct FTContext;
		friend FTContext* createFTContext();

		// stuff
		static Type* fromBuiltin(std::string builtin, FTContext* tc = 0);

		static bool areTypesEqual(Type* a, Type* b);

		static std::string typeListToString(std::vector<Type*> types);
		static std::string typeListToString(std::initializer_list<Type*> types);

		// various
		virtual std::string str() = 0;
		virtual std::string encodedStr() = 0;
		virtual bool isTypeEqual(Type* other) = 0;
		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) = 0;

		Type* getPointerTo(FTContext* tc = 0);
		Type* getPointerElementType(FTContext* tc = 0);


		DynamicArrayType* toDynamicArrayType();
		ArraySliceType* toArraySliceType();
		ParametricType* toParametricType();
		PrimitiveType* toPrimitiveType();
		FunctionType* toFunctionType();
		PointerType* toPointerType();
		StructType* toStructType();
		StringType* toStringType();
		ClassType* toClassType();
		TupleType* toTupleType();
		ArrayType* toArrayType();
		CharType* toCharType();
		EnumType* toEnumType();
		AnyType* toAnyType();

		bool isPointerTo(Type* other);
		bool isPointerElementOf(Type* other);

		bool isTupleType();
		bool isClassType();
		bool isStructType();
		bool isPackedStruct();

		bool isCharType();
		bool isStringType();

		bool isAnyType();
		bool isEnumType();
		bool isArrayType();
		bool isIntegerType();
		bool isFunctionType();
		bool isSignedIntType();
		bool isFloatingPointType();

		bool isVoidPointer();
		bool isArraySliceType();
		bool isDynamicArrayType();

		bool isParametricType();
		bool isPrimitiveType();
		bool isPointerType();
		bool isVoidType();

		Type* getIndirectedType(ssize_t times, FTContext* tc = 0);

		size_t getID() { return this->id; }


		// convenience
		static VoidType* getVoid(FTContext* tc = 0);
		static Type* getVoidPtr(FTContext* tc = 0);

		static PrimitiveType* getBool(FTContext* tc = 0);
		static PrimitiveType* getInt8(FTContext* tc = 0);
		static PrimitiveType* getInt16(FTContext* tc = 0);
		static PrimitiveType* getInt32(FTContext* tc = 0);
		static PrimitiveType* getInt64(FTContext* tc = 0);
		static PrimitiveType* getInt128(FTContext* tc = 0);

		static PrimitiveType* getUint8(FTContext* tc = 0);
		static PrimitiveType* getUint16(FTContext* tc = 0);
		static PrimitiveType* getUint32(FTContext* tc = 0);
		static PrimitiveType* getUint64(FTContext* tc = 0);
		static PrimitiveType* getUint128(FTContext* tc = 0);

		static PrimitiveType* getFloat32(FTContext* tc = 0);
		static PrimitiveType* getFloat64(FTContext* tc = 0);
		static PrimitiveType* getFloat80(FTContext* tc = 0);
		static PrimitiveType* getFloat128(FTContext* tc = 0);

		static PointerType* getInt8Ptr(FTContext* tc = 0);
		static PointerType* getInt16Ptr(FTContext* tc = 0);
		static PointerType* getInt32Ptr(FTContext* tc = 0);
		static PointerType* getInt64Ptr(FTContext* tc = 0);
		static PointerType* getInt128Ptr(FTContext* tc = 0);

		static PointerType* getUint8Ptr(FTContext* tc = 0);
		static PointerType* getUint16Ptr(FTContext* tc = 0);
		static PointerType* getUint32Ptr(FTContext* tc = 0);
		static PointerType* getUint64Ptr(FTContext* tc = 0);
		static PointerType* getUint128Ptr(FTContext* tc = 0);

		static CharType* getCharType(FTContext* tc = 0);
		static StringType* getStringType(FTContext* tc = 0);

		static AnyType* getAnyType(FTContext* tc = 0);


		protected:
		Type()
		{
			static size_t __id = 0;
			this->id = __id++;
		}

		virtual ~Type() { }

		// base things
		size_t id = 0;

		PointerType* pointerTo = 0;

		static Type* getOrCreateFloatingTypeWithConstraints(FTContext* tc, size_t bits);
		static Type* getOrCreateIntegerTypeWithConstraints(FTContext* tc, bool issigned, size_t bits);
		static Type* getOrCreateArrayTypeWithConstraints(FTContext* tc, size_t arrsize, Type* elm);
		static Type* getOrCreateStructTypeWithConstraints(FTContext* tc, bool islit, std::string name,
			std::vector<Type*> mems);

		static Type* getOrCreateFunctionTypeWithConstraints(FTContext* tc, bool isva, std::vector<Type*> args,
			Type* ret);

		static bool areTypeListsEqual(std::vector<Type*> a, std::vector<Type*> b);
		static bool areTypeListsEqual(std::initializer_list<Type*> a, std::initializer_list<Type*> b);
	};




















	struct VoidType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		VoidType();
		protected:
		virtual ~VoidType() override { }

		public:
		static VoidType* get(FTContext* tc = 0);
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
		PrimitiveType* getOppositeSignedType();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual PrimitiveType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;


		enum class Kind
		{
			Invalid,

			Integer,
			Floating,
		};


		// protected constructor
		protected:
		PrimitiveType(size_t bits, Kind _kind);
		virtual ~PrimitiveType() override { }


		// fields (protected)
		bool isTypeSigned = 0;
		size_t bitWidth = 0;

		Kind primKind = Kind::Invalid;

		// static funcs
		protected:

		static PrimitiveType* getIntWithBitWidthAndSignage(FTContext* tc, size_t bits, bool issigned);
		static PrimitiveType* getFloatWithBitWidth(FTContext* tc, size_t bits);


		public:
		static PrimitiveType* getIntN(size_t bits, FTContext* tc = 0);
		static PrimitiveType* getUintN(size_t bits, FTContext* tc = 0);

		static PrimitiveType* getBool(FTContext* tc = 0);
		static PrimitiveType* getInt8(FTContext* tc = 0);
		static PrimitiveType* getInt16(FTContext* tc = 0);
		static PrimitiveType* getInt32(FTContext* tc = 0);
		static PrimitiveType* getInt64(FTContext* tc = 0);
		static PrimitiveType* getInt128(FTContext* tc = 0);

		static PrimitiveType* getUint8(FTContext* tc = 0);
		static PrimitiveType* getUint16(FTContext* tc = 0);
		static PrimitiveType* getUint32(FTContext* tc = 0);
		static PrimitiveType* getUint64(FTContext* tc = 0);
		static PrimitiveType* getUint128(FTContext* tc = 0);

		static PrimitiveType* getFloat32(FTContext* tc = 0);
		static PrimitiveType* getFloat64(FTContext* tc = 0);
		static PrimitiveType* getFloat80(FTContext* tc = 0);
		static PrimitiveType* getFloat128(FTContext* tc = 0);
	};



	struct PointerType : Type
	{
		friend struct Type;

		friend struct FTContext;
		friend FTContext* createFTContext();

		virtual bool isTypeEqual(Type* other) override;
		virtual PointerType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		PointerType(Type* base);
		virtual ~PointerType() override { }
		virtual std::string str() override;
		virtual std::string encodedStr() override;

		Type* baseType = 0;

		// static funcs
		public:

		static PointerType* getInt8Ptr(FTContext* tc = 0);
		static PointerType* getInt16Ptr(FTContext* tc = 0);
		static PointerType* getInt32Ptr(FTContext* tc = 0);
		static PointerType* getInt64Ptr(FTContext* tc = 0);
		static PointerType* getInt128Ptr(FTContext* tc = 0);

		static PointerType* getUint8Ptr(FTContext* tc = 0);
		static PointerType* getUint16Ptr(FTContext* tc = 0);
		static PointerType* getUint32Ptr(FTContext* tc = 0);
		static PointerType* getUint64Ptr(FTContext* tc = 0);
		static PointerType* getUint128Ptr(FTContext* tc = 0);
	};



	struct TupleType : Type
	{
		friend struct Type;

		// methods
		size_t getElementCount();
		Type* getElementN(size_t n);
		std::vector<Type*> getElements();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual TupleType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		TupleType(std::vector<Type*> mems);
		virtual ~TupleType() override { }

		// fields (protected)
		std::vector<Type*> members;

		public:
		static TupleType* get(std::initializer_list<Type*> members, FTContext* tc = 0);
		static TupleType* get(std::vector<Type*> members, FTContext* tc = 0);
	};




	struct StructType : Type
	{
		friend struct Type;

		// methods
		Identifier getStructName();
		size_t getElementCount();
		Type* getElementN(size_t n);
		Type* getElement(std::string name);
		bool hasElementWithName(std::string name);
		size_t getElementIndex(std::string name);
		std::vector<Type*> getElements();

		void setBody(std::vector<std::pair<std::string, Type*>> members);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual StructType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		StructType(const Identifier& name, std::vector<std::pair<std::string, Type*>> mems, bool ispacked);
		virtual ~StructType() override { }

		// fields (protected)
		bool isTypePacked;
		Identifier structName;
		std::vector<Type*> typeList;
		std::unordered_map<std::string, size_t> indexMap;
		std::unordered_map<std::string, Type*> structMembers;

		// static funcs
		public:
		static StructType* createWithoutBody(const Identifier& name, FTContext* tc = 0, bool isPacked = false);
		static StructType* create(const Identifier& name, std::vector<std::pair<std::string, Type*>> members, FTContext* tc = 0,
			bool isPacked = false);
	};





	struct ClassType : Type
	{
		friend struct Type;

		// methods
		Identifier getClassName();
		size_t getElementCount();
		Type* getElementN(size_t n);
		Type* getElement(std::string name);
		bool hasElementWithName(std::string name);
		size_t getElementIndex(std::string name);
		std::vector<Type*> getElements();

		std::vector<Function*> getMethods();
		std::vector<Function*> getMethodsWithName(std::string id);
		Function* getMethodWithType(FunctionType* ftype);

		void setMembers(std::vector<std::pair<std::string, Type*>> members);
		void setMethods(std::vector<Function*> methods);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual ClassType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		ClassType(const Identifier& name, std::vector<std::pair<std::string, Type*>> mems, std::vector<Function*> methods);
		virtual ~ClassType() override { }

		// fields (protected)
		Identifier className;
		std::vector<Type*> typeList;
		std::vector<Function*> methodList;

		std::unordered_map<std::string, size_t> indexMap;
		std::unordered_map<std::string, Type*> classMembers;
		std::unordered_map<std::string, std::vector<Function*>> classMethodMap;

		// static funcs
		public:
		static ClassType* createWithoutBody(const Identifier& name, FTContext* tc = 0);
		static ClassType* create(const Identifier& name, std::vector<std::pair<std::string, Type*>> members,
			std::vector<Function*> methods, FTContext* tc = 0);
	};



	struct EnumType : Type
	{
		friend struct Type;

		bool hasCaseWithName(std::string name);
		ConstantValue* getCaseWithName(std::string name);
		ConstantArray* getConstantArrayOfValues();
		Type* getCaseType();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		Identifier getEnumName();

		// protected constructor
		protected:
		EnumType(const Identifier& name, Type* caseType, std::map<std::string, ConstantValue*> _cases);
		virtual ~EnumType() override { }

		Identifier enumName;
		Type* caseType;
		std::map<std::string, ConstantValue*> cases;

		// static funcs
		public:
		static EnumType* get(const Identifier& name, Type* caseType, std::map<std::string, ConstantValue*> _cases, FTContext* tc = 0);
	};






	struct ArrayType : Type
	{
		friend struct Type;

		// methods
		Type* getElementType();
		size_t getArraySize();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual ArrayType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		ArrayType(Type* elmType, size_t sz);
		virtual ~ArrayType() override { }

		// fields (protected)
		size_t arraySize;
		Type* arrayElementType;

		// static funcs
		public:
		static ArrayType* get(Type* elementType, size_t num, FTContext* tc = 0);
	};


	struct DynamicArrayType : Type
	{
		friend struct Type;

		// methods
		Type* getElementType();
		bool isFunctionVariadic();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual DynamicArrayType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		DynamicArrayType(Type* elmType);
		virtual ~DynamicArrayType() override { }

		// fields
		bool isVariadic = false;
		Type* arrayElementType;

		// static funcs
		public:
		static DynamicArrayType* get(Type* elementType, FTContext* tc = 0);
		static DynamicArrayType* getVariadic(Type* elementType, FTContext* tc = 0);
	};


	struct ArraySliceType : Type
	{
		friend struct Type;

		// methods
		Type* getElementType();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual ArraySliceType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		ArraySliceType(Type* elmType);
		virtual ~ArraySliceType() override { }

		// fields
		Type* arrayElementType;

		// static funcs
		public:
		static ArraySliceType* get(Type* elementType, FTContext* tc = 0);
	};



	struct ProtocolType : Type
	{
		friend struct Type;

		// methods


		// protected constructor


		// fields


		// static funcs
		public:
	};


	struct FunctionType : Type
	{
		friend struct Type;

		// methods
		std::vector<Type*> getArgumentTypes();
		Type* getArgumentN(size_t n);
		Type* getReturnType();

		bool isCStyleVarArg();
		bool isVariadicFunc();

		std::vector<ParametricType*> getTypeParameters();
		void addTypeParameter(ParametricType* t);
		void addTypeParameters(std::vector<ParametricType*> ts);

		bool isGenericFunction();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual FunctionType* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		FunctionType(std::vector<Type*> args, Type* ret, bool isvariadic, bool iscva);
		virtual ~FunctionType() override { }

		// fields (protected)
		bool isFnCStyleVarArg;
		bool isFnVariadic;

		std::vector<ParametricType*> typeParameters;

		std::vector<Type*> functionParams;
		Type* functionRetType;

		// static funcs
		public:
		static FunctionType* getCVariadicFunc(std::vector<Type*> args, Type* ret, FTContext* tc = 0);
		static FunctionType* getCVariadicFunc(std::initializer_list<Type*> args, Type* ret, FTContext* tc = 0);

		static FunctionType* get(std::vector<Type*> args, Type* ret, bool isVariadic, FTContext* tc = 0);
		static FunctionType* get(std::initializer_list<Type*> args, Type* ret, bool isVariadic, FTContext* tc = 0);

		static FunctionType* getWithTypeParameters(std::vector<Type*> args, Type* ret, bool isVariadic, std::vector<ParametricType*> tparams, FTContext* tc = 0);
		static FunctionType* getWithTypeParameters(std::initializer_list<Type*> args, Type* ret, bool isVariadic, std::vector<ParametricType*> tparams, FTContext* tc = 0);
	};


	struct StringType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		StringType();
		virtual ~StringType() override { }

		public:
		static StringType* get(FTContext* tc = 0);
	};

	struct CharType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		CharType();
		virtual ~CharType() override { }

		public:
		static CharType* get(FTContext* tc = 0);
	};






	struct AnyType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		// protected constructor
		protected:
		AnyType();
		virtual ~AnyType() override { }

		public:
		static AnyType* get(FTContext* tc = 0);
	};



	struct ParametricType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		virtual Type* reify(std::map<std::string, Type*> names, FTContext* tc = 0) override;

		std::string getName();

		// protected constructor
		protected:
		ParametricType(std::string name);
		virtual ~ParametricType() override { }

		std::string name;

		// static funcs
		public:
		static ParametricType* get(std::string name, FTContext* tc = 0);
	};
}




























