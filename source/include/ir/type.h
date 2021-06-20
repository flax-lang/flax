// type.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "precompile.h"

namespace fir
{
	enum class NameKind
	{
		Name,
		Type,
		Function
	};

	struct Type;

	struct Name
	{
		NameKind kind;

		std::string name;
		std::vector<std::string> scope;
		std::vector<fir::Type*> params;
		fir::Type* retty;

		std::string str() const;
		std::string mangled() const;
		std::string mangledWithoutScope() const;

		bool operator== (const Name& other) const;
		bool operator!= (const Name& other) const;

		static Name of(std::string name);
		static Name of(std::string name, std::vector<std::string> scope);

		static Name type(std::string name);
		static Name type(std::string name, std::vector<std::string> scope);

		static Name function(std::string name, std::vector<fir::Type*> params, fir::Type* retty);
		static Name function(std::string name, std::vector<std::string> scope, std::vector<fir::Type*> params, fir::Type* retty);

		static Name obfuscate(const std::string& name, NameKind kind = NameKind::Name);
		static Name obfuscate(const std::string& name, size_t id, NameKind kind = NameKind::Name);
		static Name obfuscate(const std::string& name, const std::string& extra, NameKind kind = NameKind::Name);

	private:
		Name(NameKind kind, std::string name, std::vector<std::string> scope, std::vector<fir::Type*> params, fir::Type* retty)
			: kind(kind), name(std::move(name)), scope(std::move(scope)), params(std::move(params)), retty(retty) { }
	};

	std::string obfuscateName(const std::string& name);
	std::string obfuscateName(const std::string& name, size_t id);
	std::string obfuscateName(const std::string& name, const std::string& extra);
}

template <>
struct zpr::print_formatter<fir::Name>
{
	std::string print(const fir::Name& x, const format_args& args);
};

namespace std
{
	template<>
	struct hash<fir::Name>
	{
		std::size_t operator()(const fir::Name& k) const
		{
			using std::size_t;
			using std::hash;
			using std::string;

			// Compute individual hash values for first,
			// second and third and combine them using XOR
			// and bit shifting:

			// return ((hash<string>()(k.name) ^ (hash<std::vector<std::string>>()(k.scope) << 1)) >> 1) ^ (hash<int>()(k.third) << 1);
			return hash<string>()(k.str());
		}
	};
}


namespace fir
{
	// NOTE: i don't really want to deal with inheritance stuff right now,
	// so Type will encapsulate everything.
	// we shouldn't be making any copies anyway, so space/performance is a negligible concern

	struct Type;
	struct Module;
	struct BoolType;
	struct EnumType;
	struct NullType;
	struct VoidType;
	struct ArrayType;
	struct RangeType;
	struct TraitType;
	struct TupleType;
	struct UnionType;
	struct StructType;
	struct OpaqueType;
	struct PointerType;
	struct FunctionType;
	struct RawUnionType;
	struct PrimitiveType;
	struct ArraySliceType;
	struct UnionVariantType;
	struct PolyPlaceholderType;

	struct ConstantValue;
	struct ConstantArray;
	struct Function;

	int getCastDistance(Type* from, Type* to);

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

		Null,
		Void,
		Enum,
		Bool,
		Array,
		Tuple,
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
		UnionVariant,
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
		UnionVariantType* toUnionVariantType();
		ArraySliceType* toArraySliceType();
		PrimitiveType* toPrimitiveType();
		RawUnionType* toRawUnionType();
		FunctionType* toFunctionType();
		PointerType* toPointerType();
		OpaqueType* toOpaqueType();
		StructType* toStructType();
		TraitType* toTraitType();
		RangeType* toRangeType();
		UnionType* toUnionType();
		TupleType* toTupleType();
		ArrayType* toArrayType();
		BoolType* toBoolType();
		EnumType* toEnumType();
		NullType* toNullType();

		bool isPointerTo(Type* other);
		bool isPointerElementOf(Type* other);

		bool isTraitType();
		bool isUnionType();
		bool isTupleType();
		bool isStructType();
		bool isPackedStruct();
		bool isRawUnionType();
		bool isUnionVariantType();

