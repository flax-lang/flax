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
		Identifier,
		Value,
		Type,
		Module,
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


		void addModuleDependency(std::string from, std::string to, Ast::Expr* imp)
		{
			// find existing node
			DepNode* src = 0;
			DepNode* dst = 0;
			for(auto d : this->nodes)
			{
				if(!src && d->name == from)
				{
					src = d;
				}
				else if(!dst && d->name == to)
				{
					dst = d;
				}
			}

			if(!src)
			{
				src = new DepNode();
				src->name = from;

				this->nodes.push_back(src);
			}

			if(!dst)
			{
				dst = new DepNode();
				dst->name = to;

				this->nodes.push_back(dst);
			}

			dst->users.push_back({ src, imp });

			Dep* d = new Dep();
			d->type = DepType::Module;
			d->from = src;
			d->to = dst;

			this->edgesFrom[src].push_back(d);
		}

		std::deque<std::deque<DepNode*>> findCyclicDependencies();
	};
}

namespace SemAnalysis
{
	Codegen::DependencyGraph* resolveDependencyGraph(Codegen::CodegenInstance* cgi, Ast::Root* root);
}




























