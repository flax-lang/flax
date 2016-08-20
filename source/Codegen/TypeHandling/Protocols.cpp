// Protocols.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


static bool _checkConform(CodegenInstance* cgi, ProtocolDef* prot, fir::Type* type, std::deque<FuncDecl*>* missing,
	Expr** user, std::string* name)
{
	TypePair_t* tp = cgi->getType(type);

	if(tp && tp->second.second == TypeKind::Class)
	{
		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		iceAssert(cls);

		*user = cls;
		*name = cls->ident.name;

		// first check if we're even listed -- don't allow implicit conformity
		bool found = false;
		for(auto ps : cls->protocolstrs)
		{
			if(cgi->resolveProtocolName(cls, ps) == prot)
			{
				found = true;
				break;
			}
		}

		if(!found) return false;


		for(Func* f : prot->funcs)
		{
			FuncDecl* fn = f->decl;

			bool found = false;
			for(auto cf : cls->funcs)
			{
				fir::Function* fcf = cls->functionMap[cf];

				// fcf's first argument is self -- remove that.
				auto tl = fcf->getType()->getArgumentTypes();
				tl.pop_front();

				int _ = 0;
				if(fn->ident.name == cf->decl->ident.name && cgi->isValidFuncOverload({ 0, fn }, tl, &_, true)
					&& ((fn->type.strType == "Self" && cls->createdType == fcf->getReturnType()) || cgi->getExprType(fn) == fcf->getReturnType()))
				{
					found = true;
					break;
				}
			}

			if(!found)
				(*missing).push_back(fn);
		}

		return (*missing).size() == 0;
	}

	return false;
}


bool ProtocolDef::checkTypeConformity(CodegenInstance* cgi, fir::Type* type)
{
	Expr* __ = 0;
	std::string ___;

	std::deque<FuncDecl*> _;
	return _checkConform(cgi, this, type, &_, &__, &___);
}

void ProtocolDef::assertTypeConformity(CodegenInstance* cgi, fir::Type* type)
{
	std::string name;
	Expr* user = 0;
	std::deque<FuncDecl*> missing;
	_checkConform(cgi, this, type, &missing, &user, &name);

	if(missing.size() > 0)
	{
		errorNoExit(user, "Class '%s' does not conform to protocol '%s'", name.c_str(), this->ident.name.c_str());

		std::string list;
		for(auto d : missing)
			list += "\t" + cgi->printAst(d) + "\n";

		info("Missing function%s:\n%s", missing.size() == 1 ? "" : "s", list.c_str());

		doTheExit();
	}
}








fir::Type* ProtocolDef::createType(CodegenInstance* cgi, std::unordered_map<std::string, fir::Type*> instantiatedGenericTypes)
{
	for(Func* f : this->funcs)
	{
		if(f->block != 0)
			error(f, "Default protocol implementations not (yet) supported");
	}

	return 0;
}

Result_t ProtocolDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}














