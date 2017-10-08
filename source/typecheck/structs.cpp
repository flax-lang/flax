// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::StructDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = new sst::StructDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();

	// defn->generatedScopeName = this->name;
	// defn->scope = fs->getCurrentScope();

	fs->pushTree(defn->id.name);

	std::vector<std::pair<std::string, fir::Type*>> tys;

	for(auto f : this->fields)
	{
		auto v = dynamic_cast<sst::VarDefn*>(f->typecheck(fs));
		iceAssert(v);

		defn->fields.push_back(v);
		tys.push_back({ v->id.name, v->type });
	}

	auto str = fir::StructType::create(defn->id, tys);
	defn->type = str;

	for(auto m : this->methods)
	{
		auto f = dynamic_cast<sst::FunctionDefn*>(m->typecheck(fs, str));
		iceAssert(f);

		defn->methods.push_back(f);
	}

	fs->popTree();

	fs->stree->definitions[this->name].push_back(defn);
	fs->typeDefnMap[str] = defn;

	return defn;
}















