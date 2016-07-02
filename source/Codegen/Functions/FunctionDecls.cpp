// FuncDeclCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

static fir::LinkageType getFunctionDeclLinkage(FuncDecl* fd)
{
	fir::LinkageType linkageType;

	if(fd->isFFI)
	{
		linkageType = fir::LinkageType::External;
	}
	else if((fd->attribs & Attr_VisPrivate) || (fd->attribs & Attr_VisInternal))
	{
		linkageType = fir::LinkageType::Internal;
	}
	else if(fd->attribs & Attr_VisPublic)
	{
		linkageType = fir::LinkageType::External;
	}
	else if(fd->parentClass && (fd->attribs & (Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate)) == 0)
	{
		// default.
		linkageType = fd->attribs & Attr_VisPrivate ? fir::LinkageType::Internal : fir::LinkageType::Internal;
	}
	else
	{
		linkageType = fir::LinkageType::Internal;
	}


	return linkageType;
}


static Result_t generateActualFuncDecl(CodegenInstance* cgi, FuncDecl* fd, std::vector<fir::Type*> argtypes, fir::Type* rettype)
{
	fir::FunctionType* ft = 0;

	if(fd->isCStyleVarArg)
	{
		ft = fir::FunctionType::getCVariadicFunc(argtypes, rettype);
	}
	else
	{
		ft = fir::FunctionType::get(argtypes, rettype, fd->isVariadic);
	}


	auto linkageType = getFunctionDeclLinkage(fd);

	// check for redef
	fir::Function* func = nullptr;
	if(cgi->getType(fd->mangledName) != nullptr)
	{
		GenError::duplicateSymbol(cgi, fd, fd->name + " (symbol previously declared as a type)", SymbolType::Generic);
	}
	else if(fd->genericTypes.size() == 0 && cgi->module->getFunction(fd->mangledName) != 0)
	{
		if(!fd->isFFI)
		{
			GenError::duplicateSymbol(cgi, fd, fd->name, SymbolType::Function);
		}
	}
	else
	{
		func = cgi->module->getOrCreateFunction(fd->mangledName, ft, linkageType);

		if(fd->attribs & Attr_VisPublic)
			cgi->addPublicFunc({ func, fd });

		cgi->addFunctionToScope({ func, fd });
	}


	return Result_t(func, 0);
}





Result_t FuncDecl::generateDeclForGenericType(CodegenInstance* cgi, std::map<std::string, fir::Type*> types)
{
	iceAssert(types.size() == this->genericTypes.size());

	std::vector<fir::Type*> argtypes;
	for(size_t i = 0; i < this->params.size(); i++)
	{
		VarDecl* v = this->params[i];
		fir::Type* ltype = cgi->getExprType(v, true, false);	// allowFail = true, setInferred = false

		if(!ltype && types.find(v->type.strType) != types.end())
		{
			// provided.
			fir::Type* vt = types[v->type.strType];
			argtypes.push_back(vt);
		}
		else
		{
			// either not a generic type, or not a legit type -- skip.
			argtypes.push_back(ltype);
		}
	}

	fir::Type* lret = cgi->getExprType(this, true);
	if(!lret && types.find(this->type.strType) != types.end())
	{
		lret = types[this->type.strType];
	}

	this->mangledName = cgi->mangleGenericFunctionName(this->name, this->params);
	return generateActualFuncDecl(cgi, this, argtypes, lret);
}





Result_t FuncDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// if we're a generic function, we can't generate anything
	// wait until we get specific instances
	// (where all the typenames, T, U etc. have been replaced with concrete types by callers)


	if(this->isCStyleVarArg && (!this->isFFI || this->ffiType != FFIType::C))
		error(this, "C-style variadic arguments are only supported with C-style FFI function declarations.");


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
				warn(this, "Generic type '%s' is unused", pair.first.c_str());
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

		this->mangledName = cgi->mangleMemberFunction(this->parentClass, this->name, es);

		if(!this->isStatic)
		{
			// do a check.
			for(auto p : this->params)
			{
				if(p->ident.name == "self")
					error(this, "Cannot have a parameter named 'self' in a method declaration");

				else if(p->ident.name == "super")
					error(this, "Cannot have a parameter named 'super' in a method declaration");
			}

			VarDecl* implicit_self = new VarDecl(this->pin, "self", true);

			// todo: this is *** moderately *** IFFY
			// this is to handle generating the type when we need it
			std::string finalns;
			for(auto n : this->parentClass->scope)
				finalns += n + "::";

			finalns += this->parentClass->name + "*";
			implicit_self->type = finalns;

			// implicit_self->type = this->parentClass->name + "*";
			this->params.push_front(implicit_self);
		}
	}
	else
	{
		bool alreadyMangled = false;

		if(this->name == "main")
			this->attribs |= Attr_NoMangle;

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


	if(this->isVariadic)
		this->mangledName += "__VARIADIC";



	if(!isGeneric)
	{
		std::vector<fir::Type*> argtypes;
		for(VarDecl* v : this->params)
			argtypes.push_back(cgi->getExprType(v));

		return generateActualFuncDecl(cgi, this, argtypes, cgi->getExprType(this));
	}
	else
	{
		return Result_t(0, 0);
	}
}









































Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return this->decl->codegen(cgi);
}
