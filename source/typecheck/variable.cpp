// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Expr* ast::Ident::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->name == "_")
		error(this, "'_' is a discarding binding; it does not yield a value and cannot be referred to");

	// hm.
	sst::StateTree* tree = fs->stree;
	while(tree)
	{
		std::vector<sst::Defn*> vs = tree->getDefinitionsWithName(this->name);

		if(vs.size() > 1)
		{
			if(infer == 0)
			{
				exitless_error(this, "Ambiguous reference to '%s'", this->name);
				for(auto v : vs)
					info(v, "Potential target here:");

				doTheExit();
			}


			// ok, attempt.
			// it's probably a function, anyway
			for(auto v : vs)
			{
				if(v->type == infer)
				{
					auto ret = new sst::VarRef(this->loc, v->type);
					ret->name = this->name;
					ret->def = v;

					return ret;
				}
			}

			exitless_error(this, "No definition of '%s' matching type '%s'", this->name, infer);
			for(auto v : vs)
				info(v, "Potential target here, with type '%s':", v->type ? v->type->str() : "?");

			doTheExit();
		}
		else if(!vs.empty())
		{
			auto def = vs.front();
			iceAssert(def);
			{
				auto ret = new sst::VarRef(this->loc, def->type);
				ret->name = this->name;
				ret->def = def;

				return ret;
			}
		}

		// debuglog("found nothing for '%s' in tree '%s' (%p)\n", this->name, tree->name, tree);

		if(this->traverseUpwards)
			tree = tree->parent;

		else
			break;
	}

	// ok, we haven't found anything
	error(this, "Reference to unknown variable '%s' (in scope '%s')", this->name, fs->serialiseCurrentScope());
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
	defn->visibility = this->visibility;

	defn->global = !fs->isInFunctionBody();


	if(this->type != pts::InferredType::get())
	{
		defn->type = fs->convertParserTypeToFIR(this->type);
	}
	else
	{
		if(!this->initialiser)
			error(this, "Initialiser is required for type inference");
	}

	fs->checkForShadowingOrConflictingDefinition(defn, "variable", [this](TCS* fs, sst::Defn* other) -> bool {
		if(auto v = dcast(sst::Defn, other))
			return v->id.name == this->name;

		else
			return true;
	});

	// check the defn
	if(this->initialiser)
	{
		defn->init = this->initialiser->typecheck(fs, defn->type);
		if(defn->init->type->isVoidType())
			error(defn->init, "Value has void type");

		if(defn->type == 0)
		{
			auto t = defn->init->type;
			if(t->isConstantNumberType())
				t = fs->inferCorrectTypeForLiteral(defn->init);

			defn->type = t;
		}
	}

	fs->stree->addDefinition(this->name, defn);
	// debuglog("check %s -- %p\n", util::serialiseScope(defn->id.scope), fs->stree);

	return defn;
}


















