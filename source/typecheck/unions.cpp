// unions.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "mpool.h"

TCResult ast::UnionDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);
	auto defn = util::pool<sst::UnionDefn>(this->loc);
	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->realScope;
	defn->visibility = this->visibility;
	defn->original = this;

	defn->type = fir::UnionType::createWithoutBody(defn->id);

	fs->checkForShadowingOrConflictingDefinition(defn, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; });

	fs->getTreeOfScope(this->realScope)->addDefinition(defnname, defn, gmaps);

	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });

	fs->typeDefnMap[defn->type] = defn;
	return TCResult(defn);
}


TCResult ast::UnionDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);
	if(tcr.isParametric())  return tcr;
	else if(tcr.isError())  error(this, "failed to generate declaration for union '%s'", this->name);

	auto defn = dcast(sst::UnionDefn, tcr.defn());
	iceAssert(defn);

	if(this->finishedTypechecking.find(defn) != this->finishedTypechecking.end())
		return TCResult(defn);

	auto oldscope = fs->getCurrentScope();
	fs->teleportToScope(defn->id.scope);
	fs->pushTree(defn->id.name);


	ska::flat_hash_map<std::string, std::pair<size_t, fir::Type*>> vars;
	std::vector<std::pair<sst::UnionVariantDefn*, size_t>> vdefs;
	for(auto variant : this->cases)
	{
		vars[variant.first] = { std::get<0>(variant.second), (std::get<2>(variant.second)
			? fs->convertParserTypeToFIR(std::get<2>(variant.second)) : fir::Type::getVoid())
		};


		auto vdef = util::pool<sst::UnionVariantDefn>(std::get<1>(variant.second));
		vdef->parentUnion = defn;
		vdef->variantName = variant.first;
		vdef->id = Identifier(defn->id.name + "::" + variant.first, IdKind::Name);
		vdef->id.scope = fs->getCurrentScope();

		vdefs.push_back({ vdef, std::get<0>(variant.second) });

		fs->stree->addDefinition(variant.first, vdef);

		defn->variants[variant.first] = vdef;
	}

	auto unionTy = defn->type->toUnionType();
	unionTy->setBody(vars);

	// in a bit of stupidity, we need to set the type of each definition properly.
	for(const auto& [ uvd, id ] : vdefs)
		uvd->type = unionTy->getVariant(id);

	this->finishedTypechecking.insert(defn);


	fs->popTree();
	fs->teleportToScope(oldscope);

	return TCResult(defn);
}





















