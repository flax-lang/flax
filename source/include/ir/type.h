// type.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "precompile.h"

namespace fir
{
	// NOTE: i don't really want to deal with inheritance stuff right now,
	// so Type will encapsulate everything.
	// we shouldn't be making any copies anyway, so space/performance is a negligible concern

	struct Type;
	struct Module;
	struct AnyType;
	struct BoolType;
	struct EnumType;
	struct NullType;
	struct VoidType;
	struct ArrayType;
	struct ClassType;
	struct RangeType;
	struct TraitType;
	struct TupleType;
	struct UnionType;
	struct StringType;
	struct StructType;
	struct OpaqueType;
	struct PointerType;
	struct FunctionType;
	struct RawUnionType;
	struct PrimitiveType;
	struct ArraySliceType;
	struct DynamicArrayType;
	struct UnionVariantType;
	struct ConstantNumberType;
	struct PolyPlaceholderType;

	struct ConstantValue;
	struct ConstantArray;
	struct Function;

	ConstantNumberType* unifyConstantTypes(ConstantNumberType* a, ConstantNumberType* b);
	Type* getBestFitTypeForConstant(ConstantNumberType* cnt);

	int getCastDistance(Type* from, Type* to);
	bool isRefCountedType(Type* ty);

	void setNativeWordSizeInBits(size_t sz);
	size_t getNativeWordSizeInBits();

	// in theory.
	size_t getSizeOfType(Type* type);
	size_t getAlignmentOfType(Type* type);

	bool areTypesCovariant(Type* base, Type* derv);
	bool areTypesContravariant(Type* base, Type* derv, bool traitChecking);
	bool areMethodsVirtuallyCompatible(FunctionType* base, FunctionType* fn, bool traitChecking);
	bool areTypeListsContravariant(const std::vector<Type*>& base, const std::vector<Type*>& derv, bool traitChecking);


	enum class TypeKind
	{
		Invalid,

		Any,
		Null,
		Void,
		Enum,
		Bool,
		Array,
		Tuple,
		Class,
		Range,
		Union,
		Trait,
		Struct,
		String,
		Opaque,
		Pointer,
		Function,
		RawUnion,
		Primitive,
		ArraySlice,
		DynamicArray,
		UnionVariant,
		ConstantNumber,
		PolyPlaceholder,
	};

	struct Type
	{
		// stuff
		static Type* fromBuiltin(const std::string& builtin);

		static std::string typeListToString(const std::vector<Type*>& types, bool includeBraces = false);
		static std::string typeListToString(const std::initializer_list<Type*>& types, bool includeBraces = false);

		static bool areTypeListsEqual(const std::vector<Type*>& a, const std::vector<Type*>& b);
		static bool areTypeListsEqual(const std::initializer_list<Type*>& a, const std::initializer_list<Type*>& b);

		// various
		virtual std::string str() = 0;
		virtual std::string encodedStr() = 0;
		virtual bool isTypeEqual(Type* other) = 0;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) = 0;

		Type* getPointerTo();
		Type* getMutablePointerTo();
		Type* getPointerElementType();

		Type* getMutablePointerVersion();
		Type* getImmutablePointerVersion();

		// note: works for all array types, be it dynamic, fixed, or slices
		Type* getArrayElementType();

		PolyPlaceholderType* toPolyPlaceholderType();
		ConstantNumberType* toConstantNumberType();
		DynamicArrayType* toDynamicArrayType();
		UnionVariantType* toUnionVariantType();
		ArraySliceType* toArraySliceType();
		PrimitiveType* toPrimitiveType();
		RawUnionType* toRawUnionType();
		FunctionType* toFunctionType();
		PointerType* toPointerType();
		OpaqueType* toOpaqueType();
		StructType* toStructType();
		StringType* toStringType();
		TraitType* toTraitType();
		RangeType* toRangeType();
		ClassType* toClassType();
		UnionType* toUnionType();
		TupleType* toTupleType();
		ArrayType* toArrayType();
		BoolType* toBoolType();
		EnumType* toEnumType();
		NullType* toNullType();
		AnyType* toAnyType();

