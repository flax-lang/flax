// pts.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "precompile.h"

namespace ast
{
	struct Expr;
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
	struct ArraySliceType;
	struct FixedArrayType;
	struct DynamicArrayType;
	struct VariadicArrayType;
	struct FunctionType;



	struct Type
	{
		virtual ~Type() { }
		virtual std::string str();

		NamedType* toNamedType();
		TupleType* toTupleType();
		PointerType* toPointerType();
		FunctionType* toFunctionType();
		FixedArrayType* toFixedArrayType();
		ArraySliceType* toArraySliceType();
		DynamicArrayType* toDynamicArrayType();
		VariadicArrayType* toVariadicArrayType();

		bool isNamedType();
		bool isTupleType();
		bool isPointerType();
		bool isFunctionType();
		bool isArraySliceType();
		bool isFixedArrayType();
		bool isDynamicArrayType();
		bool isVariadicArrayType();

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

		std::unordered_map<std::string, Type*> genericMapping;

		static NamedType* create(const std::string& s);
		static NamedType* create(const std::string& s, const std::map<std::string, Type*>& genericMapping);

		private:
		explicit NamedType(const std::string& n) : name(n) { }
	};


	struct PointerType : Type
	{
		virtual ~PointerType() { }
		explicit PointerType(pts::Type* b, bool mut) : base(b), isMutable(mut) { }
		virtual std::string str() override;

		pts::Type* base = 0;
		bool isMutable = false;
	};


	struct TupleType : Type
	{
		virtual ~TupleType() { }
		explicit TupleType(const std::vector<pts::Type*>& ts) : types(ts) { }
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


	struct ArraySliceType : Type
	{
		virtual ~ArraySliceType() { }
		explicit ArraySliceType(pts::Type* b, bool m) : base(b), mut(m) { }
		virtual std::string str() override;

		pts::Type* base = 0;
		bool mut = false;
	};


	struct FunctionType : Type
	{
		virtual ~FunctionType() { }
		explicit FunctionType(const std::vector<pts::Type*>& args, pts::Type* ret) : argTypes(args), returnType(ret) { }
		virtual std::string str() override;

		std::unordered_map<std::string, TypeConstraints_t> genericTypes;
		std::vector<pts::Type*> argTypes;
		pts::Type* returnType = 0;
	};
}


















