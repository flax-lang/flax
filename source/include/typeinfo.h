// typeinfo.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <string>
#include <vector>

namespace llvm
{
	class Type;
}

namespace Ast
{
	struct StructBase;
}

namespace Codegen
{
	struct CodegenInstance;
}

namespace TypeInfo
{
	void addNewPrimitiveType(Codegen::CodegenInstance* cgi, llvm::Type* type);
	void addNewStructType(Codegen::CodegenInstance* cgi, llvm::Type* stype, Ast::StructBase* str);
	void initialiseTypeInfo(Codegen::CodegenInstance* cgi);
	void generateTypeInfo(Codegen::CodegenInstance* cgi);

	enum class TypeKind
	{
		Unknown		= 0,
		Integer		= 1,
		FloatingPt	= 2,
		Struct		= 3,
		Function	= 4,
	};

	struct Type
	{
		virtual ~Type() { }
		std::string name;
		TypeKind kind;
	};

	struct StructMemberType : Type
	{
		// name
		// kind

		uint64_t offset;
	};

	struct StructType : Type
	{
		// name
		// kind

		std::vector<StructMemberType> members;
	};

	struct IntegerType : Type
	{
		// name
		// kind

		uint8_t bits;
		bool isSigned;
	};

	struct FloatingPointType : Type
	{
		// name
		// kind

		bool isSinglePrecision;
	};

	struct FunctionType : Type
	{
		// name
		// kind
	};
}























