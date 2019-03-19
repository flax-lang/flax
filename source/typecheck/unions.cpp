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

	sst::TypeDefn* defn = 0;
	if(this->israw) defn = util::pool<sst::RawUnionDefn>(this->loc);
	else            defn = util::pool<sst::UnionDefn>(this->loc);

	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->realScope;
	defn->visibility = this->visibility;
	defn->original = this;

	if(this->israw) defn->type = fir::RawUnionType::createWithoutBody(defn->id);
	else            defn->type = fir::UnionType::createWithoutBody(defn->id);

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

	//auto defn = dcast(sst::UnionDefn, tcr.defn());
	//iceAssert(defn);

	if(this->finishedTypechecking.find(tcr.defn()) != this->finishedTypechecking.end())
		return TCResult(tcr.defn());

	auto oldscope = fs->getCurrentScope();
	fs->teleportToScope(tcr.defn()->id.scope);
	fs->pushTree(tcr.defn()->id.name);


	sst::TypeDefn* ret = 0;
	if(this->israw)
	{
		auto defn = dcast(sst::RawUnionDefn, tcr.defn());
		iceAssert(defn);

		//* in many ways raw unions resemble structs rather than tagged unions
		//* and since we are using sst::StructFieldDefn for the variants, we will need
		//* to enter the struct body.

		fs->enterStructBody(defn);

		util::hash_map<std::string, fir::Type*> types;
		util::hash_map<std::string, sst::StructFieldDefn*> fields;
		for(auto variant : this->cases)
		{
			auto vdef = util::pool<ast::VarDefn>(std::get<1>(variant.second));
			vdef->immut = false;
			vdef->name = variant.first;
			vdef->initialiser = nullptr;
			vdef->type = std::get<2>(variant.second);

			auto sfd = dcast(sst::StructFieldDefn, vdef->typecheck(fs).defn());
			iceAssert(sfd);

			fields[variant.first] = sfd;
			types[variant.first] = sfd->type;
		}

		defn->fields = fields;

		auto unionTy = defn->type->toRawUnionType();
		unionTy->setBody(types);

		fs->leaveStructBody();
		ret = defn;
	}
	else
	{
		auto defn = dcast(sst::UnionDefn, tcr.defn());
		iceAssert(defn);

		util::hash_map<std::string, std::pair<size_t, fir::Type*>> vars;
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

		ret = defn;
	}

	iceAssert(ret);
	this->finishedTypechecking.insert(ret);

	fs->popTree();
	fs->teleportToScope(oldscope);

	return TCResult(ret);
}





















