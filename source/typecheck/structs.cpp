// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

sst::Stmt* ast::StructDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = new sst::StructDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();
	defn->visibility = this->visibility;


	auto str = fir::StructType::createWithoutBody(defn->id);
	defn->type = str;

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		fs->stree->addDefinition(this->name, defn);
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


	//* this is a slight misnomer, since we only 'enter' the struct body when generating methods.
	//* for all intents and purposes, static methods (aka functions) don't really need any special
	//* treatment anyway, apart from living in a special namespace -- so this should really be fine.
	fs->enterStructBody(defn);
	{
		for(auto m : this->methods)
		{
			m->generateDeclaration(fs, str);
			iceAssert(m->generatedDefn);

			defn->methods.push_back(m->generatedDefn);
		}

		for(auto m : this->methods)
			m->typecheck(fs, str);
	}
	fs->leaveStructBody();


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
	defn->visibility = this->visibility;

	auto cls = fir::ClassType::createWithoutBody(defn->id);
	defn->type = cls;

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		fs->stree->addDefinition(this->name, defn);
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

	fs->enterStructBody(defn);
	{
		for(auto m : this->methods)
		{
			m->generateDeclaration(fs, cls);
			iceAssert(m->generatedDefn);

			defn->methods.push_back(m->generatedDefn);
		}
	}
	fs->leaveStructBody();

	for(auto m : this->staticMethods)
	{
		// infer is 0 because this is a static thing
		m->generateDeclaration(fs, 0);
		iceAssert(m->generatedDefn);

		defn->staticMethods.push_back(m->generatedDefn);
	}


	//* again, same deal here.
	fs->enterStructBody(defn);
	{
		for(auto m : this->methods)
			m->typecheck(fs, cls);
	}
	fs->leaveStructBody();



	for(auto m : this->staticMethods)
		m->typecheck(fs);



	cls->setMembers(tys);





	fs->popTree();

	return defn;
}

















