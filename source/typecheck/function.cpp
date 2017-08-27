// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)


sst::Stmt* ast::FuncDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->generics.size() > 0)
	{
		fs->stree->unresolvedGenericFunctions[this->name].push_back(this);
		return 0;
	}

	using Param = sst::FunctionDefn::Param;
	auto defn = new sst::FunctionDefn(this->loc);
	std::vector<Param> ps;
	std::vector<fir::Type*> ptys;

	for(auto t : this->args)
	{
		auto p = Param { .name = t.name, .loc = t.loc, .type = fs->convertParserTypeToFIR(t.type) };
		ps.push_back(p);
		ptys.push_back(p.type);
	}

	auto retty = fs->convertParserTypeToFIR(this->returnType);

	defn->id = Identifier(this->name, IdKind::Function);
	defn->id.scope = fs->getCurrentScope();
	defn->id.params = ptys;

	defn->params = ps;
	defn->returnType = retty;
	defn->privacy = this->privacy;

	defn->isEntry = this->isEntry;
	defn->noMangle = this->noMangle;

	fs->pushTree(defn->id.mangled());

	defn->body = new sst::Block(this->body->loc);

	// do the body
	for(auto stmt : this->body->statements)
		defn->body->statements.push_back(stmt->typecheck(fs));

	for(auto stmt : this->body->deferredStatements)
		defn->body->deferred.push_back(stmt->typecheck(fs));

	fs->popTree();

	// first, get the existing functions
	{
		auto fns = fs->getFunctionsWithName(this->name);
		for(auto f : fns)
		{
			const auto& thisArgs = ptys;
			std::vector<fir::Type*> otherArgs;
			std::transform(f->params.begin(), f->params.end(), std::back_inserter(otherArgs), [](Param p) { return p.type; });

			if(fir::Type::areTypeListsEqual(thisArgs, otherArgs))
			{
				exitless_error(this, "Definition of function '%s' with duplicate signature:", this->name.c_str());
				info("%s", fir::Type::typeListToString(thisArgs).c_str());

				info(f, "Previous definition was here");
				doTheExit();
			}
		}

		fs->stree->functions[this->name].push_back(defn);
	}

	return defn;
}

sst::Stmt* ast::ForeignFuncDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::ForeignFuncDefn::Param;
	auto defn = new sst::ForeignFuncDefn(this->loc);
	std::vector<Param> ps;

	for(auto t : this->args)
		ps.push_back(Param { .name = t.name, .loc = t.loc, .type = fs->convertParserTypeToFIR(t.type) });

	auto retty = fs->convertParserTypeToFIR(this->returnType);

	defn->id = Identifier(this->name, IdKind::Name);

	defn->params = ps;
	defn->returnType = retty;
	defn->privacy = this->privacy;
	defn->isVarArg = this->isVarArg;


	// add the defn to the current thingy
	if(fs->stree->foreignFunctions.find(defn->id.str()) != fs->stree->foreignFunctions.end())
	{
		exitless_error(this->loc, "Function '%s' already exists; foreign functions cannot be overloaded", this->name.c_str());
		info(fs->stree->foreignFunctions[this->name]->loc, "Previously declared here:");

		doTheExit();
	}

	fs->stree->foreignFunctions[this->name] = defn;
	return defn;
}















