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
	std::string genericMangling, bool mangle)
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
		if(genericMangling.empty())
		{
			iceAssert(!(fd->attribs & Attr_NoMangle) && !fd->isFFI);
			func = cgi->module->getOrCreateFunction(fd->ident, ft, linkageType);

			// fprintf(stderr, "gen function (1) %s\n", fd->ident.str().c_str());
		}
		else if(mangle)
		{
			auto id = Identifier(genericMangling, IdKind::Function);
			id.functionArguments = ft->getArgumentTypes();

			func = cgi->module->getOrCreateFunction(id, ft, linkageType);

			// fprintf(stderr, "gen function (2) %s // %s\n", id.str().c_str(), genericMangling.c_str());
		}
		else if(!mangle)
		{
			auto id = Identifier(genericMangling, IdKind::Name);
			func = cgi->module->getOrCreateFunction(id, ft, linkageType);

			// fprintf(stderr, "gen function (3) %s\n", id.str().c_str());
		}


		// not generic -- add the args. generic funcs can't have this.
		if(genericMangling.empty())
			fd->ident.functionArguments = ft->getArgumentTypes();


		if(fd->attribs & Attr_VisPublic)
			cgi->addPublicFunc({ func, fd });

		cgi->addFunctionToScope({ func, fd });
	}


	return Result_t(func, 0);
}





Result_t FuncDecl::generateDeclForGenericFunction(CodegenInstance* cgi, std::map<std::string, fir::Type*> types)
{
	iceAssert(types.size() == this->genericTypes.size());

	std::deque<fir::Type*> argtypes;
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

	std::string genericMangled = cgi->mangleGenericParameters(this->params);
	return generateActualFuncDecl(cgi, this, argtypes, lret, this->ident.str() + "_GNR_" + genericMangled, true);
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
	bool isGeneric = this->genericTypes.size() > 0;


	if(isMemberFunction)
	{
		iceAssert(!this->isFFI);

		std::deque<Expr*> es;
		for(auto p : this->params)
			es.push_back(p);


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


	if(!isGeneric)
	{
		std::deque<fir::Type*> argtypes;
		for(VarDecl* v : this->params)
			argtypes.push_back(cgi->getExprType(v));

		if(isMemberFunction)
		{
			fir::Type* st = this->parentClass->createdType;
			if(st == 0)
				st = this->parentClass->createType(cgi);

			argtypes.push_front(st->getPointerTo());
		}

		this->ident.functionArguments = argtypes;

		bool disableMangle = (this->attribs & Attr_NoMangle || this->isFFI);
		return generateActualFuncDecl(cgi, this, argtypes, cgi->getExprType(this), disableMangle ? this->ident.name : "", !disableMangle);
	}
	else
	{
		info(this, "");
		return Result_t(0, 0);
	}
}









































Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return this->decl->codegen(cgi);
}
