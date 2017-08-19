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
			dst->loc = loc;

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

	std::vector<DepNode*> DependencyGraph::getDependenciesOf(std::string name)
	{
		DepNode* node = 0;
		for(auto n : this->nodes)
		{
			if(n->name == name)
			{
				node = n;
				break;
			}
		}

		if(!node)
			error("No such module with name '%s'", name.c_str());

		auto edges = this->edgesFrom[node];
		std::vector<DepNode*> ret;

		for(auto edge : edges)
			ret.push_back(edge->to);

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











	std::vector<std::string> checkForCycles(std::string topmod, frontend::DependencyGraph* graph)
	{
		auto groups = graph->findCyclicDependencies();
		for(auto grp : groups)
		{
			if(grp.size() > 1)
			{
				std::string modlist;
				std::vector<Location> locs;

				for(auto m : grp)
				{
					std::string fn = getFilenameFromPath(m->name);
					fn = fn.substr(0, fn.find_last_of('.'));

					modlist += "    " + fn + "\n";
				}

				info("Cyclic import dependencies between these modules:\n%s", modlist.c_str());
				info("Offending import statements:");

				for(auto m : grp)
				{
					for(auto u : m->users)
					{
						// va_list ap;

						info(u.second, "");

						// __error_gen(prettyErrorImport(dynamic_cast<Import*>(u.second), u.first->name), "here", "Note", false, ap);
					}
				}

				error("Cyclic dependencies found, cannot continue");
			}
		}



		if(groups.size() == 0)
		{
			frontend::DepNode* dn = new frontend::DepNode();
			dn->name = topmod;
			groups.insert(groups.begin(), { dn });
		}

		std::vector<std::string> fulls;
		for(auto grp : groups)
		{
			// make sure it's 1
			iceAssert(grp.size() == 1);

			fulls.push_back(frontend::getFullPathOfFile(grp[0]->name));
		}

		return fulls;
	}

	frontend::DependencyGraph* buildDependencyGraph(frontend::DependencyGraph* graph, std::string full,
		std::unordered_map<std::string, bool>& visited)
	{
		auto tokens = frontend::getFileTokens(full);
		auto imports = parser::parseImports(full, tokens);

		// get the proper import of each 'import'
		std::vector<std::string> fullpaths;
		for(auto imp : imports)
		{
			auto tovisit = resolveImport(imp.first, imp.second, full);
			graph->addModuleDependency(full, tovisit, imp.second);

			if(!visited[tovisit])
			{
				visited[tovisit] = true;
				buildDependencyGraph(graph, tovisit, visited);
			}
		}

		return graph;
	}
}











