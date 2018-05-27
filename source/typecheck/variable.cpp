// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

TCResult ast::Ident::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->name == "_")
		error(this, "'_' is a discarding binding; it does not yield a value and cannot be referred to");

	auto returnResult = [this](sst::Defn* def) -> TCResult {
		auto ret = new sst::VarRef(this->loc, def->type);
		ret->name = this->name;
		ret->def = def;

		return TCResult(ret);
	};

	// hm.
	sst::StateTree* tree = fs->stree;
	while(tree)
	{
		std::vector<sst::Defn*> vs = tree->getDefinitionsWithName(this->name);

		if(vs.size() > 1)
		{
			if(infer == 0)
			{
				auto errs = SimpleError::make(this, "Ambiguous reference to '%s'", this->name);

				for(auto v : vs)
					errs.append(SimpleError(v->loc, "Potential target here:", MsgType::Note));

				return TCResult(errs);
			}


			// ok, attempt.
			// it's probably a function, anyway
			for(auto v : vs)
			{
				if(v->type == infer)
					return returnResult(v);
			}

			auto errs = SimpleError::make(this, "No definition of '%s' matching type '%s'", this->name, infer);
			for(auto v : vs)
				errs.append(SimpleError(MsgType::Note).set(v, "Potential target here, with type '%s':", v->type ? v->type->str() : "?"));

			return TCResult(errs);
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
					return TCResult(
						SimpleError::make(this, "Field '%s' is an instance member of type '%s', and cannot be accessed statically.",
							this->name, fld->parentType->id.name)
						.append(SimpleError(MsgType::Note).set(def, "Field '%s' was defined here:", def->id.name))
					);
				}
			}

			return returnResult(def);
		}
		else if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(this->name); gdefs.size() > 0)
		{
			auto gmaps = fs->convertParserTypeArgsToFIR(this->mappings);

			auto res = fs->attemptToDisambiguateGenericReference(this->name, gdefs, gmaps, infer);
			if(res.isDefn())
				return returnResult(res.defn());
		}

		if(this->traverseUpwards)
			tree = tree->parent;

		else
			break;
	}

	// ok, we haven't found anything
	error(this, "Reference to unknown entity '%s'", this->name);
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
	fs->checkForShadowingOrConflictingDefinition(defn, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; });

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

	// warn(this, "my type is %s", defn->type);

	return TCResult(defn);
}


















