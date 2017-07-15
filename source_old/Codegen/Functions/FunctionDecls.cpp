// FuncDeclCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
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
	else if(fd->parentClass.first && (fd->attribs & (Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate)) == 0)
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


static Result_t generateActualFuncDecl(CodegenInstance* cgi, FuncDecl* fd, std::vector<fir::Type*> argtypes, fir::Type* rettype,
	bool mangle)
{
	fir::FunctionType* ft = 0;


	// add the things
	// note: the following won't affect non-generic functions.
	// this is to allow type normalisation with the function type *properly*
	std::vector<fir::ParametricType*> tparams;

	for(auto t : fd->genericTypes)
		tparams.push_back(fir::ParametricType::get(t.first));


	if(fd->isCStyleVarArg)
	{
		ft = fir::FunctionType::getCVariadicFunc(argtypes, rettype);
	}
	else
	{
		ft = fir::FunctionType::getWithTypeParameters(argtypes, rettype, fd->isVariadic, tparams);
	}


	auto linkageType = getFunctionDeclLinkage(fd);

	if(fd->genericTypes.empty())
		fd->ident.functionArguments = ft->getArgumentTypes();

	// check for redef
	fir::Function* func = nullptr;
	if(fd->genericTypes.size() == 0 && (/*cgi->isDuplicateFuncDecl(fd) || */cgi->module->getFunction(fd->ident) != 0))
	{
		GenError::duplicateSymbol(cgi, fd, fd->ident.str(), SymbolType::Function);
	}
	else
	{
		if(fd->genericTypes.size() == 0)
		{
			if(mangle)
			{
				iceAssert(!(fd->attribs & Attr_NoMangle) && !fd->isFFI);
				func = cgi->module->getOrCreateFunction(fd->ident, ft, linkageType);
			}
			else
			{
				auto id = Identifier(fd->ident.name, IdKind::Name);
				func = cgi->module->getOrCreateFunction(id, ft, linkageType);
			}
		}
		else
		{
			func = fir::Function::create(fd->ident, ft, cgi->module, linkageType);
		}


		// if(fd->attribs & Attr_VisPublic)
		// 	cgi->addPublicFunc(FuncDefPair(func, fd, 0));

		if(fd->parentClass.first == 0)
		{
			// if(fd->ident.name == "variadicTest")
			// {
			// 	info("%zu %s %p", fd->ident.scope.size(), fd->ident.scope[0].c_str(), cgi->getFuncTreeFromNS(fd->ident.scope));
			// }
			cgi->addFunctionToScope(FuncDefPair(func, fd, 0), cgi->getFuncTreeFromNS(fd->ident.scope));
		}
	}

	fd->generatedFunc = func;
	return Result_t(func, 0);
}




static Result_t _dogeneric(CodegenInstance* cgi, fir::Function* reified, Identifier oldid)
{
	Identifier id;
	{
		id.scope = oldid.scope;
		id.name = oldid.name;
		id.kind = oldid.kind;
		id.functionArguments = reified->getType()->getArgumentTypes();
	}

	reified->setName(id);
	cgi->module->addFunction(reified);

	return Result_t(reified, 0);
}

Result_t FuncDecl::generateDeclForGenericFunction(CodegenInstance* cgi, std::map<std::string, fir::Type*> types)
{
	if(!this->generatedFunc)
		this->codegen(cgi);

	iceAssert(this->generatedFunc);
	fir::Function* reified = this->generatedFunc->reify(types);

	return _dogeneric(cgi, reified, this->generatedFunc->getName());
}

Result_t FuncDecl::generateDeclForGenericFunctionUsingFunctionType(CodegenInstance* cgi, fir::FunctionType* ft)
{
	if(!this->generatedFunc)
		this->codegen(cgi);

	iceAssert(this->generatedFunc);
	fir::Function* reified = this->generatedFunc->reifyUsingFunctionType(ft);

	return _dogeneric(cgi, reified, this->generatedFunc->getName());
}





Result_t FuncDecl::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	if(this->isCStyleVarArg && (!this->isFFI || this->ffiType != FFIType::C))
		error(this, "C-style variadic arguments are only supported with C-style FFI function declarations.");

	if(this->ident.scope.empty())
		error(this, "no scope");

	if(this->didCodegen)
	{
		exitless_error("again??");
		iceAssert(this->generatedFunc);
		return Result_t(this->generatedFunc, 0);
	}


	// printf("gen func: %s\n", this->ident.str().c_str());

	// check if empty and if it's an extern. mangle the name to include type info if possible.
	bool isMemberFunction = (this->parentClass.first != nullptr);


	if(isMemberFunction)
	{
		iceAssert(!this->isFFI);

		if(!this->isStatic)
		{
			// do a check.
			for(auto p : this->params)
			{
				if(p->ident.name == "self")
					error(this, "Cannot have a parameter named 'self' in a method declaration");
			}
		}
	}
	else
	{
		if(this->ident.name == "main")
			this->attribs |= Attr_NoMangle;
	}

	std::vector<fir::Type*> argtypes;
	fir::Type* returnType = 0;
	cgi->pushGenericTypeStack();
	{
		// if we're not generic, then genericTypes will be empty anyway.
		for(auto p : this->genericTypes)
			cgi->pushGenericType(p.first, fir::ParametricType::get(p.first));

		for(VarDecl* v : this->params)
			argtypes.push_back(v->getType(cgi));

		returnType = this->getType(cgi);
	}
	cgi->popGenericTypeStack();


	if(isMemberFunction && !this->isStatic)
	{
		fir::Type* st = this->parentClass.second;
		if(st == 0)
			st = this->parentClass.first->createType(cgi);

		argtypes.insert(argtypes.begin(), st->getPointerTo());
	}

	this->didCodegen = true;
	bool disableMangle = (this->attribs & Attr_NoMangle || this->isFFI);
	return generateActualFuncDecl(cgi, this, argtypes, returnType, !disableMangle);
}






fir::Type* FuncDecl::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	fir::Type* t = cgi->getTypeFromParserType(this, this->ptype);
	if(t)
		return t;

	else
		error(this, "Unknown type '%s'", this->ptype->str().c_str());
}
































fir::Type* ForeignFuncDecl::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return 0;
}

Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	return this->decl->codegen(cgi);
}











