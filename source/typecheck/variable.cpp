// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::Ident::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// hm.
	auto tree = fs->stree;
	while(tree)
	{
		auto vs = tree->definitions[this->name];
		if(vs.size() > 1)
		{
			exitless_error(this, "Ambiguous reference to '%s'", this->name);
			for(auto v : vs)
				info(v, "Potential target here:");

			doTheExit();
		}
		else if(vs.empty())
		{
			continue;
		}

		auto def = vs.front();
		if(def)
		{
			auto ret = new sst::VarRef(this->loc);
			ret->name = this->name;
			ret->def = def;

			// check what it is
			if(auto var = dcast(sst::Defn, def))
			{
				ret->type = var->type;
			}
			else
			{
				error(this, "what is this?");
			}

			return ret;
		}

		tree = tree->parent;
	}

	// ok, we haven't found anything
	error(this, "Reference to unknown variable '%s'", this->name);
}




sst::Stmt* ast::VarDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// ok, then.
	auto defn = new sst::VarDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Name);
	defn->id.scope = fs->getCurrentScope();

	defn->immutable = this->immut;
	defn->privacy = this->privacy;
	defn->type = fs->convertParserTypeToFIR(this->type);


	fs->checkForShadowingOrConflictingDefinition(defn, "variable", [this](TCS* fs, sst::Defn* other) -> bool {
		if(auto v = dcast(sst::Defn, other))
			return v->id.name == this->name;

		else
			return true;
	});


	if(this->initialiser)
	{
		defn->init = dcast(sst::Expr, this->initialiser->typecheck(fs, defn->type));
		if(!defn->init)
			error(this->initialiser, "Statement cannot be used as an expression");
	}

	fs->stree->definitions[this->name].push_back(defn);
	return defn;
}


















