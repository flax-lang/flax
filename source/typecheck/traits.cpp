// traits.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "mpool.h"
#include "ir/type.h"


TCResult ast::TraitDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);
	auto defn = util::pool<sst::TraitDefn>(this->loc);
	defn->bareName = this->name;

	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->realScope;
	defn->visibility = this->visibility;
	defn->original = this;

	// make all our methods be methods
	for(auto m : this->methods)
		m->parentType = this, m->realScope = this->realScope + defn->id.name;

	auto str = fir::TraitType::create(defn->id);
	defn->type = str;

	fs->checkForShadowingOrConflictingDefinition(defn, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; });

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		fs->getTreeOfScope(this->realScope)->addDefinition(defnname, defn, gmaps);
		fs->typeDefnMap[str] = defn;
	}

	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });
	return TCResult(defn);
}

TCResult ast::TraitDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);

	if(tcr.isParametric()) return tcr;

	auto defn = dcast(sst::TraitDefn, tcr.defn());
	iceAssert(defn);

	if(this->finishedTypechecking.find(defn) != this->finishedTypechecking.end())
		return TCResult(defn);

	auto trt = defn->type->toTraitType();
	iceAssert(trt);


	auto oldscope = fs->getCurrentScope();
	fs->teleportToScope(defn->id.scope);
	fs->pushTree(defn->id.name);

	std::vector<std::pair<std::string, fir::FunctionType*>> meths;

	for(auto m : this->methods)
	{
		// make sure we don't have bodies -- for now!
		iceAssert(m->body == 0);

		// this is a problem LOL, because we are not (strictly) in a struct body, 'self' will not be available,
		// so this thing will die. also we need to figure out how to get the function type, and how to do the matching...

		auto res = m->generateDeclaration(fs, trt, { });
		if(res.isParametric())
			continue;

		auto decl = dcast(sst::FunctionDecl, res.defn());
		iceAssert(decl);

		defn->methods.push_back(decl);

		// meths.push_back({ m->name, decl->type->toFunctionType() });
	}

	trt->setMethods(meths);

	fs->popTree();
	fs->teleportToScope(oldscope);

	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}

