		bool isPointerTo(Type* other);
		bool isPointerElementOf(Type* other);

		bool isTraitType();
		bool isUnionType();
		bool isTupleType();
		bool isClassType();
		bool isStructType();
		bool isPackedStruct();
		bool isRawUnionType();
		bool isUnionVariantType();

		bool isRangeType();

		bool isCharType();
		bool isStringType();

		bool isOpaqueType();

		bool isAnyType();
		bool isEnumType();
		bool isArrayType();
		bool isIntegerType();
		bool isFunctionType();
		bool isSignedIntType();
		bool isUnsignedIntType();
		bool isFloatingPointType();

		bool isArraySliceType();
		bool isDynamicArrayType();
		bool isVariadicArrayType();

		bool isCharSliceType();

		bool isPrimitiveType();
		bool isPointerType();
		bool isVoidType();
		bool isNullType();
		bool isBoolType();

		bool isMutablePointer();
		bool isImmutablePointer();
		bool isConstantNumberType();
		bool isPolyPlaceholderType();

		bool containsPlaceholders();
		std::vector<PolyPlaceholderType*> getContainedPlaceholders();

		size_t getBitWidth();

		Type* getIndirectedType(int times);

		size_t getID() { return this->id; }


		// convenience
		static VoidType* getVoid();
		static NullType* getNull();

		static Type* getVoidPtr();

		static BoolType* getBool();

		static PrimitiveType* getInt8();
		static PrimitiveType* getInt16();
		static PrimitiveType* getInt32();
		static PrimitiveType* getInt64();
		static PrimitiveType* getInt128();

		static PrimitiveType* getUint8();
		static PrimitiveType* getUint16();
		static PrimitiveType* getUint32();
		static PrimitiveType* getUint64();
		static PrimitiveType* getUint128();

		static PrimitiveType* getFloat32();
		static PrimitiveType* getFloat64();
		static PrimitiveType* getFloat128();

		static PointerType* getInt8Ptr();
		static PointerType* getInt16Ptr();
		static PointerType* getInt32Ptr();
		static PointerType* getInt64Ptr();
		static PointerType* getInt128Ptr();

		static PointerType* getUint8Ptr();
		static PointerType* getUint16Ptr();
		static PointerType* getUint32Ptr();
		static PointerType* getUint64Ptr();
		static PointerType* getUint128Ptr();

		static PointerType* getMutInt8Ptr();
		static PointerType* getMutInt16Ptr();
		static PointerType* getMutInt32Ptr();
		static PointerType* getMutInt64Ptr();
		static PointerType* getMutInt128Ptr();

		static PointerType* getMutUint8Ptr();
		static PointerType* getMutUint16Ptr();
		static PointerType* getMutUint32Ptr();
		static PointerType* getMutUint64Ptr();
		static PointerType* getMutUint128Ptr();

		static ArraySliceType* getCharSlice(bool mut);
		static StringType* getString();
		static RangeType* getRange();

		static AnyType* getAny();

		static PrimitiveType* getNativeWord();
		static PrimitiveType* getNativeUWord();

		static PointerType* getNativeWordPtr();


		virtual ~Type() { }
		const TypeKind kind;

		protected:
		Type(TypeKind k) : kind(k)
		{
			static size_t __id = 0;
			this->id = __id++;
		}


		// base things
		size_t id = 0;

		PointerType* pointerTo = 0;
		PointerType* mutablePointerTo = 0;

		static Type* getOrCreateFloatingTypeWithConstraints(size_t bits);
		static Type* getOrCreateIntegerTypeWithConstraints(bool issigned, size_t bits);
		static Type* getOrCreateArrayTypeWithConstraints(size_t arrsize, Type* elm);
		static Type* getOrCreateStructTypeWithConstraints(bool islit, std::string name,
			std::vector<Type*> mems);

