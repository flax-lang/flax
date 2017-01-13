// pts.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stddef.h>

#include <string>
#include <vector>

#include "defs.h"

namespace Ast
{
	struct Expr;
}

namespace fir
{
	struct Type;
}

// Parser Type System
// why in the ever-living fuck does this even exist?
// type system upon type system...

// it's not meant to be full-fledged (nor transferrable)
// so i'm gonna use a bunch of shortcuts -- dynamic_cast everywhere, direct constructing, etc.
namespace pts
{
	struct Type;
	struct NamedType;
	struct PointerType;
	struct TupleType;
	struct FixedArrayType;
	struct DynamicArrayType;
	struct VariadicArrayType;
	struct FunctionType;


	struct TypeTransformer
	{
		enum class Type
		{
			None,
			Pointer,
			FixedArray,
			DynamicArray,
			VariadicArray
		};

		bool operator == (const TypeTransformer& other) const { return this->type == other.type && this->data == other.data; }
		bool operator != (const TypeTransformer& other) const { return !(*this == other); }


		TypeTransformer(Type t, size_t d) : type(t), data(d) { }

		Type type = Type::None;
		size_t data = 0;
	};


	fir::Type* applyTransformationsOnType(fir::Type* base, std::vector<TypeTransformer> trfs);

	bool areTransformationsCompatible(std::vector<TypeTransformer> a, std::vector<TypeTransformer> b);
	fir::Type* reduceMaximallyWithSubset(fir::Type* type, std::vector<TypeTransformer> a, std::vector<TypeTransformer> b);
	std::pair<fir::Type*, std::vector<TypeTransformer>> decomposeFIRTypeIntoBaseTypeWithTransformations(fir::Type* type);
	std::pair<pts::Type*, std::vector<TypeTransformer>> decomposeTypeIntoBaseTypeWithTransformations(pts::Type* type);


	struct Type
	{
		virtual ~Type() { }
		explicit Type(fir::Type* ft) : resolvedFType(ft) { }

		virtual std::string str();

		fir::Type* resolvedFType = 0;


		NamedType* toNamedType();
		PointerType* toPointerType();
		TupleType* toTupleType();
		FixedArrayType* toFixedArrayType();
		DynamicArrayType* toDynamicArrayType();
		VariadicArrayType* toVariadicArrayType();
		FunctionType* toFunctionType();


		bool isNamedType();
		bool isPointerType();
		bool isTupleType();
		bool isFixedArrayType();
		bool isDynamicArrayType();
		bool isVariadicArrayType();
		bool isFunctionType();

		protected:
			Type() { }
	};


	struct InferredType : Type
	{
		virtual ~InferredType() { }
		InferredType() { }

		virtual std::string str() override { return "?"; }


		static InferredType* get();
	};


	struct NamedType : Type
	{
		virtual ~NamedType() { }

		virtual std::string str() override;
		std::string name;


		static NamedType* create(std::string s);

		private:
		explicit NamedType(std::string n) : name(n) { }
	};


	struct PointerType : Type
	{
		virtual ~PointerType() { }
		explicit PointerType(pts::Type* b) : base(b) { }
		virtual std::string str() override;

		pts::Type* base = 0;
	};


	struct TupleType : Type
	{
		virtual ~TupleType() { }
		explicit TupleType(std::vector<pts::Type*> ts) : types(ts) { }
		virtual std::string str() override;

		std::vector<pts::Type*> types;
	};


	// int[x], where x *is* a literal (eg. 4, 10, 0xF00D)
	struct FixedArrayType : Type
	{
		virtual ~FixedArrayType() { }
		explicit FixedArrayType(pts::Type* b, size_t s) : base(b), size(s) { }
		virtual std::string str() override;

		pts::Type* base = 0;
		size_t size = 0;
	};


	// int[x], where x is *not* a literal
	struct DynamicArrayType : Type
	{
		virtual ~DynamicArrayType() { }
		explicit DynamicArrayType(pts::Type* b) : base(b) { }
		virtual std::string str() override;

		pts::Type* base = 0;
	};


	struct VariadicArrayType : Type
	{
		virtual ~VariadicArrayType() { }
		explicit VariadicArrayType(pts::Type* b) : base(b) { }
		virtual std::string str() override;

		pts::Type* base = 0;
	};


	struct FunctionType : Type
	{
		virtual ~FunctionType() { }
		explicit FunctionType(std::vector<pts::Type*> args, pts::Type* ret) : argTypes(args), returnType(ret) { }
		virtual std::string str() override;

		std::map<std::string, TypeConstraints_t> genericTypes;
		std::vector<pts::Type*> argTypes;
		pts::Type* returnType = 0;
	};




	pts::Type* parseType(std::string type);
}


















