// misc.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"
#include "ir/type.h"

#include "memorypool.h"


TCResult ast::PlatformDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->defnType == Type::Intrinsic)
	{
		this->intrinsicDefn->isIntrinsic = true;
		return this->intrinsicDefn->typecheck(fs, infer);
	}
	else if(this->defnType == Type::IntegerType)
	{
		auto defn = util::pool<sst::BareTypeDefn>(this->loc);
		// auto opty = fir::OpaqueType::get(this->typeName, this->typeSizeInBits);
		auto ty = fir::PrimitiveType::getUintN(this->typeSizeInBits);

		defn->type = ty;
		defn->id = Identifier(this->typeName, IdKind::Type);
		defn->id.scope = fs->scope();

		fs->checkForShadowingOrConflictingDefinition(defn, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; });

		// add it first so we can use it in the method bodies,
		// and make pointers to it
		{
			fs->stree->addDefinition(this->typeName, defn);
			fs->typeDefnMap[ty] = defn;
		}

		return TCResult(defn);
	}
	else
	{
		return TCResult(SimpleError::make(this->loc, "nani?"));
	}
}
















