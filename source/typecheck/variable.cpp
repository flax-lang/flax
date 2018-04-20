// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

TCResult ast::Ident::typecheck(sst::TypecheckState* fs, fir::Type* infer)
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

					return TCResult(ret);
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

			if(auto fld = dcast(sst::StructFieldDefn, def))
			{
				// check that we're actually in a method def.
				//* this is superior to the previous 'isInStructBody()' approach, because this will properly handle defining nested functions
				//* -- those are technically "in" a function body, but they're certainly not methods.
				if(!fs->isInFunctionBody() || !fs->getCurrentFunction()->parentTypeForMethod)
				{
					exitless_error(this, "Field '%s' is an instance member of type '%s', and cannot be accessed statically.",
						this->name, fld->parentType->id.name);

					info(def, "Field '%s' was defined here:", def->id.name);
					doTheExit();
				}
			}


			{
				auto ret = new sst::VarRef(this->loc, def->type);
				ret->name = this->name;
				ret->def = def;

				return TCResult(ret);
			}
		}
		else if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(this->name); gdefs.size() > 0)
		{
			// ok, great...
			if(this->mappings.empty())
				error(this, "Parametric entity '%s' cannot be referenced without type arguments", this->name);

			if(infer == 0 && gdefs.size() > 1)
			{
				exitless_error(this, "Ambiguous reference to parametric entity '%s'", this->name);
				for(auto g : gdefs)
					info(g, "Potential target here:");

				doTheExit();
			}

			TypeParamMap_t gmaps;
			for(const auto& p : this->mappings)
				gmaps[p.first] = fs->convertParserTypeToFIR(p.second);

			//? now if we have multiple things then we need to try them all, which can get real slow real quick.
			//? unfortunately I see no better way to do this.
			// TODO: find a better way to do this??

			std::vector<sst::Defn*> pots;
			for(const auto& gdef : gdefs)
			{
				// because we're trying multiple things potentially, allow failure.
				auto d = fs->instantiateGenericEntity(gdef, gmaps, true);
				if(d && (infer ? d->type == infer : true))
					pots.push_back(d);
			}

			if(!pots.empty())
			{
				if(pots.size() > 1)
				{
					exitless_error(this, "Ambiguous reference to parametric entity '%s'", this->name);
					for(auto p : pots)
						info(p, "Potential target here:");

					doTheExit();
				}
				else
				{
					// ok, great. just return that shit.
					auto ret = new sst::VarRef(this->loc, pots[0]->type);
					ret->name = this->name;
					ret->def = pots[0];

					return TCResult(ret);
				}
			}
			// else: just go up the tree and try again.
		}

		if(this->traverseUpwards)
			tree = tree->parent;

		else
			break;
	}

	// ok, we haven't found anything
	error(this, "Reference to unknown entity '%s' (in scope '%s')", this->name, fs->serialiseCurrentScope());
}




TCResult ast::VarDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// ok, then.
	sst::VarDefn* defn = 0;
	if(fs->isInStructBody() && !fs->isInFunctionBody())
	{
		auto fld = new sst::StructFieldDefn(this->loc);
		fld->parentType = fs->getCurrentStructBody();

		defn = fld;
	}
	else
	{
		defn = new sst::VarDefn(this->loc);
	}

	iceAssert(defn);
	defn->id = Identifier(this->name, IdKind::Name);
	defn->id.scope = fs->getCurrentScope();

	defn->immutable = this->immut;
	defn->visibility = this->visibility;

	defn->global = !fs->isInFunctionBody();


	if(this->type != pts::InferredType::get())
		defn->type = fs->convertParserTypeToFIR(this->type);

	else if(!this->initialiser)
		error(this, "Initialiser is required for type inference");


	//* for variables, as long as the name matches, we conflict.
	fs->checkForShadowingOrConflictingDefinition(defn, "variable", [](TCS* fs, sst::Defn* other) -> bool { return true; });

	// check the defn
	if(this->initialiser)
	{
		defn->init = this->initialiser->typecheck(fs, defn->type).expr();
		if(defn->init->type->isVoidType())
			error(defn->init, "Value has void type");

		if(defn->type == 0)
		{
			auto t = defn->init->type;
			if(t->isConstantNumberType())
				t = fs->inferCorrectTypeForLiteral(defn->init);

			defn->type = t;
		}
		else if(fs->getCastDistance(defn->init->type, defn->type) < 0)
		{
			error(defn->init, "Cannot assign a value of type '%s' to a variable with type '%s'",
				defn->init->type, defn->type);
		}
	}

	fs->stree->addDefinition(this->name, defn);

	return TCResult(defn);
}


















