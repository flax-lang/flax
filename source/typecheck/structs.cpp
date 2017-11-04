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

	auto str = fir::StructType::createWithoutBody(defn->id);
	defn->type = str;

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		fs->stree->definitions[this->name].push_back(defn);
		fs->typeDefnMap[str] = defn;
	}

	fs->pushTree(defn->id.name);


	std::vector<std::pair<std::string, fir::Type*>> tys;


	for(auto t : this->nestedTypes)
	{
		auto st = dynamic_cast<sst::TypeDefn*>(t->typecheck(fs));
		iceAssert(st);

		defn->nestedTypes.push_back(st);
	}

	for(auto f : this->fields)
	{
		auto v = dynamic_cast<sst::VarDefn*>(f->typecheck(fs));
		iceAssert(v);

		defn->fields.push_back(v);
		tys.push_back({ v->id.name, v->type });
	}

	for(auto m : this->methods)
	{
		m->generateDeclaration(fs, str);
		iceAssert(m->generatedDefn);

		defn->methods.push_back(m->generatedDefn);
	}

	for(auto m : this->methods)
		m->typecheck(fs, str);

	str->setBody(tys);

	fs->popTree();

	return defn;
}












sst::Stmt* ast::ClassDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = new sst::ClassDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();

	// defn->generatedScopeName = this->name;
	// defn->scope = fs->getCurrentScope();

	auto cls = fir::ClassType::createWithoutBody(defn->id);
	defn->type = cls;

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		fs->stree->definitions[this->name].push_back(defn);
		fs->typeDefnMap[cls] = defn;
	}

	fs->pushTree(defn->id.name);


	std::vector<std::pair<std::string, fir::Type*>> tys;


	for(auto t : this->nestedTypes)
	{
		auto st = dynamic_cast<sst::TypeDefn*>(t->typecheck(fs));
		iceAssert(st);

		defn->nestedTypes.push_back(st);
	}

	for(auto f : this->fields)
	{
		auto v = dynamic_cast<sst::VarDefn*>(f->typecheck(fs));
		iceAssert(v);

		defn->fields.push_back(v);
		tys.push_back({ v->id.name, v->type });
	}

	// for(auto f : this->staticFields)
	// {
	// 	auto v = dynamic_cast<sst::VarDefn*>(f->typecheck(fs));
	// 	iceAssert(v);

	// 	defn->fields.push_back(v);
	// 	tys.push_back({ v->id.name, v->type });
	// }


	for(auto m : this->methods)
	{
		m->generateDeclaration(fs, cls);
		iceAssert(m->generatedDefn);

		defn->methods.push_back(m->generatedDefn);
	}

	for(auto m : this->staticMethods)
	{
		// infer is 0 because this is a static thing
		m->generateDeclaration(fs, 0);
		iceAssert(m->generatedDefn);

		defn->staticMethods.push_back(m->generatedDefn);
	}

	for(auto m : this->methods)
		m->typecheck(fs, cls);

	for(auto m : this->staticMethods)
		m->typecheck(fs);



	cls->setMembers(tys);





	fs->popTree();

	return defn;
}

















