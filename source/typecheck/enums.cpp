// enums.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)


/*
	ok, here's some documentation of how enumerations work
	they're basically strong enums, unlike C enums.
	you can cast them to ints, and they'll be numbered appropriately starting at 0.

	each enumeration value is a struct: { index: i64, value: $T }
	the index is the index of the thing, and allows some runtime things on the enum;

	for example, a value of type enumeration can have a .name, and in the future can simplify
	runtime type information getting by having a simpler array-index mechanism.

	so, a fir::EnumType will now be like a StringType, opaque-ish thing. we'll have IRB things in a similar
	fashion (it'll be a value type), and that's about it.
*/


sst::Stmt* ast::EnumDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = new sst::EnumDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();
	defn->visibility = this->visibility;

	fs->stree->addDefinition(this->name, defn);

	fs->pushTree(defn->id.name);
	defer(fs->popTree());

	if(this->memberType)	defn->memberType = fs->convertParserTypeToFIR(this->memberType);
	else					defn->memberType = fir::Type::getInt64();


	size_t index = 0;
	for(auto cs : this->cases)
	{
		sst::Expr* val = 0;
		if(cs.value)
		{
			iceAssert(defn->memberType);
			val = cs.value->typecheck(fs, defn->memberType);

			if(val->type != defn->memberType)
				error(cs.value, "Mismatched type in enum case value; expected type '%s', but found type '%s'", defn->memberType, val->type);
		}

		auto ecd = new sst::EnumCaseDefn(cs.loc);
		ecd->id = Identifier(cs.name, IdKind::Name);
		ecd->id.scope = fs->getCurrentScope();
		ecd->type = defn->memberType;
		ecd->parentEnum = defn;
		ecd->val = val;
		ecd->index = index++;

		defn->cases[cs.name] = ecd;
		fs->stree->addDefinition(cs.name, ecd);
	}

	auto ety = fir::EnumType::get(defn->id, defn->memberType);
	defn->type = ety;

	return defn;
}