		static Type* getOrCreateFunctionTypeWithConstraints(bool isva, std::vector<Type*> args,
			Type* ret);
	};


















	struct BoolType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~BoolType() override { }

		BoolType();
		protected:

		public:
		static BoolType* get();
	};

	struct VoidType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~VoidType() override { }
		VoidType();
		protected:

		public:
		static VoidType* get();
	};


	struct NullType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~NullType() override { }
		NullType();
		protected:

		public:
		static NullType* get();
	};


	// special case -- the type also needs to store the number, to know things like
	// whether it's signed, negative, an integer, and other stuff.
	struct ConstantNumberType : Type
	{
		friend struct Type;

		bool isSigned();
		bool isFloating();
		size_t getMinBits();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		static ConstantNumberType* get(bool neg, bool flt, size_t bits);

		virtual ~ConstantNumberType() override { }


		protected:
		ConstantNumberType(bool neg, bool floating, size_t bits);

		bool _floating = false;
		bool _signed = false;
		size_t _bits = 0;
	};


	struct PolyPlaceholderType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		std::string getName();
		int getGroup();

		// session allows placeholders to share a name while being unrelated.
		static PolyPlaceholderType* get(const std::string& name, int session);
		virtual ~PolyPlaceholderType() override { }

		protected:
		PolyPlaceholderType(const std::string& n, int ses);

		std::string name;
		int group = 0;
	};





	struct PrimitiveType : Type
	{
		friend struct Type;

		// methods
		bool isSigned();
		size_t getIntegerBitWidth();
		size_t getFloatingPointBitWidth();
		PrimitiveType* getOppositeSignedType();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;


		enum class Kind
		{
			Invalid,

			Integer,
			Floating,
		};


		// protected constructor
		virtual ~PrimitiveType() override { }

		protected:
		PrimitiveType(size_t bits, bool issigned, Kind _kind);


		// fields (protected)
		bool isTypeSigned = 0;
		size_t bitWidth = 0;

		Kind primKind = Kind::Invalid;

		static PrimitiveType* getIntWithBitWidthAndSignage(size_t bits, bool issigned);
		static PrimitiveType* getFloatWithBitWidth(size_t bits);


		public:

		static PrimitiveType* getIntN(size_t bits);
		static PrimitiveType* getUintN(size_t bits);

		static PrimitiveType* getInt8();
		static PrimitiveType* getInt16();
		static PrimitiveType* getInt32();
		static PrimitiveType* getInt64();
		static PrimitiveType* getInt128();

		static PrimitiveType* getUint8();
		static PrimitiveType* getUint16();
		static PrimitiveType* getUint32();
		static PrimitiveType* getUint64();
		static PrimitiveType* getUint128();

		static PrimitiveType* getFloat32();
		static PrimitiveType* getFloat64();
		static PrimitiveType* getFloat128();
	};



	struct PointerType : Type
	{
		friend struct Type;

		virtual bool isTypeEqual(Type* other) override;

		PointerType* getMutable();
		PointerType* getImmutable();

		bool isMutable();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~PointerType() override { }
		protected:
		PointerType(Type* base, bool mut);

		Type* baseType = 0;
		bool isPtrMutable = false;

		// static funcs
		public:

		static PointerType* getInt8Ptr();
		static PointerType* getInt16Ptr();
		static PointerType* getInt32Ptr();
		static PointerType* getInt64Ptr();
		static PointerType* getInt128Ptr();

		static PointerType* getUint8Ptr();
		static PointerType* getUint16Ptr();
		static PointerType* getUint32Ptr();
		static PointerType* getUint64Ptr();
		static PointerType* getUint128Ptr();
	};



	struct TupleType : Type
	{
		friend struct Type;

		// methods
		size_t getElementCount();
		Type* getElementN(size_t n);
		const std::vector<Type*>& getElements();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~TupleType() override { }
		protected:
		TupleType(const std::vector<Type*>& mems);

		// fields (protected)
		std::vector<Type*> members;

		public:
		static TupleType* get(const std::initializer_list<Type*>& members);
		static TupleType* get(const std::vector<Type*>& members);
	};

	struct UnionVariantType;
	struct UnionType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		Identifier getTypeName();

		size_t getVariantCount();
		size_t getIdOfVariant(const std::string& name);
		const util::hash_map<std::string, UnionVariantType*>& getVariants();

		bool hasVariant(const std::string& name);
		UnionVariantType* getVariant(const std::string& name);
		UnionVariantType* getVariant(size_t id);
		void setBody(const util::hash_map<std::string, std::pair<size_t, Type*>>& variants);

		virtual ~UnionType() override { }
		protected:

		UnionType(const Identifier& id, const util::hash_map<std::string, std::pair<size_t, Type*>>& variants);

		Identifier unionName;
		util::hash_map<size_t, UnionVariantType*> indexMap;
		util::hash_map<std::string, UnionVariantType*> variants;

		public:
		static UnionType* create(const Identifier& id, const util::hash_map<std::string, std::pair<size_t, Type*>>& variants);
		static UnionType* createWithoutBody(const Identifier& id);
	};


	struct UnionVariantType : Type
	{
		friend struct Type;
		friend struct UnionType;

		// methods
		std::string getName() { return this->name; }
		size_t getVariantId() { return this->variantId; }
		Type* getInteriorType() { return this->interiorType; }
		UnionType* getParentUnion() { return this->parent; }

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~UnionVariantType() override { }

		protected:
		UnionVariantType(UnionType* parent, size_t id, const std::string& name, Type* actual);

		// fields (protected)
		UnionType* parent;
		Type* interiorType;
		std::string name;
		size_t variantId;
	};



	struct RawUnionType : Type
	{
		friend struct Type;

		Identifier getTypeName();
		size_t getVariantCount();

		bool hasVariant(const std::string& name);
		Type* getVariant(const std::string& name);
		const util::hash_map<std::string, Type*>& getVariants();

		void setBody(const util::hash_map<std::string, Type*>& variants);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;


		virtual ~RawUnionType() override { }
		protected:

		RawUnionType(const Identifier& id, const util::hash_map<std::string, Type*>& variants);

		Identifier unionName;
		util::hash_map<std::string, Type*> variants;

		public:
		static RawUnionType* create(const Identifier& id, const util::hash_map<std::string, Type*>& variants);
		static RawUnionType* createWithoutBody(const Identifier& id);
	};



	struct TraitType : Type
	{
		friend struct Type;

		// methods
		Identifier getTypeName();
		size_t getMethodCount();
		const std::vector<std::pair<std::string, FunctionType*>>& getMethods();
		void setMethods(const std::vector<std::pair<std::string, FunctionType*>>& m);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~TraitType() override { }
		protected:
		TraitType(const Identifier& name, const std::vector<std::pair<std::string, FunctionType*>>& meths);

		// fields
		Identifier traitName;
		std::vector<std::pair<std::string, FunctionType*>> methods;

		// static funcs
		public:
		static TraitType* create(const Identifier& name);
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
		const std::vector<Type*>& getElements();

		void setBody(const std::vector<std::pair<std::string, Type*>>& members);

		void addTraitImpl(TraitType* trt);
		bool implementsTrait(TraitType* trt);
		std::vector<TraitType*> getImplementedTraits();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~StructType() override { }
		protected:
		StructType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, bool ispacked);

		// fields (protected)
		bool isTypePacked;
		Identifier structName;
		std::vector<Type*> typeList;
		std::vector<TraitType*> implTraits;
		util::hash_map<std::string, size_t> indexMap;
		util::hash_map<std::string, Type*> structMembers;

		// static funcs
		public:
		static StructType* createWithoutBody(const Identifier& name, bool isPacked = false);
		static StructType* create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members,
			bool isPacked = false);
	};





	struct ClassType : Type
	{
		friend struct Type;
		friend struct Module;

		// methods
		Identifier getTypeName();
		size_t getElementCount();
		// Type* getElementN(size_t n);
		Type* getElement(const std::string& name);
		bool hasElementWithName(const std::string& name);
		size_t getAbsoluteElementIndex(const std::string& name);
		const std::vector<Type*>& getElements();
		std::vector<Type*> getAllElementsIncludingBase();
		const std::vector<std::string>& getNameList();

		const util::hash_map<std::string, size_t>& getElementNameMap();

		const std::vector<Function*>& getMethods();
		std::vector<Function*> getMethodsWithName(const std::string& id);
		Function* getMethodWithType(FunctionType* ftype);

		const std::vector<Function*>& getInitialiserFunctions();
		void setInitialiserFunctions(const std::vector<Function*>& list);

		Function* getInlineInitialiser();
		void setInlineInitialiser(Function* fn);

		Function* getInlineDestructor();
		void setInlineDestructor(Function* fn);

		void setMembers(const std::vector<std::pair<std::string, Type*>>& members);
		void setMethods(const std::vector<Function*>& methods);

		ClassType* getBaseClass();
		void setBaseClass(ClassType* ty);

		void setDestructor(Function* f);
		void setCopyConstructor(Function* f);
		void setMoveConstructor(Function* f);

		Function* getDestructor();
		Function* getCopyConstructor();
		Function* getMoveConstructor();

		void addTraitImpl(TraitType* trt);
		bool implementsTrait(TraitType* trt);
		std::vector<TraitType*> getImplementedTraits();

		bool hasParent(Type* base);

		void addVirtualMethod(Function* method);
		size_t getVirtualMethodIndex(const std::string& name, FunctionType* ft);

		size_t getVirtualMethodCount();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~ClassType() override { }
		protected:
		ClassType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, const std::vector<Function*>& methods,
			const std::vector<Function*>& inits);


		// fields (protected)
		Identifier className;
		std::vector<Type*> typeList;
		std::vector<std::string> nameList;
		std::vector<Function*> methodList;
		std::vector<Function*> initialiserList;

		std::vector<TraitType*> implTraits;

		util::hash_map<std::string, size_t> indexMap;
		util::hash_map<std::string, Type*> classMembers;
		util::hash_map<std::string, std::vector<Function*>> classMethodMap;

		//* how it works is that we will add in the mappings from the base class,
		//* and for our own matching virtual methods, we'll map to the same index.


		size_t virtualMethodCount = 0;
		// util::hash_map<Function*, size_t> virtualMethodMap;
		util::hash_map<size_t, Function*> reverseVirtualMethodMap;

		//* note: we do it this way (where we *EXCLUDE THE SELF POINTER*), because it's just easier -- to compare, and everything.
		//* we really don't have a use for mapping a Function to an index, only the other way.
		std::map<std::pair<std::string, std::vector<Type*>>, size_t> virtualMethodMap;

		ClassType* baseClass = 0;

		Function* inlineInitialiser = 0;
		Function* inlineDestructor = 0;

		Function* destructor = 0;
		Function* copyConstructor = 0;
		Function* moveConstructor = 0;

		// static funcs
		public:
		static ClassType* createWithoutBody(const Identifier& name);
		static ClassType* create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members,
			const std::vector<Function*>& methods, const std::vector<Function*>& inits);
	};


	struct EnumType : Type
	{
		friend struct Type;

		Type* getCaseType();
		Identifier getTypeName();

		ConstantValue* getNameArray();
		ConstantValue* getCaseArray();

		void setNameArray(ConstantValue* arr);
		void setCaseArray(ConstantValue* arr);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~EnumType() override { }
		protected:
		EnumType(const Identifier& name, Type* ty);

		Type* caseType;
		Identifier typeName;

		ConstantValue* runtimeNameArray = 0;
		ConstantValue* runtimeCasesArray = 0;

		// static funcs
		public:
		static EnumType* get(const Identifier& name, Type* caseType);
		static EnumType* getEmpty();
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
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~ArrayType() override { }
		protected:
		ArrayType(Type* elmType, size_t sz);

		// fields (protected)
		size_t arraySize;
		Type* arrayElementType;

		// static funcs
		public:
		static ArrayType* get(Type* elementType, size_t num);
	};


	struct DynamicArrayType : Type
	{
		friend struct Type;

		// methods
		Type* getElementType();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~DynamicArrayType() override { }
		protected:
		DynamicArrayType(Type* elmType);

		// fields
		Type* arrayElementType;

		// static funcs
		public:
		static DynamicArrayType* get(Type* elementType);
	};


	struct ArraySliceType : Type
	{
		friend struct Type;

		// methods
		Type* getElementType();

		bool isMutable();
		bool isVariadicType();

		// simplifies the mutability checking and stuff.
		Type* getDataPointerType();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~ArraySliceType() override { }
		protected:
		ArraySliceType(Type* elmType, bool mut);

		// fields
		bool isSliceMutable;
		Type* arrayElementType;

		bool isVariadic = false;

		// static funcs
		public:
		static ArraySliceType* get(Type* elementType, bool mut);
		static ArraySliceType* getMutable(Type* elementType);
		static ArraySliceType* getImmutable(Type* elementType);

		static ArraySliceType* getVariadic(Type* elementType);
	};




	struct FunctionType : Type
	{
		friend struct Type;

		// methods
		const std::vector<Type*>& getArgumentTypes();
		size_t getArgumentCount();
		Type* getArgumentN(size_t n);
		Type* getReturnType();

		bool isCStyleVarArg();
		bool isVariadicFunc();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~FunctionType() override { }
		protected:
		FunctionType(const std::vector<Type*>& args, Type* ret, bool iscva);

		// fields (protected)
		bool isFnCStyleVarArg;

		std::vector<Type*> functionParams;
		Type* functionRetType;

		// static funcs
		public:
		static FunctionType* getCVariadicFunc(const std::vector<Type*>& args, Type* ret);
		static FunctionType* getCVariadicFunc(const std::initializer_list<Type*>& args, Type* ret);

		static FunctionType* get(const std::vector<Type*>& args, Type* ret);
		static FunctionType* get(const std::initializer_list<Type*>& args, Type* ret);
	};


	struct RangeType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;


		// protected constructor
		virtual ~RangeType() override { }
		protected:
		RangeType();

		public:
		static RangeType* get();
	};


	struct StringType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;


		// protected constructor
		virtual ~StringType() override { }
		protected:
		StringType();

		public:
		static StringType* get();
	};



	struct AnyType : Type
	{
		friend struct Type;

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;


		// protected constructor
		virtual ~AnyType() override { }
		protected:
		AnyType();

		public:
		static AnyType* get();
	};


	struct OpaqueType : Type
	{
		friend struct Type;


		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		size_t getTypeSizeInBits() { return this->typeSizeInBits; }

		// protected constructor
		virtual ~OpaqueType() override { }
		protected:
		OpaqueType(const std::string& name, size_t sizeInBits);

		std::string typeName;
		size_t typeSizeInBits;

		public:
		static OpaqueType* get(const std::string& name, size_t sizeInBits);
	};






	struct LocatedType
	{
		LocatedType() { }
		explicit LocatedType(Type* t) : type(t) { }
		LocatedType(Type* t, const Location& l) : type(t), loc(l) { }

		operator Type* () const { return this->type; }
		Type* operator -> () const { return this->type; }

		Type* type = 0;
		Location loc;
	};








	struct HashTypeByStr
	{
		size_t operator() (Type* t) const
		{
			return std::hash<std::string>()(t->str());
		}
	};

	struct TypesEqual
	{
		bool operator() (Type* a, Type* b) const
		{
			return a->isTypeEqual(b);
		}
	};

	struct TypeCache
	{
		std::unordered_set<Type*, HashTypeByStr, TypesEqual> cache;

		template <typename T>
		T* getOrAddCachedType(T* type)
		{
			if(auto it = cache.find(type); it != cache.end())
			{
				delete type;
				return dynamic_cast<T*>(*it);
			}
			else
			{
				cache.insert(type);
				return type;
			}
		}

		static TypeCache& get();
	};
}
























