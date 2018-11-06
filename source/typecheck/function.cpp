// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "polymorph.h"

#include "ir/type.h"
#include "mpool.h"


TCResult ast::FuncDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	std::vector<FnParam> ps;
	std::vector<fir::Type*> ptys;

	if(infer)
	{
		//! SELF HANDLING
		iceAssert((infer->isStructType() || infer->isClassType()) && "expected struct type for method");
		auto p = FnParam(this->loc, "self", (this->isMutating ? infer->getMutablePointerTo() : infer->getPointerTo()));

		ps.push_back(p);
		ptys.push_back(p.type);
	}

	int polyses = sst::poly::internal::getNextSessionId();
	for(auto t : this->args)
	{
		auto p = FnParam(t.loc, t.name, sst::poly::internal::convertPtsType(fs, this->generics, t.type, polyses));
		ps.push_back(p);
		ptys.push_back(p.type);
	}

	fir::Type* retty = sst::poly::internal::convertPtsType(fs, this->generics, this->returnType, polyses);
	fir::Type* fnType = fir::FunctionType::get(ptys, retty);

	auto defn = (infer && infer->isClassType() && this->name == "init" ? util::pool<sst::ClassInitialiserDefn>(this->loc)
		: util::pool<sst::FunctionDefn>(this->loc));

	defn->type = fnType;

	if(this->name != "init")
		defn->original = this;

	iceAssert(!infer || (infer->isStructType() || infer->isClassType()));
	defn->parentTypeForMethod = infer;


	defn->id = Identifier(this->name, IdKind::Function);
	defn->id.scope = fs->getCurrentScope();
	defn->id.params = ptys;

	defn->params = ps;
	defn->returnType = retty;
	defn->visibility = this->visibility;

	defn->isEntry = this->isEntry;
	defn->noMangle = this->noMangle;


	defn->global = !fs->isInFunctionBody();

	defn->isVirtual = this->isVirtual;
	defn->isOverride = this->isOverride;
	defn->isMutating = this->isMutating;

	if(defn->isVirtual && !defn->parentTypeForMethod)
	{
		error(defn, "only methods can be marked 'virtual' or 'override' at this point in time");
	}
	else if(defn->isVirtual && defn->parentTypeForMethod && !defn->parentTypeForMethod->isClassType())
	{
		error(defn, "only methods of a class (which '%s' is not) can be marked 'virtual' or 'override'",
			defn->parentTypeForMethod->str());
	}
	else if(defn->isMutating && !defn->parentTypeForMethod)
	{
		error(defn, "only methods of a type can be marked as mutating with 'mut'");
	}

	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, [defn](sst::TypecheckState* fs, sst::Stmt* other) -> bool {

		if(auto decl = dcast(sst::FunctionDecl, other))
		{
			// make sure we didn't fuck up somewhere
			iceAssert(decl->id.name == defn->id.name);
			return fs->isDuplicateOverload(defn->params, decl->params);
		}
		else
		{
			// variables and functions always conflict if they're in the same namespace
			return true;
		}
	});

	if(conflicts)
		error(this, "conflicting");

	if(!defn->type->containsPlaceholders())
		fs->stree->addDefinition(this->name, defn, gmaps);

	else if(fs->stree->unresolvedGenericDefs[this->name].empty())
		fs->stree->unresolvedGenericDefs[this->name].push_back(this);


	// add to our versions.
	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });
	return TCResult(defn);
}

TCResult ast::FuncDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);
	if(tcr.isParametric())  return tcr;
	else if(!tcr.isDefn())  error(this, "failed to generate declaration for function '%s'", this->name);

	auto defn = dcast(sst::FunctionDefn, tcr.defn());
	iceAssert(defn);

	if(this->finishedTypechecking.find(defn) != this->finishedTypechecking.end())
		return TCResult(defn);

	// if we have placeholders, don't bother generating anything.
	if(!defn->type->containsPlaceholders())
	{
		fs->enterFunctionBody(defn);
		fs->pushTree(defn->id.mangledName());
		{
			// add the arguments to the tree

			for(auto arg : defn->params)
			{
				auto vd = util::pool<sst::ArgumentDefn>(arg.loc);
				vd->id = Identifier(arg.name, IdKind::Name);
				vd->id.scope = fs->getCurrentScope();

				vd->type = arg.type;

				fs->stree->addDefinition(arg.name, vd);

				defn->arguments.push_back(vd);
			}

			this->body->isFunctionBody = true;
			defn->body = dcast(sst::Block, this->body->typecheck(fs, defn->returnType).stmt());
			defn->body->isSingleExpr = this->body->isArrow;

			iceAssert(defn->body);
		}
		fs->popTree();
		fs->leaveFunctionBody();

		// ok, do the check.
		defn->needReturnVoid = !fs->checkAllPathsReturn(defn);
	}

	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}


