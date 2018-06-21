// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

TCResult ast::FuncDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);
	if(tcr.isParametric())  return tcr;
	else if(!tcr.isDefn())  error(this, "Failed to generate declaration for function '%s'", this->name);

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
				auto vd = new sst::ArgumentDefn(arg.loc);
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

TCResult ast::FuncDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);


	using Param = sst::FunctionDefn::Param;
	std::vector<Param> ps;
	std::vector<fir::Type*> ptys;

	if(infer)
	{
		iceAssert((infer->isStructType() || infer->isClassType()) && "expected struct type for method");
		auto p = Param("self", this->loc, (this->isMutating ? infer->getMutablePointerTo() : infer->getPointerTo()));

		ps.push_back(p);
		ptys.push_back(p.type);
	}

	for(auto t : this->args)
	{
		auto p = Param(t.name, t.loc, fs->convertParserTypeToFIR(t.type));
		ps.push_back(p);
		ptys.push_back(p.type);
	}

	fir::Type* retty = fs->convertParserTypeToFIR(this->returnType);
	fir::Type* fnType = fir::FunctionType::get(ptys, retty);

	auto defn = (infer && infer->isClassType() && this->name == "init" ? new sst::ClassInitialiserDefn(this->loc) :  new sst::FunctionDefn(this->loc));
	defn->type = fnType;

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
		error(defn, "Only methods can be marked 'virtual' or 'override' at this point in time");
	}
	else if(defn->isVirtual && defn->parentTypeForMethod && !defn->parentTypeForMethod->isClassType())
	{
		error(defn, "Only methods of a class (which '%s' is not) can be marked 'virtual' or 'override'",
			defn->parentTypeForMethod->str());
	}
	else if(defn->isMutating && !defn->parentTypeForMethod)
	{
		error(defn, "Only methods of a type can be marked as mutating with 'mut'");
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

	fs->stree->addDefinition(this->name, defn);

	// add to our versions.
	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });
	return TCResult(defn);
}



TCResult ast::ForeignFuncDefn::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	if(this->generatedDecl)
		return TCResult(this->generatedDecl);

	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;
	auto defn = new sst::ForeignFuncDefn(this->loc);
	std::vector<Param> ps;

	for(auto t : this->args)
		ps.push_back(Param(t.name, t.loc, fs->convertParserTypeToFIR(t.type)));

	auto retty = fs->convertParserTypeToFIR(this->returnType);

	defn->id = Identifier(this->name, IdKind::Name);

	defn->params = ps;
	defn->returnType = retty;
	defn->visibility = this->visibility;
	defn->isVarArg = this->isVarArg;

	if(this->isVarArg)
		defn->type = fir::FunctionType::getCVariadicFunc(util::map(ps, [](Param p) -> auto { return p.type; }), retty);

	else
		defn->type = fir::FunctionType::get(util::map(ps, [](Param p) -> auto { return p.type; }), retty);


	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, [defn](sst::TypecheckState* fs, sst::Stmt* other) -> bool {

		if(auto decl = dcast(sst::FunctionDecl, other))
		{
			// make sure we didn't fuck up somewhere
			iceAssert(decl->id.name == defn->id.name);

			// check the typelists, then
			bool ret = fir::Type::areTypeListsEqual(
				util::map(defn->params, [](Param p) -> fir::Type* { return p.type; }),
				util::map(decl->params, [](Param p) -> fir::Type* { return p.type; })
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
	fs->stree->addDefinition(this->name, defn);

	return TCResult(defn);
}





TCResult ast::Block::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::Block(this->loc);

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
				error(ex, "Invalid single-expression with type '%s' in function returning '%s'", ex->type, inferred);

			auto rst = new sst::ReturnStmt(s->loc);
			rst->expectedType = (inferred ? inferred : fs->getCurrentFunction()->returnType);
			rst->value = ex;

			ret->statements = { rst };
		}
		else
		{
			error(s, "Invalid use of statement in single-expression function body");
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










