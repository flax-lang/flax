// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include <set>
#include "memorypool.h"

static void _checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc, std::set<fir::Type*>& seeing);
static void _checkTransparentFieldRedefinition(sst::TypecheckState* fs, sst::TypeDefn* defn, const std::vector<sst::StructFieldDefn*>& fields,
	util::hash_map<std::string, Location>& seen);

// used in typecheck/unions.cpp and typecheck/classes.cpp
void checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc)
{
	std::set<fir::Type*> seeing;
	_checkFieldRecursion(fs, strty, field, floc, seeing);
}

void checkTransparentFieldRedefinition(sst::TypecheckState* fs, sst::TypeDefn* defn, const std::vector<sst::StructFieldDefn*>& fields)
{
	util::hash_map<std::string, Location> seen;
	_checkTransparentFieldRedefinition(fs, defn, fields, seen);
}

// defined in typecheck/traits.cpp
void checkTraitConformity(sst::TypecheckState* fs, sst::TypeDefn* defn);



TCResult ast::StructDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());


	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);
	auto defn = util::pool<sst::StructDefn>(this->loc);
	defn->bareName = this->name;
	defn->attrs = this->attrs;

	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->enclosingScope;
	defn->visibility = this->visibility;
	defn->original = this;
	defn->enclosingScope = this->enclosingScope;
	defn->innerScope = this->enclosingScope.appending(defnname);

	// make all our methods be methods
	for(auto m : this->methods)
	{
		m->parentType = this;
		m->enclosingScope = defn->innerScope;
	}

	auto str = fir::StructType::createWithoutBody(defn->id.convertToName(), /* isPacked: */ this->attrs.has(attr::PACKED));
	defn->type = str;

	if(auto err = fs->checkForShadowingOrConflictingDefinition(defn, [](auto, auto) -> bool { return true; }))
		return TCResult(err);

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		this->enclosingScope.stree->addDefinition(defnname, defn, gmaps);
		fs->typeDefnMap[str] = defn;
	}

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

	for(auto t : this->bases)
	{
		auto base = fs->convertParserTypeToFIR(t);
		if(!base->isTraitType())
			error(this, "struct '%s' can only implement traits, which '%s' is not", this->name, base);

		auto tdef = dcast(sst::TraitDefn, fs->typeDefnMap[base]);
		iceAssert(tdef);

		defn->traits.push_back(tdef);
		str->addTraitImpl(tdef->type->toTraitType());
	}

	fs->teleportInto(defn->innerScope);

	std::vector<std::pair<std::string, fir::Type*>> tys;

	fs->pushSelfContext(str);
	{
		for(auto f : this->fields)
		{
			auto vdef = util::pool<ast::VarDefn>(std::get<1>(f));
			vdef->immut = false;
			vdef->name = std::get<0>(f);
			vdef->initialiser = nullptr;
			vdef->type = std::get<2>(f);
			vdef->isField = true;

			auto v = dcast(sst::StructFieldDefn, vdef->typecheck(fs).defn());
			iceAssert(v);

			if(v->id.name == "_")
				v->isTransparentField = true;

			defn->fields.push_back(v);
			tys.push_back({ v->id.name, v->type });

			checkFieldRecursion(fs, str, v->type, v->loc);
		}

		//* generate all the decls first so we can call methods out of order.
		for(auto m : this->methods)
		{
			auto res = m->generateDeclaration(fs, str, { });
			if(res.isParametric())
				continue;

			auto decl = dcast(sst::FunctionDefn, res.defn());
			iceAssert(decl);

			defn->methods.push_back(decl);
		}

		for(auto m : this->methods)
			m->typecheck(fs, str, { });
	}

	checkTransparentFieldRedefinition(fs, defn, defn->fields);

	fs->popSelfContext();

	str->setBody(tys);

	fs->teleportOut();

	checkTraitConformity(fs, defn);

	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}










static void _checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc, std::set<fir::Type*>& seeing)
{
	seeing.insert(strty);

	if(field == strty)
	{
		SimpleError::make(floc, "composite type '%s' cannot contain a field of its own type; use a pointer.", strty)
			->append(SimpleError::make(MsgType::Note, fs->typeDefnMap[strty]->loc, "type '%s' was defined here:", strty))
			->postAndQuit();
	}
	else if(seeing.find(field) != seeing.end())
	{
		SimpleError::make(floc, "recursive definition of field with a non-pointer type; mutual recursion between types '%s' and '%s'", field, strty)
			->append(SimpleError::make(MsgType::Note, fs->typeDefnMap[strty]->loc, "type '%s' was defined here:", strty))
			->postAndQuit();
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
	else if(field->isRawUnionType())
	{
		for(auto f : field->toRawUnionType()->getVariants())
			_checkFieldRecursion(fs, field, f.second, floc, seeing);
	}

	// ok, we should be fine...?
}

static void _checkTransparentFieldRedefinition(sst::TypecheckState* fs, sst::TypeDefn* defn, const std::vector<sst::StructFieldDefn*>& fields,
	util::hash_map<std::string, Location>& seen)
{
	for(auto fld : fields)
	{
		if(fld->isTransparentField)
		{
			auto ty = fld->type;
			if(!ty->isRawUnionType() && !ty->isStructType())
			{
				// you can't have a transparentl field if it's not an aggregate type, lmao
				error(fld, "transparent fields must have either a struct or raw-union type.");
			}

			auto innerdef = fs->typeDefnMap[ty];
			iceAssert(innerdef);

			std::vector<sst::StructFieldDefn*> flds;
			if(auto str = dcast(sst::StructDefn, innerdef); str)
				flds = str->fields;

			else if(auto unn = dcast(sst::RawUnionDefn, innerdef); unn)
				flds = zfu::map(unn->fields, zfu::pair_second()) + unn->transparentFields;

			else
				error(fs->loc(), "what kind of type is this? '%s'", ty);

			_checkTransparentFieldRedefinition(fs, innerdef, flds, seen);
		}
		else
		{
			if(auto it = seen.find(fld->id.name); it != seen.end())
			{
				SimpleError::make(fld->loc, "redefinition of transparently accessible field '%s'", fld->id.name)
					->append(SimpleError::make(MsgType::Note, it->second, "previous definition was here:"))
					->postAndQuit();
			}
			else
			{
				seen[fld->id.name] = fld->loc;
			}
		}
	}
}









