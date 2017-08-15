// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)


sst::Stmt* ast::FuncDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::ForeignFuncDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	using Param = sst::ForeignFuncDefn::Param;
	auto defn = new sst::ForeignFuncDefn(this->loc);
	std::vector<Param> ps;

	for(auto t : this->args)
		ps.push_back(Param { .name = t.name, .type = fs->convertParserTypeToFIR(t.type) });

	auto rett = fs->convertParserTypeToFIR(this->returnType);

	defn->params = ps;
	defn->name = this->name;
	defn->returnType = rett;
	defn->privacy = this->privacy;
	defn->isVarArg = defn->isVarArg;

	return defn;
}

