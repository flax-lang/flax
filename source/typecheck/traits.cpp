// traits.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"
#include "polymorph.h"

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

	fs->pushSelfContext(trt);

	std::vector<std::pair<std::string, fir::FunctionType*>> meths;

	for(auto m : this->methods)
	{
		// make sure we don't have bodies -- for now!
		iceAssert(m->body == 0);

		auto res = m->generateDeclaration(fs, 0, { });
		if(res.isParametric())
			continue;

		auto decl = dcast(sst::FunctionDecl, res.defn());
		iceAssert(decl);

		defn->methods.push_back(decl);
		meths.push_back({ m->name, decl->type->toFunctionType() });
	}

	trt->setMethods(meths);

	fs->popSelfContext();

	fs->popTree();
	fs->teleportToScope(oldscope);

	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}








// used by typecheck/structs.cpp and typecheck/classes.cpp
static bool _checkFunctionTypesMatch(fir::Type* trait, fir::Type* type, fir::FunctionType* required, fir::FunctionType* candidate)
{
	auto as = required->getArgumentTypes() + required->getReturnType();
	auto bs = candidate->getArgumentTypes() + candidate->getReturnType();

	// all candidates must have a self!!
	iceAssert(as.size() > 0 && bs.size() > 0);
	if(as.size() != bs.size())
		return false;

	for(size_t i = 0; i < as.size(); i++)
	{
		auto ax = as[i];
		auto bx = bs[i];

		if(ax != bx)
		{
			auto [ abase, atrfs ] = sst::poly::internal::decomposeIntoTransforms(ax, SIZE_MAX);
			auto [ bbase, btrfs ] = sst::poly::internal::decomposeIntoTransforms(bx, SIZE_MAX);

			if(atrfs != btrfs)
				return false;

			if(abase != trait || bbase != type)
				return false;
		}
	}

	return true;
}

// TODO: needs to handle extensions!!!
void checkTraitConformity(sst::TypecheckState* fs, sst::TypeDefn* defn)
{
	std::vector<sst::TraitDefn*> traits;
	util::hash_map<std::string, std::vector<sst::FunctionDecl*>> methods;

	if(auto cls = dcast(sst::ClassDefn, defn))
	{
		for(auto m : cls->methods)
			methods[m->id.name].push_back(m);

		error("wait a bit");
	}
	else if(auto str = dcast(sst::StructDefn, defn))
	{
		for(auto m : str->methods)
			methods[m->id.name].push_back(m);

		traits = str->traits;
	}
	else
	{
		return;
	}


	// make this a little less annoying: report errors by trait, so all the missing methods for a trait are given at once
	for(auto trait : traits)
	{
		std::vector<std::tuple<Location, std::string, fir::FunctionType*>> missings;
		for(auto meth : trait->methods)
		{
			auto cands = methods[meth->id.name];
			bool found = false;

			for(auto cand : cands)
			{
				// c++ really fucking needs named arguments!!
				if(_checkFunctionTypesMatch(trait->type, defn->type,
					/* required: */ meth->type->toFunctionType(),
					/* candidate: */ cand->type->toFunctionType()
				))
				{
					found = true;
					break;
				}
			}

			if(!found)
				missings.push_back({ meth->loc, meth->id.name, meth->type->toFunctionType() });
		}

		if(missings.size() > 0)
		{
			auto err = SimpleError::make(defn->loc, "type '%s' does not conform to trait '%s'",
				defn->id.name, trait->id.name);

			for(const auto& m : missings)
			{
				err->append(SimpleError::make(MsgType::Note, std::get<0>(m), "missing implementation for method '%s': %s:",
					std::get<1>(m), (fir::Type*) std::get<2>(m)));
			}

			err->append(
				SimpleError::make(MsgType::Note, trait->loc, "trait '%s' was defined here:", trait->id.name)
			)->postAndQuit();
		}
	}
}

















