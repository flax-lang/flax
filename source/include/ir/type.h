// type.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "identifier.h"
#include "precompile.h"

namespace fir
{
	// NOTE: i don't really want to deal with inheritance stuff right now,
	// so Type will encapsulate everything.
	// we shouldn't be making any copies anyway, so space/performance is a negligible concern

	struct Type;
	struct Module;
	struct AnyType;
	struct NullType;
	struct VoidType;
	struct CharType;
	struct EnumType;
	struct BoolType;
	struct ArrayType;
	struct TupleType;
	struct ClassType;
	struct RangeType;
	struct StructType;
	struct StringType;
	struct PointerType;
	struct FunctionType;
	struct PrimitiveType;
	struct ArraySliceType;
	struct DynamicArrayType;
	struct ConstantNumberType;

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

		// special thing #2
		NullType* nullType = 0;

		// #3
		BoolType* boolType = 0;

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
		static Type* fromBuiltin(const std::string& builtin, FTContext* tc = 0);

		static bool areTypesEqual(Type* a, Type* b);

		static std::string typeListToString(const std::vector<Type*>& types);
		static std::string typeListToString(const std::initializer_list<Type*>& types);

		static bool areTypeListsEqual(const std::vector<Type*>& a, const std::vector<Type*>& b);
		static bool areTypeListsEqual(const std::initializer_list<Type*>& a, const std::initializer_list<Type*>& b);

		// various
		virtual std::string str() = 0;
		virtual std::string encodedStr() = 0;
		virtual bool isTypeEqual(Type* other) = 0;

		Type* getPointerTo(FTContext* tc = 0);
		Type* getMutablePointerTo(FTContext* tc = 0);
		Type* getPointerElementType(FTContext* tc = 0);

		Type* getMutablePointerVersion(FTContext* tc = 0);
		Type* getImmutablePointerVersion(FTContext* tc = 0);

		// note: works for all array types, be it dynamic, fixed, or slices
		Type* getArrayElementType();

		ConstantNumberType* toConstantNumberType();
		DynamicArrayType* toDynamicArrayType();
		ArraySliceType* toArraySliceType();
		PrimitiveType* toPrimitiveType();
		FunctionType* toFunctionType();
		PointerType* toPointerType();
		StructType* toStructType();
		StringType* toStringType();
		RangeType* toRangeType();
		ClassType* toClassType();
		TupleType* toTupleType();
		ArrayType* toArrayType();
		BoolType* toBoolType();
		CharType* toCharType();
		EnumType* toEnumType();
		NullType* toNullType();
		AnyType* toAnyType();

		bool isPointerTo(Type* other);
		bool isPointerElementOf(Type* other);

		bool isTupleType();
		bool isClassType();
		bool isStructType();
		bool isPackedStruct();

		bool isRangeType();

		bool isCharType();
		bool isStringType();

		bool isAnyType();
		bool isEnumType();
		bool isArrayType();
		bool isIntegerType();
		bool isFunctionType();
		bool isSignedIntType();
		bool isFloatingPointType();

		bool isArraySliceType();
		bool isDynamicArrayType();
		bool isVariadicArrayType();

		bool isPrimitiveType();
		bool isPointerType();
		bool isVoidType();
		bool isNullType();
		bool isBoolType();

		bool isMutablePointer();
		bool isImmutablePointer();
		bool isConstantNumberType();

		size_t getBitWidth();

		Type* getIndirectedType(int times, FTContext* tc = 0);

		size_t getID() { return this->id; }


		// convenience
		static VoidType* getVoid(FTContext* tc = 0);
		static NullType* getNull(FTContext* tc = 0);

		static Type* getVoidPtr(FTContext* tc = 0);

		static ConstantNumberType* getConstantNumber(mpfr::mpreal n, FTContext* tc = 0);

		static BoolType* getBool(FTContext* tc = 0);

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

		static PointerType* getMutInt8Ptr(FTContext* tc = 0);
		static PointerType* getMutInt16Ptr(FTContext* tc = 0);
		static PointerType* getMutInt32Ptr(FTContext* tc = 0);
		static PointerType* getMutInt64Ptr(FTContext* tc = 0);
		static PointerType* getMutInt128Ptr(FTContext* tc = 0);

