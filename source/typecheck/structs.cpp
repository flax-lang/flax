// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include <set>



static void _checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc, std::set<fir::Type*>& seeing)
{
	seeing.insert(strty);

	if(field == strty)
	{
		exitless_error(floc, "Composite type '%s' cannot contain a field of its own type; use a pointer.", strty);
		info(fs->typeDefnMap[strty]->loc, "Type '%s' was defined here:", strty);
		doTheExit();
	}
	else if(seeing.find(field) != seeing.end())
	{
		exitless_error(floc, "Recursive definition of field with a non-pointer type; mutual recursion between types '%s' and '%s'", field, strty);
		info(fs->typeDefnMap[strty]->loc, "Type '%s' was defined here:", strty);
		doTheExit();
	}
	else if(field->isClassType())
	{
		for(auto f : field->toClassType()->getElements())
			_checkFieldRecursion(fs, field, f, floc, seeing);
	}
	else if(field->isStructType())
	{
		for(auto f : field->toStructType()->getElements())
			_checkFieldRecursion(fs, field, f, floc, seeing);
	}

	// ok, we should be fine...?
}

static void checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc)
{
	std::set<fir::Type*> seeing;
	_checkFieldRecursion(fs, strty, field, floc, seeing);
}













void ast::StructDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer)
{
	if(this->generatedDefn) return;

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

	this->generatedDefn = defn;
}

sst::Stmt* ast::StructDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	this->generateDeclaration(fs, infer);

	auto defn = dcast(sst::StructDefn, this->generatedDefn);
	iceAssert(defn);

	auto str = defn->type->toStructType();
	iceAssert(str);


	fs->pushTree(defn->id.name);
	std::vector<std::pair<std::string, fir::Type*>> tys;


	for(auto t : this->nestedTypes)
	{
		auto st = dcast(sst::TypeDefn, t->typecheck(fs));
		iceAssert(st);

		defn->nestedTypes.push_back(st);
	}


	//* this is a slight misnomer, since we only 'enter' the struct body when generating methods.
	//* for all intents and purposes, static methods (aka functions) don't really need any special
	//* treatment anyway, apart from living in a special namespace -- so this should really be fine.
	fs->enterStructBody(defn);
	{
		for(auto f : this->fields)
		{
			auto v = dcast(sst::StructFieldDefn, f->typecheck(fs));
			iceAssert(v);

			if(v->init) error(v, "Struct fields cannot have inline initialisers");

			defn->fields.push_back(v);
			tys.push_back({ v->id.name, v->type });

			checkFieldRecursion(fs, str, v->type, v->loc);
		}

		for(auto m : this->methods)
		{
			m->generateDeclaration(fs, str);
			iceAssert(m->generatedDefn);

			defn->methods.push_back(dcast(sst::FunctionDefn, m->generatedDefn));
		}

		for(auto m : this->methods)
			m->typecheck(fs, str);
	}
	fs->leaveStructBody();


	str->setBody(tys);

	fs->popTree();

	return defn;
}







sst::Stmt* ast::InitFunctionDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	return this->actualDefn->typecheck(fs, infer);
}


void ast::InitFunctionDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer)
{
	//* so here's the thing
	//* basically this init function thingy is just a normal function definition
	//* but due to the way the AST was built, and because it's actually slightly less messy IMO,
	//* we return a separate AST type that does not inherit from FuncDefn.

	//* so, to reduce code dupe and make it less stupid, we actually make a fake FuncDefn from ourselves,
	//* and typecheck that, returning that as the result.

	//* we don't want to be carrying too many distinct types around in SST nodes.


	this->actualDefn = new ast::FuncDefn(this->loc);

	this->actualDefn->name = "init";
	this->actualDefn->args = this->args;
	this->actualDefn->body = this->body;
	this->actualDefn->returnType = pts::NamedType::create(VOID_TYPE_STRING);

	this->actualDefn->generateDeclaration(fs, infer);
	this->generatedDefn = this->actualDefn->generatedDefn;
}



