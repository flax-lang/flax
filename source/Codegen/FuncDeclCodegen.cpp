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
	// check if empty and if it's an extern. mangle the name to include type info if possible.
	bool isMemberFunction = (this->parentStruct != nullptr);


	this->mangledName = this->name;
	if(isMemberFunction)
	{
		assert(!this->isFFI);
		std::deque<Expr*> es;
		for(auto p : this->params)
			es.push_back(p);

		this->mangledName = cgi->mangleMemberFunction(this->parentStruct, this->name, es);

		VarDecl* implicit_self = new VarDecl(this->posinfo, "self", true);
		implicit_self->type = cgi->mangleWithNamespace(this->parentStruct->name) + "*";
		this->params.push_front(implicit_self);
	}
	else
	{
		bool alreadyMangled = false;
		if(!this->isFFI)
		{
			alreadyMangled = true;
			this->mangledName = cgi->mangleWithNamespace(this->mangledName);
			this->mangledName = cgi->mangleName(this->mangledName, this->params);
		}


		if(!alreadyMangled && (!this->isFFI || this->attribs & Attr_ForceMangle) && !(this->attribs & Attr_NoMangle))
			this->mangledName = cgi->mangleName(this->name, this->params);

		else if(!alreadyMangled && this->isFFI && this->ffiType == FFIType::Cpp)
			this->mangledName = cgi->mangleCppName(this->name, this->params);
	}

	std::vector<llvm::Type*> argtypes;
	for(VarDecl* v : this->params)
	{
		argtypes.push_back(cgi->getLlvmType(v));
	}




	llvm::FunctionType* ft = llvm::FunctionType::get(cgi->getLlvmType(this), argtypes, this->hasVarArg);
	llvm::GlobalValue::LinkageTypes linkageType;

	if(this->isFFI)
		linkageType = llvm::Function::ExternalWeakLinkage;

	else if((this->attribs & Attr_VisPrivate) || (this->attribs & Attr_VisInternal))
		linkageType = llvm::Function::InternalLinkage;

	else
		linkageType = llvm::Function::ExternalLinkage;



	// check for redef
	llvm::Function* func = nullptr;
	if(cgi->getType(this->mangledName) != nullptr)
	{
		GenError::duplicateSymbol(this, this->name + " (symbol previously declared as a type)", SymbolType::Generic);
	}
	else if(cgi->mainModule->getFunction(this->mangledName))
	{
		if(!this->isFFI)
		{
			GenError::duplicateSymbol(this, this->name, SymbolType::Function);
		}
	}
	else
	{
		func = llvm::Function::Create(ft, linkageType, this->mangledName, cgi->mainModule);
		cgi->addFunctionToScope(this->mangledName, FuncPair_t(func, this));
	}

	if(this->attribs & Attr_VisPublic)
		cgi->getRootAST()->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(this, func));

	return Result_t(func, 0);
}

Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi)
{
	return this->decl->codegen(cgi);
}