		static PointerType* getMutUint8Ptr(FTContext* tc = 0);
		static PointerType* getMutUint16Ptr(FTContext* tc = 0);
		static PointerType* getMutUint32Ptr(FTContext* tc = 0);
		static PointerType* getMutUint64Ptr(FTContext* tc = 0);
		static PointerType* getMutUint128Ptr(FTContext* tc = 0);

		static CharType* getChar(FTContext* tc = 0);
		static StringType* getString(FTContext* tc = 0);
		static RangeType* getRange(FTContext* tc = 0);

		static AnyType* getAny(FTContext* tc = 0);


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
		PointerType* mutablePointerTo = 0;

		static Type* getOrCreateFloatingTypeWithConstraints(FTContext* tc, size_t bits);
		static Type* getOrCreateIntegerTypeWithConstraints(FTContext* tc, bool issigned, size_t bits);
		static Type* getOrCreateArrayTypeWithConstraints(FTContext* tc, size_t arrsize, Type* elm);
		static Type* getOrCreateStructTypeWithConstraints(FTContext* tc, bool islit, std::string name,
			std::vector<Type*> mems);

		static Type* getOrCreateFunctionTypeWithConstraints(FTContext* tc, bool isva, std::vector<Type*> args,
			Type* ret);
	};


















	struct BoolType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		BoolType();
		protected:
		virtual ~BoolType() override { }

		public:
		static BoolType* get(FTContext* tc = 0);
	};

	struct VoidType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		VoidType();
		protected:
		virtual ~VoidType() override { }

		public:
		static VoidType* get(FTContext* tc = 0);
	};


	struct NullType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		NullType();
		protected:
		virtual ~NullType() override { }

		public:
		static NullType* get(FTContext* tc = 0);
	};

	// special case -- the type also needs to store the number, to know things like
	// whether it's signed, negative, an integer, and other stuff.
	struct ConstantNumberType : Type
	{
		friend struct Type;
		friend struct FTContext;
		friend FTContext* createFTContext();

		mpfr::mpreal getValue();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		static ConstantNumberType* get(mpfr::mpreal num, FTContext* tc = 0);

		protected:
		ConstantNumberType(mpfr::mpreal n);
		mpfr::mpreal number;
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

		PointerType* getMutable(FTContext* tc = 0);
		PointerType* getImmutable(FTContext* tc = 0);

		bool isMutable();

		// protected constructor
		protected:
		PointerType(Type* base, bool mut);
		virtual ~PointerType() override { }
		virtual std::string str() override;
		virtual std::string encodedStr() override;

		Type* baseType = 0;
		bool isPtrMutable = false;

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

		// protected constructor
		protected:
		TupleType(const std::vector<Type*>& mems);
		virtual ~TupleType() override { }

		// fields (protected)
		std::vector<Type*> members;

		public:
		static TupleType* get(const std::initializer_list<Type*>& members, FTContext* tc = 0);
		static TupleType* get(const std::vector<Type*>& members, FTContext* tc = 0);
	};




	struct StructType : Type
	{
		friend struct Type;

		// methods
		Identifier getTypeName();
		size_t getElementCount();
		Type* getElementN(size_t n);
		Type* getElement(const std::string& name);
		bool hasElementWithName(const std::string& name);
		size_t getElementIndex(const std::string& name);
		std::vector<Type*> getElements();

		void setBody(const std::vector<std::pair<std::string, Type*>>& members);



		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		protected:
		StructType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, bool ispacked);
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
		static StructType* create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members, FTContext* tc = 0,
			bool isPacked = false);
	};





	struct ClassType : Type
	{
		friend struct Type;
		friend struct Module;

		// methods
		Identifier getTypeName();
		size_t getElementCount();
		Type* getElementN(size_t n);
		Type* getElement(const std::string& name);
		bool hasElementWithName(const std::string& name);
		size_t getElementIndex(const std::string& name);
		std::vector<Type*> getElements();

		std::vector<Function*> getMethods();
		std::vector<Function*> getMethodsWithName(std::string id);
		Function* getMethodWithType(FunctionType* ftype);

		std::vector<Function*> getInitialiserFunctions();
		void setInitialiserFunctions(const std::vector<Function*>& list);

		Function* getInlineInitialiser();
		void setInlineInitialiser(Function* fn);

		void setMembers(const std::vector<std::pair<std::string, Type*>>& members);
		void setMethods(const std::vector<Function*>& methods);

		ClassType* getBaseClass();
		void setBaseClass(ClassType* ty);

		bool isInParentHierarchy(Type* base);

		void addVirtualMethod(Function* method);
		size_t getVirtualMethodIndex(const std::string& name, FunctionType* ft);

		size_t getVirtualMethodCount();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		protected:
		ClassType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, const std::vector<Function*>& methods,
			const std::vector<Function*>& inits);

		virtual ~ClassType() override { }

		// fields (protected)
		Identifier className;
		std::vector<Type*> typeList;
		std::vector<Function*> methodList;
		std::vector<Function*> initialiserList;

		std::unordered_map<std::string, size_t> indexMap;
		std::unordered_map<std::string, Type*> classMembers;
		std::unordered_map<std::string, std::vector<Function*>> classMethodMap;

		//* how it works is that we will add in the mappings from the base class,
		//* and for our own matching virtual methods, we'll map to the same index.


		size_t virtualMethodCount = 0;
		// std::unordered_map<Function*, size_t> virtualMethodMap;
		std::unordered_map<size_t, Function*> reverseVirtualMethodMap;

		//* note: we do it this way (where we *EXCLUDE THE SELF POINTER*), because it's just easier -- to compare, and everything.
		//* we really don't have a use for mapping a fir::Function to an index, only the other way.
		std::map<std::pair<std::string, std::vector<Type*>>, size_t> virtualMethodMap;

		ClassType* baseClass = 0;
		Function* inlineInitialiser = 0;

		// static funcs
		public:
		static ClassType* createWithoutBody(const Identifier& name, FTContext* tc = 0);
		static ClassType* create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members,
			const std::vector<Function*>& methods, const std::vector<Function*>& inits, FTContext* tc = 0);
	};



	struct EnumType : Type
	{
		friend struct Type;

		Type* getCaseType();
		Identifier getTypeName();

		fir::ConstantValue* getNameArray();
		fir::ConstantValue* getCaseArray();

		void setNameArray(fir::ConstantValue* arr);
		void setCaseArray(fir::ConstantValue* arr);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		protected:
		EnumType(const Identifier& name, Type* ty);
		virtual ~EnumType() override { }

		Type* caseType;
		Identifier typeName;

		fir::ConstantValue* runtimeNameArray = 0;
		fir::ConstantValue* runtimeCasesArray = 0;

		// static funcs
		public:
		static EnumType* get(const Identifier& name, Type* caseType, FTContext* tc = 0);
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

		bool isMutable();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		protected:
		ArraySliceType(Type* elmType, bool mut);
		virtual ~ArraySliceType() override { }

		// fields
		bool isSliceMutable;
		Type* arrayElementType;

		// static funcs
		public:
		static ArraySliceType* get(Type* elementType, bool mut, FTContext* tc = 0);
		static ArraySliceType* getMutable(Type* elementType, FTContext* tc = 0);
		static ArraySliceType* getImmutable(Type* elementType, FTContext* tc = 0);
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

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;

		// protected constructor
		protected:
		FunctionType(const std::vector<Type*>& args, Type* ret, bool iscva);
		virtual ~FunctionType() override { }

		// fields (protected)
		bool isFnCStyleVarArg;

		std::vector<Type*> functionParams;
		Type* functionRetType;

		// static funcs
		public:
		static FunctionType* getCVariadicFunc(const std::vector<Type*>& args, Type* ret, FTContext* tc = 0);
		static FunctionType* getCVariadicFunc(const std::initializer_list<Type*>& args, Type* ret, FTContext* tc = 0);

		static FunctionType* get(const std::vector<Type*>& args, Type* ret, FTContext* tc = 0);
		static FunctionType* get(const std::initializer_list<Type*>& args, Type* ret, FTContext* tc = 0);
	};


	struct RangeType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;


		// protected constructor
		protected:
		RangeType();
		virtual ~RangeType() override { }

		public:
		static RangeType* get(FTContext* tc = 0);
	};


	struct StringType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;


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


		// protected constructor
		protected:
		AnyType();
		virtual ~AnyType() override { }

		public:
		static AnyType* get(FTContext* tc = 0);
	};
}
























