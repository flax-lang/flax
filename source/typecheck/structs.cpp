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
		SimpleError::make(floc, "Composite type '%s' cannot contain a field of its own type; use a pointer.", strty)
			.append(SimpleError::make(MsgType::Note, fs->typeDefnMap[strty]->loc, "Type '%s' was defined here:", strty))
			.postAndQuit();
	}
	else if(seeing.find(field) != seeing.end())
	{
		SimpleError::make(floc, "Recursive definition of field with a non-pointer type; mutual recursion between types '%s' and '%s'", field, strty)
			.append(SimpleError::make(MsgType::Note, fs->typeDefnMap[strty]->loc, "Type '%s' was defined here:", strty))
			.postAndQuit();
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

void checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc)
{
	std::set<fir::Type*> seeing;
	_checkFieldRecursion(fs, strty, field, floc, seeing);
}













TCResult ast::StructDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);
	auto defn = new sst::StructDefn(this->loc);
	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();
	defn->visibility = this->visibility;
	defn->original = this;

	auto str = fir::StructType::createWithoutBody(defn->id);
	defn->type = str;

	fs->checkForShadowingOrConflictingDefinition(defn, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; });

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		//? see comment in typecheck/functions.cpp about this.
		if(!defn->type->containsPlaceholders()) fs->stree->addDefinition(this->name, defn, gmaps);
		// else                                    fs->stree->unresolvedGenericDefs[this->name].push_back(this);

		fs->typeDefnMap[str] = defn;
	}


	fs->pushTree(defn->id.name);
	{
		for(auto t : this->nestedTypes)
			t->generateDeclaration(fs, 0, { });
	}
	fs->popTree();

	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });
	return TCResult(defn);
}

TCResult ast::StructDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);

	if(tcr.isParametric()) return tcr;

	auto defn = dcast(sst::StructDefn, tcr.defn());
	iceAssert(defn);

	if(this->finishedTypechecking.find(defn) != this->finishedTypechecking.end())
		return TCResult(defn);

	auto str = defn->type->toStructType();
	iceAssert(str);


	fs->pushTree(defn->id.name);
	std::vector<std::pair<std::string, fir::Type*>> tys;


	for(auto t : this->nestedTypes)
	{
		auto tcr = t->typecheck(fs);
		if(tcr.isParametric())  continue;
		if(tcr.isError())       error(t, "Failed to generate declaration for nested type '%s' in struct '%s'", t->name, this->name);

		auto st = dcast(sst::TypeDefn, tcr.defn());
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
			auto v = dcast(sst::StructFieldDefn, f->typecheck(fs).defn());
			iceAssert(v);

			if(v->init) error(v, "Struct fields cannot have inline initialisers");

			defn->fields.push_back(v);
			tys.push_back({ v->id.name, v->type });

			checkFieldRecursion(fs, str, v->type, v->loc);
		}

		//* generate all the decls first so we can call methods out of order.
		for(auto m : this->methods)
		{
			auto res = m->generateDeclaration(fs, str, { });
			if(res.isParametric())
				error(m, "Methods of a type cannot be polymorphic (for now???)");

			auto decl = dcast(sst::FunctionDefn, res.defn());
			iceAssert(decl);

			defn->methods.push_back(decl);
		}

		for(auto m : this->methods)
			m->typecheck(fs, str, { });
	}
	fs->leaveStructBody();


	// do static things.
	{
		for(auto f : this->staticFields)
		{
			auto v = dcast(sst::VarDefn, f->typecheck(fs).defn());
			iceAssert(v);

			defn->staticFields.push_back(v);
		}

		// same deal so we can call them out of order.
		for(auto m : this->staticMethods)
		{
			// infer is 0 because this is a static thing
			auto res = m->generateDeclaration(fs, str, { });
			if(res.isParametric())
				error(m, "Static methods of a type cannot be polymorphic (for now???)");

			auto decl = dcast(sst::FunctionDefn, res.defn());
			iceAssert(decl);

			defn->staticMethods.push_back(decl);
		}

		for(auto m : this->staticMethods)
		{
			m->typecheck(fs, 0, { });
		}
	}


	str->setBody(tys);
	fs->popTree();

	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}



