TCResult ast::ForeignFuncDefn::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	if(this->generatedDecl)
		return TCResult(this->generatedDecl);

	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = util::pool<sst::ForeignFuncDefn>(this->loc);
	std::vector<FnParam> ps;

	for(auto t : this->args)
		ps.push_back(FnParam(t.loc, t.name, fs->convertParserTypeToFIR(t.type)));

	auto retty = fs->convertParserTypeToFIR(this->returnType);

	// use our 'asname' as the identifier.
	defn->id = Identifier(this->asName, IdKind::Name);

	defn->params = ps;
	defn->returnType = retty;
	defn->visibility = this->visibility;
	defn->isVarArg = this->isVarArg;

	// the realname is the actual name of the function.
	defn->realName = this->name;

	if(this->isVarArg)
		defn->type = fir::FunctionType::getCVariadicFunc(util::map(ps, [](FnParam p) -> auto { return p.type; }), retty);

	else
		defn->type = fir::FunctionType::get(util::map(ps, [](FnParam p) -> auto { return p.type; }), retty);


	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, [defn](sst::TypecheckState* fs, sst::Stmt* other) -> bool {

		if(auto decl = dcast(sst::FunctionDecl, other))
		{
			// make sure we didn't fuck up somewhere
			iceAssert(decl->id.name == defn->id.name);

			// check the typelists, then
			bool ret = fir::Type::areTypeListsEqual(
				util::map(defn->params, [](FnParam p) -> fir::Type* { return p.type; }),
				util::map(decl->params, [](FnParam p) -> fir::Type* { return p.type; })
			);

			return ret;
		}
		else
		{
			// variables and functions always conflict if they're in the same namespace
			return true;
		}
	});

	if(conflicts)
		error(this, "conflicting");

	this->generatedDecl = defn;

	// same, use our 'asname'.
	fs->stree->addDefinition(this->asName, defn);

	return TCResult(defn);
}





TCResult ast::Block::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::Block>(this->loc);

	ret->closingBrace = this->closingBrace;

	if(this->isArrow && this->isFunctionBody)
	{
		iceAssert(this->deferredStatements.empty());
		iceAssert(this->statements.size() == 1);

		auto s = this->statements[0];
		if(auto e = dcast(ast::Expr, s))
		{
			auto ex = e->typecheck(fs, inferred).expr();
			if(inferred && ex->type != inferred)
				error(ex, "invalid single-expression with type '%s' in function returning '%s'", ex->type, inferred);

			auto rst = util::pool<sst::ReturnStmt>(s->loc);
			rst->expectedType = (inferred ? inferred : fs->getCurrentFunction()->returnType);
			rst->value = ex;

			ret->statements = { rst };
		}
		else
		{
			error(s, "invalid use of statement in single-expression function body");
		}
	}
	else
	{
		for(auto stmt : this->statements)
		{
			auto tcr = stmt->typecheck(fs);
			if(tcr.isError())
				return TCResult(tcr.error());

			else if(!tcr.isParametric() && !tcr.isDummy())
				ret->statements.push_back(tcr.stmt());
		}

		for(auto dstmt : this->deferredStatements)
		{
			auto tcr = dstmt->typecheck(fs);
			if(tcr.isError())
				return TCResult(tcr.error());

			else if(!tcr.isParametric() && !tcr.isDummy())
				ret->deferred.push_back(tcr.stmt());
		}
	}


	return TCResult(ret);
}










