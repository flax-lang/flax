// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "sst.h"
#include "typecheck.h"

#include "resolver.h"
#include "polymorph.h"

#include "ir/type.h"
#include "memorypool.h"




static TCResult getResult(ast::Ident* ident, sst::Defn* def, bool implicit = false)
{
	auto ret = util::pool<sst::VarRef>(ident->loc, def->type);
	ret->name = ident->name;
	ret->def = def;
	ret->isImplicitField = implicit;

	return TCResult(ret);
}



static TCResult checkPotentialCandidate(sst::TypecheckState* fs, ast::Ident* ident, sst::Defn* def, fir::Type* infer)
{
	bool implicit = false;
	if(auto fld = dcast(sst::StructFieldDefn, def))
	{
		implicit = true;

		// check that we're actually in a method def.
		//* this is superior to the previous 'isInStructBody()' approach, because this will properly handle defining nested functions
		//* -- those are technically "in" a function body, but they're certainly not methods.
		if(!fs->isInFunctionBody() || !fs->getCurrentFunction()->parentTypeForMethod)
		{
			return TCResult(
				SimpleError::make(ident->loc, "field '%s' is an instance member of type '%s', and cannot be accessed statically",
					ident->name, fld->parentType->id.name)
				->append(SimpleError::make(MsgType::Note, def->loc, "field '%s' was defined here:", def->id.name))
			);
		}
	}

	// check if we're not doing something stupid!
	if(auto vd = dcast(sst::VarDefn, def))
	{
		if(fs->isInFunctionBody() && vd->definingFunction && vd->definingFunction != fs->getCurrentFunction())
		{
			return TCResult(
				SimpleError::make(ident->loc, "invalid reference to variable '%s', which was defined in another function", ident->name)
					->append(SimpleError::make(MsgType::Note, vd->loc, "'%s' was defined here:", ident->name))
					->append(BareError::make(MsgType::Note, "variable capturing (ie. closures) are currently not supported"))
			);
		}
	}


	if(def->type->isUnionVariantType())
	{
		auto uvd = dcast(sst::UnionVariantDefn, def);
		iceAssert(uvd);

		if(uvd->type->containsPlaceholders() || uvd->parentUnion->type->containsPlaceholders())
		{
			std::vector<FnCallArgument> fake_args;
			auto res = sst::resolver::resolveAndInstantiatePolymorphicUnion(fs, uvd, &fake_args, infer,
				/* isFnCall: */ /* uvd->type->toUnionVariantType()->getInteriorType()->isVoidType() ? true :  */ false);

			if(res.isError())
				return TCResult(res);

			// update uvd
			uvd = dcast(sst::UnionVariantDefn, res.defn());
		}

		// see the explanation in ast.h for ident flag.
		if(ident->checkAsType)
		{
			// make sure the types match, at least.
			if(infer && uvd->parentUnion->type != infer)
			{
				if(!infer->isUnionType())
				{
					return TCResult(SimpleError::make(ident->loc, "non-union type '%s' inferred for variant '%s' of union '%s'",
						infer, ident->name, uvd->parentUnion->type)
					);
				}
				else
				{
					return TCResult(SimpleError::make(ident->loc, "inferred union '%s' cannot yield variant '%s' of unrelated union '%s'",
						infer, ident->name, uvd->parentUnion->type)
					);
				}
			}

			return getResult(ident, uvd, implicit);
		}
		else
		{
			auto ret = util::pool<sst::UnionVariantConstructor>(ident->loc, uvd->parentUnion->type);

			ret->variantId = uvd->parentUnion->type->toUnionType()->getIdOfVariant(uvd->variantName);
			ret->parentUnion = uvd->parentUnion;
			ret->args = { };

			return TCResult(ret);
		}
	}
	else if(infer && def->type->containsPlaceholders())
	{
		auto fd = dcast(sst::FunctionDefn, def);
		iceAssert(fd);

		std::vector<FnCallArgument> infer_args;
		auto [ res, soln ] = sst::poly::attemptToInstantiatePolymorph(fs, fd->original, fd->id.name, { }, /* return_infer: */ nullptr,
			/* type_infer: */ infer, /* isFnCall: */ false, &infer_args, /* fillPlacholders: */ false,
			/* problem_infer: */ def->type);

		if(res.isError())
			return TCResult(res);

		else
			return getResult(ident, res.defn(), implicit);
	}

	return getResult(ident, def, implicit);
}




