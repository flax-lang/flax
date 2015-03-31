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
	enum class ExprType;
}

namespace TypeInfo
{
	void addNewType(Codegen::CodegenInstance* cgi, llvm::Type* stype, Ast::StructBase* str, Codegen::ExprType etype);
	void initialiseTypeInfo(Codegen::CodegenInstance* cgi);
	void generateTypeInfo(Codegen::CodegenInstance* cgi);
}























