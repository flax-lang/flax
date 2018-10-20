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
	defn->id.scope = fs->getCurrentScope();
	defn->visibility = this->visibility;
	defn->original = this;

	defn->type = fir::UnionType::createWithoutBody(defn->id);

	fs->checkForShadowingOrConflictingDefinition(defn, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; });

	fs->stree->addDefinition(defnname, defn, gmaps);

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


	fs->pushTree(defn->id.name);
	defer(fs->popTree());


	// size_t maxSize = 0;

	std::unordered_map<std::string, std::pair<size_t, fir::Type*>> vars;
	std::vector<std::pair<sst::UnionVariantDefn*, size_t>> vdefs;
	for(auto variant : this->cases)
	{
		vars[variant.first] = { std::get<0>(variant.second), (std::get<2>(variant.second)
			? fs->convertParserTypeToFIR(std::get<2>(variant.second)) : fir::Type::getVoid())
		};

		defn->variants[variant.first] = std::get<1>(variant.second);

		// it's a little cheaty thing here; we add ourselves to the subtree under different names,
		// so that dot-operator checking still comes back to us in the end.

		auto vdef = util::pool<sst::UnionVariantDefn>(std::get<1>(variant.second));
		vdef->parentUnion = defn;
		// vdef->type = vars[variant.first].second;
		vdef->id = Identifier(variant.first, IdKind::Name);
		vdef->id.scope = fs->getCurrentScope();

		vdefs.push_back({ vdef, std::get<0>(variant.second) });

		fs->stree->addDefinition(variant.first, vdef);
	}

	// log2 of the number of cases, rounded up, gives us the minimum number of bits we need to represent the thing.
	// given the number of bits, we divide by 8.

	// size_t idSize = 0;
	// if(maxSize <= 1)        idSize = std::max(1, (int) std::ceil(std::log2(this->cases.size())) / 8);
	// else if(maxSize <= 2)   idSize = std::max(2, (int) std::ceil(std::log2(this->cases.size())) / 8);
	// else if(maxSize <= 4)	idSize = std::max(4, (int) std::ceil(std::log2(this->cases.size())) / 8);
	// else                    idSize = std::max(8, (int) std::ceil(std::log2(this->cases.size())) / 8);

	// fir::Type* idTy = fir::PrimitiveType::getIntN(idSize * 8);

	auto unionTy = defn->type->toUnionType();
	unionTy->setBody(vars);

	// in a bit of stupidity, we need to set the type of each definition properly.
	for(const auto& [ uvd, id ] : vdefs)
		uvd->type = unionTy->getVariant(id);


	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}





















