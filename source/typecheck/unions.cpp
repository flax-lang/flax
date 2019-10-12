// unions.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "memorypool.h"

// defined in typecheck/structs.cpp
void checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc);
void checkTransparentFieldRedefinition(sst::TypecheckState* fs, sst::TypeDefn* defn, const std::vector<sst::StructFieldDefn*>& fields);




TCResult ast::UnionDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);

	bool israw = this->attrs.has(attr::RAW);

	sst::TypeDefn* defn = 0;
	if(israw)   defn = util::pool<sst::RawUnionDefn>(this->loc);
	else        defn = util::pool<sst::UnionDefn>(this->loc);

	defn->bareName = this->name;

	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->realScope;
	defn->visibility = this->visibility;
	defn->original = this;

	if(israw)   defn->type = fir::RawUnionType::createWithoutBody(defn->id);
	else        defn->type = fir::UnionType::createWithoutBody(defn->id);

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


	if(this->finishedTypechecking.find(tcr.defn()) != this->finishedTypechecking.end())
		return TCResult(tcr.defn());

	auto oldscope = fs->getCurrentScope();
	fs->teleportToScope(tcr.defn()->id.scope);
	fs->pushTree(tcr.defn()->id.name);


	sst::TypeDefn* ret = 0;
	if(this->attrs.has(attr::RAW))
	{
		auto defn = dcast(sst::RawUnionDefn, tcr.defn());
		iceAssert(defn);

		//* in many ways raw unions resemble structs rather than tagged unions
		//* and since we are using sst::StructFieldDefn for the variants, we will need
		//* to enter the struct body.

		auto unionTy = defn->type->toRawUnionType();
		fs->pushSelfContext(unionTy);

		util::hash_map<std::string, fir::Type*> types;
		util::hash_map<std::string, sst::StructFieldDefn*> fields;



		auto make_field = [fs, unionTy](const std::string& name, const Location& loc, pts::Type* ty) -> sst::StructFieldDefn* {

			auto vdef = util::pool<ast::VarDefn>(loc);
			vdef->immut = false;
			vdef->name = name;
			vdef->initialiser = nullptr;
			vdef->type = ty;
			vdef->isField = true;

			auto sfd = dcast(sst::StructFieldDefn, vdef->typecheck(fs).defn());
			iceAssert(sfd);

			if(fir::isRefCountedType(sfd->type))
				error(sfd, "reference-counted type '%s' cannot be a member of a raw union", sfd->type);

			checkFieldRecursion(fs, unionTy, sfd->type, sfd->loc);
			return sfd;
		};

		std::vector<sst::StructFieldDefn*> tfields;
		std::vector<sst::StructFieldDefn*> allFields;

		for(auto variant : this->cases)
		{
			auto sfd = make_field(variant.first, std::get<1>(variant.second), std::get<2>(variant.second));
			iceAssert(sfd);

			fields[sfd->id.name] = sfd;
			types[sfd->id.name] = sfd->type;

			allFields.push_back(sfd);
		}


		// do the transparent fields
		{
			size_t tfn = 0;
			for(auto [ loc, pty ] : this->transparentFields)
			{
				auto sfd = make_field(util::obfuscateName("transparent_field", tfn++), loc, pty);
				iceAssert(sfd);

				sfd->isTransparentField = true;

				// still add to the types, cos we need to compute sizes and stuff
				types[sfd->id.name] = sfd->type;
				tfields.push_back(sfd);
				allFields.push_back(sfd);
			}
		}

		checkTransparentFieldRedefinition(fs, defn, allFields);


		defn->fields = fields;
		defn->transparentFields = tfields;




		unionTy->setBody(types);

		fs->popSelfContext();
		ret = defn;
	}
	else
	{
		auto defn = dcast(sst::UnionDefn, tcr.defn());
		iceAssert(defn);

		util::hash_map<std::string, std::pair<size_t, fir::Type*>> vars;
		std::vector<std::pair<sst::UnionVariantDefn*, size_t>> vdefs;

		iceAssert(this->transparentFields.empty());

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





















