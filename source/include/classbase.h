// classbase.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "ir/type.h"
#include "ir/function.h"

namespace Ast
{
	struct ClassDef;
	struct Func;
}

namespace Codegen
{
	struct CodegenInstance;

	std::map<Ast::Func*, fir::Function*> doCodegenForMemberFunctions(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType,
		std::map<std::string, fir::Type*> tm);

	void doCodegenForComputedProperties(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType,
		std::map<std::string, fir::Type*> tm);

	void generateDeclForOperators(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType,
		std::map<std::string, fir::Type*> tm);

	void doCodegenForGeneralOperators(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm);
	void doCodegenForSubscriptOperators(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm);
	void doCodegenForAssignmentOperators(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType, std::map<std::string, fir::Type*> tm);

	void generateMemberFunctionBody(CodegenInstance* cgi, Ast::ClassDef* cls, fir::Type* clsType, Ast::Func* fn,
		fir::Function* defaultInitFunc, fir::Function* firdecl, std::map<std::string, fir::Type*> tm);
}
