// FuncDeclCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t FuncDecl::codegen(CodegenInstance* cgi)
{
	std::vector<llvm::Type*> argtypes;
	std::deque<Expr*> params_expr;
	for(VarDecl* v : this->params)
	{
		params_expr.push_back(v);
		argtypes.push_back(cgi->getLlvmType(v));
	}

	// check if empty and if it's an extern. mangle the name to include type info if possible.
	this->mangledName = this->name;
	if((!this->isFFI || this->attribs & Attr_ForceMangle) && !(this->attribs & Attr_NoMangle))
		this->mangledName = cgi->mangleName(this->name, params_expr);

	llvm::FunctionType* ft = llvm::FunctionType::get(cgi->getLlvmType(this), argtypes, this->hasVarArg);
	llvm::GlobalValue::LinkageTypes linkageType;

	if(this->isFFI)
		linkageType = llvm::Function::ExternalWeakLinkage;

	else if((this->attribs & Attr_VisPrivate) || (this->attribs & Attr_VisInternal))
		linkageType = llvm::Function::InternalLinkage;

	else
		linkageType = llvm::Function::ExternalLinkage;


	llvm::Function* func = llvm::Function::Create(ft, linkageType, this->mangledName, cgi->mainModule);


	// check for redef
	if(func->getName() != this->mangledName)
	{
		if(!this->isFFI)
		{
			error(this, "Redefinition of function '%s'", this->name.c_str());
		}
		else
		{
			// check for same name but different args
			// TODO: c++ compat
		}
	}

	cgi->addFunctionToScope(this->mangledName, FuncPair_t(func, this));

	if(this->attribs & Attr_VisPublic)
		cgi->getRootAST()->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(this, func));

	return Result_t(func, 0);
}

Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi)
{
	return this->decl->codegen(cgi);
}
