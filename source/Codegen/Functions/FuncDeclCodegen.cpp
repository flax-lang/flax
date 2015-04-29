// FuncDeclCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t FuncDecl::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// if we're a generic function, we can't generate anything
	// wait until we get specific instances
	// (where all the typenames, T, U etc. have been replaced with concrete types by callers)

	if(this->genericTypes.size() > 0)
	{
		bool usedAny = false;
		std::map<std::string, bool> usage;

		for(auto gtype : this->genericTypes)
		{
			// check if we actually use it.

			usage[gtype] = false;
			for(auto v : this->params)
			{
				if(v->type.isLiteral && v->type.strType == gtype)
				{
					usage[gtype] = true;
					usedAny = true;
					break;
				}
			}

			if(this->type.isLiteral && this->type.strType == gtype)
			{
				usage[gtype] = true;
				usedAny = true;
			}
		}



		for(auto pair : usage)
		{
			if(!pair.second)
			{
				warn(cgi, this, "Generic type '%s' is unused", pair.first.c_str());
			}
		}


		if(usedAny)
		{
			// defer generation, until all dependencies have been resolved.
			FuncPair_t fp;
			fp.first = 0;
			fp.second = this;

			cgi->addFunctionToScope(cgi->mangleWithNamespace(this->name), fp);

			return Result_t(0, 0);
		}
	}



	// check if empty and if it's an extern. mangle the name to include type info if possible.
	bool isMemberFunction = (this->parentStruct != nullptr);


	this->mangledName = this->name;
	if(isMemberFunction)
	{
		iceAssert(!this->isFFI);
		std::deque<Expr*> es;
		for(auto p : this->params)
			es.push_back(p);

		this->mangledName = cgi->mangleMemberFunction(this->parentStruct, this->name, es);

		if(!this->isStatic)
		{
			VarDecl* implicit_self = new VarDecl(this->posinfo, "self", true);
			implicit_self->type = this->parentStruct->mangledName + "*";
			this->params.push_front(implicit_self);
		}
	}
	else
	{
		bool alreadyMangled = false;

		// if we're a normal function, or we're ffi and the type is c++, mangle it
		// our mangling is compatible with c++ to reduce headache
		if((!this->isFFI && !(this->attribs & Attr_NoMangle)) || (this->isFFI && this->ffiType == FFIType::Cpp))
		{
			alreadyMangled = true;

			bool isNested = false;
			if(Func* cfs = cgi->getCurrentFunctionScope())
			{
				isNested = true;
				cgi->pushNamespaceScope(cfs->decl->mangledName);
			}

			this->mangledName = cgi->mangleWithNamespace(this->mangledName);
			this->mangledName = cgi->mangleName(this->mangledName, this->params);

			if(isNested)
			{
				cgi->popNamespaceScope();
			}
		}


		if(!alreadyMangled && (!this->isFFI || this->attribs & Attr_ForceMangle) && !(this->attribs & Attr_NoMangle))
			this->mangledName = cgi->mangleName(this->name, this->params);
	}

	std::vector<llvm::Type*> argtypes;
	for(VarDecl* v : this->params)
		argtypes.push_back(cgi->getLlvmType(v));

	llvm::FunctionType* ft = llvm::FunctionType::get(cgi->getLlvmType(this), argtypes, this->hasVarArg);
	llvm::GlobalValue::LinkageTypes linkageType;

	if(this->isFFI)
	{
		linkageType = llvm::Function::ExternalWeakLinkage;
	}
	else if((this->attribs & Attr_VisPrivate) || (this->attribs & Attr_VisInternal))
	{
		linkageType = llvm::Function::InternalLinkage;
	}
	else if(this->attribs & Attr_VisPublic)
	{
		linkageType = llvm::Function::ExternalLinkage;
	}
	else if(this->parentStruct && (this->attribs & (Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate)) == 0)
	{
		// default.
		linkageType = (this->attribs & Attr_VisPrivate) || (this->attribs & Attr_VisInternal) ? llvm::Function::InternalLinkage : llvm::Function::ExternalLinkage;
	}
	else
	{
		linkageType = llvm::Function::InternalLinkage;
	}



	// check for redef
	llvm::Function* func = nullptr;
	if(cgi->getType(this->mangledName) != nullptr)
	{
		GenError::duplicateSymbol(cgi, this, this->name + " (symbol previously declared as a type)", SymbolType::Generic);
	}
	else if(cgi->isDuplicateFuncDecl(this->mangledName) /*cgi->mainModule->getFunction(this->mangledName)*/)
	{
		if(!this->isFFI)
		{
			GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Function);
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

Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	return this->decl->codegen(cgi);
}
