// classbase.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

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

	void doCodegenForMemberFunctions(CodegenInstance* cgi, Ast::ClassDef* cls);
	void doCodegenForComputedProperties(CodegenInstance* cgi, Ast::ClassDef* cls);

	void generateDeclForOperators(CodegenInstance* cgi, Ast::ClassDef* cls);

	void doCodegenForGeneralOperators(CodegenInstance* cgi, Ast::ClassDef* cls);
	void doCodegenForSubscriptOperators(CodegenInstance* cgi, Ast::ClassDef* cls);
	void doCodegenForAssignmentOperators(CodegenInstance* cgi, Ast::ClassDef* cls);

	void generateMemberFunctionBody(CodegenInstance* cgi, Ast::ClassDef* cls, Ast::Func* fn, fir::Function* defaultInitFunc);
}
