// Protocols.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


bool ProtocolDef::checkClassConformity(CodegenInstance* cgi, ClassDef* cls)
{
	std::deque<FuncDecl*> missing;

	for(Func* f : this->funcs)
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
			missing.push_back(fn);
	}

	if(missing.size() > 0)
	{
		errorNoExit(cls, "Class '%s' does not conform to protocol '%s'", cls->ident.name.c_str(), this->ident.name.c_str());

		std::string list;
		for(auto d : missing)
			list += "\t" + cgi->printAst(d) + "\n";

		info("Missing function%s:\n%s", missing.size() == 1 ? "" : "s", list.c_str());

		doTheExit();
	}

	return true;
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














