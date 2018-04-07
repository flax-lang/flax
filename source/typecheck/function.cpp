// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

sst::Stmt* ast::FuncDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->generics.size() > 0)
	{
		fs->stree->unresolvedGenericDefs[this->name].push_back(this);
		return new sst::DummyStmt(this->loc);
	}


	this->generateDeclaration(fs, infer);

	auto defn = dcast(sst::FunctionDefn, this->generatedDefn);
	iceAssert(defn);

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

		defn->body = dcast(sst::Block, this->body->typecheck(fs));
		defn->body->isSingleExpr = this->body->isArrow;

		iceAssert(defn->body);
	}
	fs->popTree();
	fs->leaveFunctionBody();

	// ok, do the check.
	defn->needReturnVoid = !fs->checkAllPathsReturn(defn);
	return defn;
}

void ast::FuncDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer)
{
	if(this->generatedDefn)
		return;

	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->generics.size() > 0)
	{
		fs->stree->unresolvedGenericDefs[this->name].push_back(this);
		return;
	}

	using Param = sst::FunctionDefn::Param;
	auto defn = (infer && infer->isClassType() && this->name == "init" ? new sst::ClassInitialiserDefn(this->loc) :  new sst::FunctionDefn(this->loc));

	std::vector<Param> ps;
	std::vector<fir::Type*> ptys;

	if(infer)
	{
		iceAssert((infer->isStructType() || infer->isClassType()) && "expected struct type for method");
		auto p = Param { "self", this->loc, (this->isMutating ? infer->getMutablePointerTo() : infer->getPointerTo()) };

		ps.push_back(p);
		ptys.push_back(p.type);

		defn->parentTypeForMethod = infer;
	}

	for(auto t : this->args)
	{
		auto p = Param { t.name, t.loc, fs->convertParserTypeToFIR(t.type) };
		ps.push_back(p);
		ptys.push_back(p.type);
	}

	fir::Type* retty = fs->convertParserTypeToFIR(this->returnType);

	defn->id = Identifier(this->name, IdKind::Function);
	defn->id.scope = fs->getCurrentScope();
	defn->id.params = ptys;

	defn->params = ps;
	defn->returnType = retty;
	defn->visibility = this->visibility;

	defn->isEntry = this->isEntry;
	defn->noMangle = this->noMangle;

	defn->type = fir::FunctionType::get(ptys, retty);

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

	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, "function", [defn](sst::TypecheckState* fs, sst::Stmt* other) -> bool {

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

	this->generatedDefn = defn;
	fs->stree->addDefinition(this->name, defn);
}



sst::Stmt* ast::ForeignFuncDefn::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	if(this->generatedDecl)
		return this->generatedDecl;


	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;
	auto defn = new sst::ForeignFuncDefn(this->loc);
	std::vector<Param> ps;

	for(auto t : this->args)
		ps.push_back(Param { t.name, t.loc, fs->convertParserTypeToFIR(t.type) });

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


	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, "function", [defn](sst::TypecheckState* fs, sst::Stmt* other) -> bool {

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
	return defn;
}





sst::Stmt* ast::Block::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::Block(this->loc);

	ret->scope = fs->getCurrentScope();
	ret->closingBrace = this->closingBrace;

	for(auto stmt : this->statements)
		ret->statements.push_back(stmt->typecheck(fs));

	for(auto dstmt : this->deferredStatements)
		ret->deferred.push_back(dstmt->typecheck(fs));

	return ret;
}