TCResult ast::Ident::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->name == "_")
	{
		return TCResult(
			SimpleError::make(this->loc, "'_' is a discarding binding; it does not yield a value and cannot be referred to")
		);
	}

	auto makeScopeExpr = [this](const sst::Scope& scope) -> sst::ScopeExpr* {
		return util::pool<sst::ScopeExpr>(this->loc, fir::Type::getVoid(), scope);
	};

	if(this->name == "::")
	{
		// find the root.
		auto t = fs->stree;
		while(t->parent)
			t = t->parent;

		return TCResult(makeScopeExpr(t->getScope2()));
	}
	else if(this->name == "^")
	{
		if(!fs->stree->parent)
		{
			return TCResult(
				SimpleError::make(fs->loc(), "invalid use of '^' at the topmost scope '%s'", fs->stree->name)
			);
		}
		else
		{
			auto t = fs->stree->parent;
			while(t->isAnonymous && t->parent)
				t = t->parent;

			return TCResult(makeScopeExpr(t->getScope2()));
		}
	}

	if(auto builtin = fir::Type::fromBuiltin(this->name))
		return TCResult(util::pool<sst::TypeExpr>(this->loc, builtin));

	if(infer && infer->containsPlaceholders())
		infer = 0;

	if(this->name == "this" && fs->isInFunctionBody() && fs->getCurrentFunction()->parentTypeForMethod)
		return TCResult(util::pool<sst::SelfVarRef>(this->loc, fs->getCurrentFunction()->parentTypeForMethod));

	// hm.
	sst::StateTree* tree = fs->stree;
	while(tree)
	{
		if(auto vs = tree->getDefinitionsWithName(this->name); vs.size() == 1)
		{
			return checkPotentialCandidate(fs, this, vs[0], infer);
		}
		else if(vs.size() > 1)
		{
			std::vector<std::pair<sst::Defn*, TCResult>> ambigs;

			for(auto v : vs)
			{
				auto res = checkPotentialCandidate(fs, this, v, infer);
				ambigs.push_back({ v, res });
			}


			if(ambigs.size() == 1)
			{
				return ambigs[0].second;
			}
			else
			{
				std::vector<std::pair<sst::Defn*, TCResult>> fails;
				std::vector<std::pair<sst::Defn*, TCResult>> succs;

				for(const auto& v : ambigs)
				{
					if(v.second.isError())  fails.push_back(v);
					else                    succs.push_back(v);
				}

				if(succs.empty())
				{
					auto errs = SimpleError::make(this->loc, "no definition of '%s'%s", this->name,
						infer ? strprintf(" matching type '%s'", infer) : "");

					for(const auto& v : succs)
						errs->append(v.second.error());

					return TCResult(errs);
				}
				else if(succs.size() > 1)
				{
					auto errs = SimpleError::make(this->loc, "ambiguous reference to '%s'", this->name);

					for(const auto& v : succs)
						errs->append(SimpleError::make(MsgType::Note, v.first->loc, "potential target here:", v.first));

					return TCResult(errs);
				}
				else
				{
					return getResult(this, succs[0].first, false);
				}
			}
		}
		else if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(this->name); gdefs.size() > 0)
		{
			std::vector<FnCallArgument> fake;
			auto pots = sst::poly::findPolymorphReferences(fs, this->name, gdefs, this->mappings, /* return_infer: */ 0, /* type_infer: */ infer,
				/* isFnCall: */ false, &fake);

			if(pots.size() > 1)
			{
				auto err = SimpleError::make(this->loc, "ambiguous reference to '%s', potential candidates:", this->name);
				for(const auto& p : pots)
					err->append(SimpleError::make(p.res.defn()->loc, ""));

				return TCResult(err);
			}
			else
			{
				iceAssert(pots.size() == 1);
				if(pots[0].res.isDefn())
					return getResult(this, pots[0].res.defn());

				else
					return pots[0].res;
			}
		}

		// check if there is a subtree with this name.
		if(auto it = tree->subtrees.find(this->name); it != tree->subtrees.end())
			return TCResult(makeScopeExpr(it->second->getScope2()));

		if(this->traverseUpwards)
			tree = tree->parent;

		else
			break;
	}

	// ok, we haven't found anything
	return TCResult(
		SimpleError::make(this->loc, "reference to unknown entity '%s'", this->name)
	);
}




TCResult ast::VarDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// ok, then.
	sst::VarDefn* defn = 0;
	if(this->isField)
	{
		auto fld = util::pool<sst::StructFieldDefn>(this->loc);
		fld->parentType = fs->typeDefnMap[fs->getCurrentSelfContext()];
		iceAssert(fld->parentType);

		defn = fld;
	}
	else
	{
		defn = util::pool<sst::VarDefn>(this->loc);
	}

	// check for people being stupid.
	if(this->name == "this" && fs->isInFunctionBody() && fs->getCurrentFunction()->parentTypeForMethod)
		return TCResult(SimpleError::make(this->loc, "invalid redefinition of 'this' inside method body"));



	iceAssert(defn);
	defn->bareName = this->name;

	defn->attrs = this->attrs;
	defn->id = Identifier(this->name, IdKind::Name);
	defn->id.scope2 = fs->getCurrentScope2();

	defn->immutable = this->immut;
	defn->visibility = this->visibility;

	defn->global = !fs->isInFunctionBody();


	if(this->type != pts::InferredType::get())
		defn->type = fs->convertParserTypeToFIR(this->type);

	else if(!this->initialiser)
		error(this, "initialiser is required for type inference");


	//* for variables, as long as the name matches, we conflict.
	if(auto err = fs->checkForShadowingOrConflictingDefinition(defn, [](auto, auto) -> bool { return true; }))
		return TCResult(err);

	// check the defn
	if(this->initialiser)
	{
		defn->init = this->initialiser->typecheck(fs, defn->type).expr();
		if(defn->init->type->isVoidType())
			error(defn->init, "value has void type");

		if(defn->type == 0)
		{
			auto t = defn->init->type;
			if(t->isConstantNumberType())
				t = fs->inferCorrectTypeForLiteral(defn->init->type->toConstantNumberType());

			defn->type = t;
		}
		else if(fir::getCastDistance(defn->init->type, defn->type) < 0)
		{
			SpanError::make(SimpleError::make(this->loc, "cannot initialise variable of type '%s' with a value of type '%s'", defn->type, defn->init->type))
				->add(util::ESpan(defn->init->loc, strprintf("type '%s'", defn->init->type)))
				->postAndQuit();
		}
	}

	if(this->name != "_")
		fs->stree->addDefinition(this->name, defn);

	// store the place where we were defined.
	if(fs->isInFunctionBody())
		defn->definingFunction = fs->getCurrentFunction();

	return TCResult(defn);
}


















