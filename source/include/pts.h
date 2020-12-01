// pts.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "precompile.h"

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

		Location loc;

		protected:
			Type(const Location& l) : loc(l) { }
	};


	struct InferredType : Type
	{
		virtual ~InferredType() { }
		InferredType(const Location& l) : Type(l) { }

		virtual std::string str() override { return "?"; }


		static InferredType* get();
	};


	struct NamedType : Type
	{
		virtual ~NamedType() { }

		virtual std::string str() override;
		std::string name;

		PolyArgMapping_t genericMapping;

		static NamedType* create(const Location& l, const std::string& s);
		static NamedType* create(const Location& l, const std::string& s, const PolyArgMapping_t& genericMapping);

		// private:
		explicit NamedType(const Location& l, const std::string& n) : Type(l), name(n) { }
	};


	struct PointerType : Type
	{
		virtual ~PointerType() { }
		explicit PointerType(const Location& l, pts::Type* b, bool mut) : Type(l), base(b), isMutable(mut) { }
		virtual std::string str() override;

		pts::Type* base = 0;
		bool isMutable = false;
	};


	struct TupleType : Type
	{
		virtual ~TupleType() { }
		explicit TupleType(const Location& l, const std::vector<pts::Type*>& ts) : Type(l), types(ts) { }
		virtual std::string str() override;

		std::vector<pts::Type*> types;
	};


	// int[x], where x *is* a literal (eg. 4, 10, 0xF00D)
	struct FixedArrayType : Type
	{
		virtual ~FixedArrayType() { }
		explicit FixedArrayType(const Location& l, pts::Type* b, size_t s) : Type(l), base(b), size(s) { }
		virtual std::string str() override;

		pts::Type* base = 0;
		size_t size = 0;
	};


	// int[x], where x is *not* a literal
	struct DynamicArrayType : Type
	{
		virtual ~DynamicArrayType() { }
		explicit DynamicArrayType(const Location& l, pts::Type* b) : Type(l), base(b) { }
		virtual std::string str() override;

		pts::Type* base = 0;
	};


	struct VariadicArrayType : Type
	{
		virtual ~VariadicArrayType() { }
		explicit VariadicArrayType(const Location& l, pts::Type* b) : Type(l), base(b) { }
		virtual std::string str() override;

		pts::Type* base = 0;
	};


	struct ArraySliceType : Type
	{
		virtual ~ArraySliceType() { }
		explicit ArraySliceType(const Location& l, pts::Type* b, bool m) : Type(l), base(b), mut(m) { }
		virtual std::string str() override;

		pts::Type* base = 0;
		bool mut = false;
	};


	struct FunctionType : Type
	{
		virtual ~FunctionType() { }
		explicit FunctionType(const Location& l, const std::vector<pts::Type*>& args, pts::Type* ret) : Type(l), argTypes(args), returnType(ret) { }
		virtual std::string str() override;

		util::hash_map<std::string, TypeConstraints_t> genericTypes;
		std::vector<pts::Type*> argTypes;
		pts::Type* returnType = 0;
	};
}


















