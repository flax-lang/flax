// dependency.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <stack>
#include <string>

#pragma once

namespace Ast
{
	struct Expr;
	struct Root;
}

namespace Codegen
{
	struct CodegenInstance;

	enum class DepType
	{
		Invalid,
		Function,
		Module,
		Type,
	};

	struct DepNode
	{
		Ast::Expr* expr = 0;
		std::string name;

		// mainly to aid error reporting
		std::deque<std::pair<DepNode*, Ast::Expr*>> users;

		int index = -1;
		int lowlink = -1;
		bool onStack = false;
	};

	struct Dep
	{
		DepNode* from = 0;
		DepNode* to = 0;

		DepType type;
	};

	struct DependencyGraph
	{
		std::deque<DepNode*> nodes;
		std::map<DepNode*, std::deque<Dep*>> edgesFrom;

		std::stack<DepNode*> stack;


		void addModuleDependency(std::string from, std::string to, Ast::Expr* imp);
		std::deque<std::deque<DepNode*>> findCyclicDependencies();

		std::deque<DepNode*> findDependenciesOf(Ast::Expr* expr);
	};
}

namespace SemAnalysis
{
	Codegen::DependencyGraph* resolveDependencyGraph(Codegen::CodegenInstance* cgi, Ast::Root* root);
}




























