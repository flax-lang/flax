// FuncDeclCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Function.h"

using namespace Ast;
using namespace Codegen;

llvm::GlobalValue::LinkageTypes CodegenInstance::getFunctionDeclLinkage(FuncDecl* fd)
{
	llvm::GlobalValue::LinkageTypes linkageType;

	if(fd->isFFI)
	{
		linkageType = llvm::Function::ExternalLinkage;
	}
	else if((fd->attribs & Attr_VisPrivate) || (fd->attribs & Attr_VisInternal))
	{
		linkageType = llvm::Function::InternalLinkage;
	}
	else if(fd->attribs & Attr_VisPublic)
	{
		linkageType = llvm::Function::ExternalLinkage;
	}
	else if(fd->parentClass && (fd->attribs & (Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate)) == 0)
	{
		// default.
		linkageType = (fd->attribs & Attr_VisPrivate) || (fd->attribs & Attr_VisInternal) ?
			llvm::Function::InternalLinkage : llvm::Function::ExternalLinkage;
	}
	else
	{
		linkageType = llvm::Function::InternalLinkage;
	}


	return linkageType;
}


Result_t CodegenInstance::generateActualFuncDecl(FuncDecl* fd, std::vector<llvm::Type*> argtypes, llvm::Type* rettype)
{
	llvm::FunctionType* ft = llvm::FunctionType::get(rettype, argtypes, fd->hasVarArg);
	auto linkageType = this->getFunctionDeclLinkage(fd);

	// check for redef
	llvm::Function* func = nullptr;
	if(this->getType(fd->mangledName) != nullptr)
	{
		GenError::duplicateSymbol(this, fd, fd->name + " (symbol previously declared as a type)", SymbolType::Generic);
	}
	else if(fd->genericTypes.size() == 0 && this->isDuplicateFuncDecl(fd))
	{
		if(!fd->isFFI)
		{
			GenError::duplicateSymbol(this, fd, fd->name, SymbolType::Function);
		}
	}
	else
	{
		func = llvm::Function::Create(ft, linkageType, fd->mangledName, this->module);

		if(fd->attribs & Attr_VisPublic)
			this->addPublicFunc({ func, fd });

		this->addFunctionToScope({ func, fd });
	}


	return Result_t(func, 0);
}

Result_t FuncDecl::generateDeclForGenericType(CodegenInstance* cgi, std::map<std::string, llvm::Type*> types)
{
	iceAssert(types.size() == this->genericTypes.size());

	std::vector<llvm::Type*> argtypes;
	for(size_t i = 0; i < this->params.size(); i++)
	{
		VarDecl* v = this->params[i];
		llvm::Type* ltype = cgi->getLlvmType(v, true, false);	// allowFail = true, setInferred = false

		if(!ltype && types.find(v->type.strType) != types.end())
		{
			// provided.
			llvm::Type* vt = types[v->type.strType];
			argtypes.push_back(vt);
		}
		else
		{
			// either not a generic type, or not a legit type -- skip.
			argtypes.push_back(ltype);
		}
	}

	llvm::Type* lret = cgi->getLlvmType(this, true);
	if(!lret && types.find(this->type.strType) != types.end())
	{
		lret = types[this->type.strType];
	}

	return cgi->generateActualFuncDecl(this, argtypes, lret);
}





Result_t FuncDecl::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// if we're a generic function, we can't generate anything
	// wait until we get specific instances
	// (where all the typenames, T, U etc. have been replaced with concrete types by callers)

	if(this->genericTypes.size() > 0)
	{
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
					break;
				}
			}

			if(this->type.isLiteral && this->type.strType == gtype)
			{
				usage[gtype] = true;
			}
		}

		for(auto pair : usage)
		{
			if(!pair.second)
			{
				warn(cgi, this, "Generic type '%s' is unused", pair.first.c_str());
			}
		}
	}



	// check if empty and if it's an extern. mangle the name to include type info if possible.
	bool isMemberFunction = (this->parentClass != nullptr);
	bool isGeneric = this->genericTypes.size() > 0;

	this->mangledName = this->name;
	if(isMemberFunction)
	{
		iceAssert(!this->isFFI);
		std::deque<Expr*> es;
		for(auto p : this->params)
			es.push_back(p);

		this->mangledName = cgi->mangleMemberFunction(dynamic_cast<Class*>(this->parentClass), this->name, es);

		if(!this->isStatic)
		{
			// do a check.
			for(auto p : this->params)
			{
				if(p->name == "self")
					error(cgi, this, "Cannot have a parameter named 'self' in a method declaration");

				else if(p->name == "super")
					error(cgi, this, "Cannot have a parameter named 'super' in a method declaration");
			}

			VarDecl* implicit_self = new VarDecl(this->posinfo, "self", true);
			implicit_self->type = this->parentClass->mangledName + "*";
			this->params.push_front(implicit_self);
		}
	}
	else
	{
		bool alreadyMangled = false;

		// if we're a normal function, or we're ffi and the type is c++, mangle it
		// our mangling is compatible with c++ to reduce headache

		// if [ (not ffi) AND (not @nomangle) ] OR [ (is ffi) AND (ffi is cpp) ]
		if((!this->isFFI && !(this->attribs & Attr_NoMangle)) || (this->isFFI && this->ffiType == FFIType::Cpp))
		{
			alreadyMangled = true;

			bool isNested = false;
			if(Func* cfs = cgi->getCurrentFunctionScope())
			{
				isNested = true;
				cgi->pushNamespaceScope(cfs->decl->mangledName, false);
			}

			this->mangledNamespaceOnly = cgi->mangleWithNamespace(this->mangledName);
			this->mangledName = this->mangledNamespaceOnly;

			if(isGeneric)	this->mangledName = cgi->mangleGenericFunctionName(this->mangledName, this->params);
			else			this->mangledName = cgi->mangleFunctionName(this->mangledName, this->params);

			if(isNested)
			{
				cgi->popNamespaceScope();
			}
		}

		// if (not alreadyMangled) AND [ (not ffi) OR (@nomangle) ] AND (not @nomangle)
		if(!alreadyMangled && (!this->isFFI || this->attribs & Attr_ForceMangle) && !(this->attribs & Attr_NoMangle))
		{
			if(isGeneric)	this->mangledName = cgi->mangleGenericFunctionName(this->name, this->params);
			else			this->mangledName = cgi->mangleFunctionName(this->name, this->params);
		}
	}

	if(isGeneric)
	{
		cgi->rootNode->genericFunctions.push_back(this);
		cgi->addFunctionToScope({ 0, this });
		return Result_t(0, 0);
	}
	else
	{
		std::vector<llvm::Type*> argtypes;
		for(VarDecl* v : this->params)
			argtypes.push_back(cgi->getLlvmType(v));

		return cgi->generateActualFuncDecl(this, argtypes, cgi->getLlvmType(this));
	}
}









































Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	return this->decl->codegen(cgi);
}
