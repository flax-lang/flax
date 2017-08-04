// dependencies.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "frontend.h"

namespace frontend
{
	static void stronglyConnect(DependencyGraph* graph, int& index, std::vector<std::vector<DepNode*>>& connected, DepNode* node);

	void DependencyGraph::addModuleDependency(std::string from, std::string to, const Location& loc)
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

		dst->users.push_back({ src, loc });

		Dep* d = new Dep();
		d->from = src;
		d->to = dst;

		this->edgesFrom[src].push_back(d);
	}



	std::vector<std::vector<DepNode*>> DependencyGraph::findCyclicDependencies()
	{
		int index = 0;
		std::vector<std::vector<DepNode*>> ret;

		for(auto n : this->nodes)
		{
			n->index = -1;
			n->onStack = false;
			n->lowlink = -1;
		}

		for(auto n : this->nodes)
		{
			if(n->index == -1)
				stronglyConnect(this, index, ret, n);
		}

		return ret;
	}


	static void stronglyConnect(DependencyGraph* graph, int& index, std::vector<std::vector<DepNode*>>& connected, DepNode* node)
	{
		node->index = index;
		node->lowlink = index;

		index++;


		graph->stack.push(node);
		node->onStack = true;


		std::vector<Dep*> edges = graph->edgesFrom[node];
		for(auto edge : edges)
		{
			DepNode* w = edge->to;
			if(w->index == -1)
			{
				stronglyConnect(graph, index, connected, w);
				node->lowlink = (node->lowlink < w->lowlink ? node->lowlink : w->lowlink);
			}
			else if(w->onStack)
			{
				node->lowlink = (node->lowlink < w->index ? node->lowlink : w->index);
			}
		}



		if(node->lowlink == node->index)
		{
			std::vector<DepNode*> set;

			while(true)
			{
				DepNode* w = graph->stack.top();
				graph->stack.pop();

				w->onStack = false;

				set.push_back(w);

				if(w == node)
					break;
			}

			connected.push_back(set);
		}
	}
}