void ast::ClassDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer)
{
	if(this->generatedDefn) return;

	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = new sst::ClassDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();
	defn->visibility = this->visibility;

	auto cls = fir::ClassType::createWithoutBody(defn->id);
	defn->type = cls;

	if(this->bases.size() > 0)
	{
		auto base = fs->convertParserTypeToFIR(this->bases[0]);
		if(!base->isClassType())
			error(this, "Class '%s' can only inherit from a class, which '%s' is not", this->name, base);

		cls->setBaseClass(base->toClassType());

		if(this->bases.size() > 1)
			error(this, "Cannot inherit from more than one class");

		auto basedef = dcast(sst::ClassDefn, fs->typeDefnMap[base]);
		iceAssert(basedef);

		defn->baseClass = basedef;
	}


	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		fs->stree->addDefinition(this->name, defn);
		fs->typeDefnMap[cls] = defn;
	}

	this->generatedDefn = defn;
}

sst::Stmt* ast::ClassDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	this->generateDeclaration(fs, infer);

	auto defn = dcast(sst::ClassDefn, this->generatedDefn);
	iceAssert(defn);

	auto cls = defn->type->toClassType();
	iceAssert(cls);

	fs->pushTree(defn->id.name);

	if(this->initialisers.empty())
		error(this, "Class must have at least one initialiser");


	std::vector<std::pair<std::string, fir::Type*>> tys;

	for(auto t : this->nestedTypes)
	{
		auto st = dcast(sst::TypeDefn, t->typecheck(fs));
		iceAssert(st);

		defn->nestedTypes.push_back(st);
	}


	fs->enterStructBody(defn);
	{
		for(auto f : this->fields)
		{
			auto v = dcast(sst::StructFieldDefn, f->typecheck(fs));
			iceAssert(v);

			defn->fields.push_back(v);
			tys.push_back({ v->id.name, v->type });

			checkFieldRecursion(fs, cls, v->type, v->loc);

			std::function<void (sst::ClassDefn*, sst::StructFieldDefn*)> checkDupe = [&checkDupe](sst::ClassDefn* cls, sst::StructFieldDefn* fld) -> auto {
				while(cls)
				{
					for(auto bf : cls->fields)
					{
						if(bf->id.name == fld->id.name)
						{
							exitless_error(fld, "Redefinition of field '%s' (with type '%s'), that exists in the base class '%s'", fld->id.name, fld->type, cls->id);

							info(bf, "'%s' was previously defined in the base class here:", fld->id.name);
							info(cls, "Base class '%s' was defined here:", cls->id);
							doTheExit();
						}
					}

					cls = cls->baseClass;
				}
			};

			checkDupe(defn->baseClass, v);
		}

		for(auto m : this->methods)
		{
			if(m->name == "init")
				error(m, "Cannot have methods named 'init' in a class; to create an initialiser, omit the 'fn' keyword.");

			m->generateDeclaration(fs, cls);
			iceAssert(m->generatedDefn);

			defn->methods.push_back(dcast(sst::FunctionDefn, m->generatedDefn));
		}

		for(auto it : this->initialisers)
		{
			it->generateDeclaration(fs, cls);

			auto gd = dcast(sst::FunctionDefn, it->generatedDefn);
			iceAssert(gd);

			defn->methods.push_back(gd);
			defn->initialisers.push_back(gd);
		}
	}
	fs->leaveStructBody();


	//* do all the static stuff together
	{
		for(auto f : this->staticFields)
		{
			auto v = dcast(sst::VarDefn, f->typecheck(fs));
			iceAssert(v);

			defn->staticFields.push_back(v);
			tys.push_back({ v->id.name, v->type });
		}

		for(auto m : this->staticMethods)
		{
			// infer is 0 because this is a static thing
			m->generateDeclaration(fs, 0);
			iceAssert(m->generatedDefn);

			defn->staticMethods.push_back(dcast(sst::FunctionDefn, m->generatedDefn));
		}

		for(auto m : this->staticMethods)
		{
			m->typecheck(fs);
		}
	}


	// once we get all the proper declarations and such, create the function bodies.
	fs->enterStructBody(defn);
	{
		for(auto m : this->methods)
			m->typecheck(fs, cls);

		for(auto m : this->initialisers)
			m->typecheck(fs, cls);
	}
	fs->leaveStructBody();



	cls->setMembers(tys);





	fs->popTree();

	return defn;
}

















