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


static Result_t generateActualFuncDecl(CodegenInstance* cgi, FuncDecl* fd, std::deque<fir::Type*> argtypes, fir::Type* rettype,
	bool mangle)
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
	if(fd->genericTypes.size() == 0 && cgi->module->getFunction(fd->ident) != 0)
	{
		if(!fd->isFFI)
		{
			GenError::duplicateSymbol(cgi, fd, fd->ident.str(), SymbolType::Function);
		}
	}
	else
	{
		if(fd->genericTypes.size() == 0)
		{
			fd->ident.functionArguments = ft->getArgumentTypes();

			if(mangle)
			{
				iceAssert(!(fd->attribs & Attr_NoMangle) && !fd->isFFI);
				func = cgi->module->getOrCreateFunction(fd->ident, ft, linkageType);

				// fprintf(stderr, "gen function (1) %s\n", fd->ident.str().c_str());
			}
			else
			{
				auto id = Identifier(fd->ident.name, IdKind::Name);
				func = cgi->module->getOrCreateFunction(id, ft, linkageType);

				// fprintf(stderr, "gen function (3) %s\n", id.str().c_str());
			}

		}
		else
		{
			func = fir::Function::create(fd->ident, ft, cgi->module, linkageType);
		}

		if(fd->attribs & Attr_VisPublic)
			cgi->addPublicFunc({ func, fd });

		cgi->addFunctionToScope({ func, fd });
	}

	fd->generatedFunc = func;
	return Result_t(func, 0);
}





Result_t FuncDecl::generateDeclForGenericFunction(CodegenInstance* cgi, std::map<std::string, fir::Type*> types)
{
	if(!this->generatedFunc)
		this->codegen(cgi);

	iceAssert(this->generatedFunc);
	fir::Function* reified = this->generatedFunc->reify(types);

	Identifier id;
	{
		id.scope = this->generatedFunc->getName().scope;
		id.name = this->generatedFunc->getName().name;
		id.kind = this->generatedFunc->getName().kind;
		id.functionArguments = reified->getType()->getArgumentTypes();
	}

	reified->setName(id);
	cgi->module->addFunction(reified);

	return Result_t(reified, 0);
}





Result_t FuncDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// if we're a generic function, we can't generate anything
	// wait until we get specific instances
	// (where all the typenames, T, U etc. have been replaced with concrete types by callers)

	if(this->isCStyleVarArg && (!this->isFFI || this->ffiType != FFIType::C))
		error(this, "C-style variadic arguments are only supported with C-style FFI function declarations.");

	if(this->ident.scope.empty())
		this->ident.scope = cgi->getFullScope();



	// check if empty and if it's an extern. mangle the name to include type info if possible.
	bool isMemberFunction = (this->parentClass != nullptr);
	// bool isGeneric = this->genericTypes.size() > 0;




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

				else if(p->ident.name == "super")
					error(this, "Cannot have a parameter named 'super' in a method declaration");
			}
		}
	}
	else
	{
		if(this->ident.str() == "main")
			this->attribs |= Attr_NoMangle;
	}

	std::deque<fir::Type*> argtypes;
	for(VarDecl* v : this->params)
	{
		cgi->pushGenericTypeStack();

		// if we're not generic, then genericTypes will be empty anyway.
		for(auto p : this->genericTypes)
			cgi->pushGenericType(p.first, fir::ParametricType::get(p.first));

		argtypes.push_back(cgi->getExprType(v));

		cgi->popGenericTypeStack();
	}

	if(isMemberFunction && !this->isStatic)
	{
		fir::Type* st = this->parentClass->createdType;
		if(st == 0)
			st = this->parentClass->createType(cgi);

		argtypes.push_front(st->getPointerTo());
		info(this, "gen member (%s)", argtypes.size() > 0 ? argtypes[0]->str().c_str() : "no");
	}

	bool disableMangle = (this->attribs & Attr_NoMangle || this->isFFI);
	return generateActualFuncDecl(cgi, this, argtypes, cgi->getExprType(this), !disableMangle);
}









































Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return this->decl->codegen(cgi);
}
