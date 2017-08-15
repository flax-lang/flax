// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "typecheck.h"

using namespace ast;

namespace sst
{

	TypecheckState* typecheck(const parser::ParsedFile& file)
	{
		auto fs = new TypecheckState();
		auto tns = dynamic_cast<NamespaceDefn*>(file.root->typecheck(fs));
		iceAssert(tns && fs->topLevelNamespace == tns);

		return fs;
	}

}


sst::Stmt* ast::TopLevelBlock::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	auto ret = new sst::NamespaceDefn(this->loc);
	if(this->name == "")
		fs->topLevelNamespace = ret;

	for(auto stmt : this->statements)
	{
		if(auto imp = dynamic_cast<ast::ImportStmt*>(stmt))
		{
			if(this->name != "")	error(imp->loc, "Import statement not allowed here");
			else					continue;
		}

		ret->statements.push_back(stmt->typecheck(fs));
	}

	return ret;
}
