		bool isRangeType();

		bool isCharType();

		bool isOpaqueType();

		bool isEnumType();
		bool isArrayType();
		bool isIntegerType();
		bool isFunctionType();
		bool isSignedIntType();
		bool isUnsignedIntType();
		bool isFloatingPointType();

		bool isArraySliceType();
		bool isVariadicArrayType();

		bool isCharSliceType();

		bool isPrimitiveType();
		bool isPointerType();
		bool isVoidType();
		bool isNullType();
		bool isBoolType();

		bool isMutablePointer();
		bool isImmutablePointer();
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
		static RangeType* getRange();

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

		Name getTypeName();

		size_t getVariantCount();
		size_t getIdOfVariant(const std::string& name);
		const util::hash_map<std::string, UnionVariantType*>& getVariants();

		bool hasVariant(const std::string& name);
		UnionVariantType* getVariant(const std::string& name);
		UnionVariantType* getVariant(size_t id);
		void setBody(const util::hash_map<std::string, std::pair<size_t, Type*>>& variants);

		virtual ~UnionType() override { }
		protected:

		UnionType(const Name& id, const util::hash_map<std::string, std::pair<size_t, Type*>>& variants);

		Name unionName;
		util::hash_map<size_t, UnionVariantType*> indexMap;
		util::hash_map<std::string, UnionVariantType*> variants;

		public:
		static UnionType* create(const Name& id, const util::hash_map<std::string, std::pair<size_t, Type*>>& variants);
		static UnionType* createWithoutBody(const Name& id);
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

		Name getTypeName();
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

		RawUnionType(const Name& id, const util::hash_map<std::string, Type*>& variants);

		Name unionName;
		util::hash_map<std::string, Type*> variants;

		public:
		static RawUnionType* create(const Name& id, const util::hash_map<std::string, Type*>& variants);
		static RawUnionType* createWithoutBody(const Name& id);
	};



	struct TraitType : Type
	{
		friend struct Type;

		// methods
		Name getTypeName();
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
		TraitType(const Name& name, const std::vector<std::pair<std::string, FunctionType*>>& meths);

		// fields
		Name traitName;
		std::vector<std::pair<std::string, FunctionType*>> methods;

		// static funcs
		public:
		static TraitType* create(const Name& name);
	};



	struct StructType : Type
	{
		friend struct Type;

		// methods
		Name getTypeName();
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
		const util::hash_map<std::string, size_t>& getIndexMap();

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~StructType() override { }
		protected:
		StructType(const Name& name, const std::vector<std::pair<std::string, Type*>>& mems, bool ispacked);

		// fields (protected)
		bool isTypePacked;
		Name structName;
		std::vector<Type*> typeList;
		std::vector<TraitType*> implTraits;
		util::hash_map<std::string, size_t> indexMap;
		util::hash_map<std::string, Type*> structMembers;

		// static funcs
		public:
		static StructType* createWithoutBody(const Name& name, bool isPacked = false);
		static StructType* create(const Name& name, const std::vector<std::pair<std::string, Type*>>& members,
			bool isPacked = false);
	};







	struct EnumType : Type
	{
		friend struct Type;

		Type* getCaseType();
		Name getTypeName();

		ConstantValue* getNameArray();
		ConstantValue* getCaseArray();

		void setCaseType(Type* t);
		void setNameArray(ConstantValue* arr);
		void setCaseArray(ConstantValue* arr);

		virtual std::string str() override;
		virtual std::string encodedStr() override;
		virtual bool isTypeEqual(Type* other) override;
		virtual Type* substitutePlaceholders(const util::hash_map<Type*, Type*>& subst) override;

		// protected constructor
		virtual ~EnumType() override { }
		protected:
		EnumType(const Name& name, Type* ty);

		Type* caseType;
		Name typeName;

		ConstantValue* runtimeNameArray = 0;
		ConstantValue* runtimeCasesArray = 0;

		// static funcs
		public:
		static EnumType* get(const Name& name, Type* caseType);
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
























