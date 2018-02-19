// collector.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <sys/stat.h>

#include <unordered_map>

#include "errors.h"
#include "codegen.h"
#include "frontend.h"
#include "typecheck.h"

namespace frontend
{
	fir::Module* collectFiles(std::string _filename)
	{
		// first, collect and parse the first file
		std::string full = getFullPathOfFile(_filename);

		auto graph = new DependencyGraph();

		std::unordered_map<std::string, bool> visited;
		auto files = checkForCycles(full, buildDependencyGraph(graph, full, visited));

		CollectorState state;
		auto& parsed = state.parsed;
		auto& dtrees = state.dtrees;

		for(auto file : files)
		{
			// parse it all
			auto opers = parser::parseOperators(frontend::getFileTokens(file));

			{
				// TODO: clean this up maybe.
				auto checkDupes = [](const std::unordered_map<std::string, parser::CustomOperatorDecl>& existing,
					const std::unordered_map<std::string, parser::CustomOperatorDecl>& newops, const std::string& kind) {

					for(const auto& op : newops)
					{
						if(auto it = existing.find(op.first); it != existing.end())
						{
							exitless_error(op.second.loc, "Duplicate declaration for %s operator '%s'", kind, op.second.symbol);

							info(it->second.loc, "Previous declaration was here:");
							doTheExit();
						}
					}
				};

				checkDupes(state.binaryOps, std::get<0>(opers), "infix");
				checkDupes(state.prefixOps, std::get<1>(opers), "prefix");
				checkDupes(state.postfixOps, std::get<2>(opers), "postfix");

				for(const auto& op : std::get<0>(opers))
					state.binaryOps[op.first] = op.second;

				for(const auto& op : std::get<1>(opers))
					state.prefixOps[op.first] = op.second;

				for(const auto& op : std::get<2>(opers))
					state.postfixOps[op.first] = op.second;
			}


			parsed[file] = parser::parseFile(file, state);



			// note that we're guaranteed (because that's the whole point)
			// that any module we encounter here will have had all of its dependencies checked already

			std::vector<std::pair<ImportThing, sst::StateTree*>> imports;
			for(auto d : graph->getDependenciesOf(file))
			{
				auto imported = d->to;

				auto stree = dtrees[imported->name]->base;
				// debuglog("stree = %p\n", stree);
				iceAssert(stree);

				ImportThing ithing { imported->name, d->ithing.importAs, d->ithing.loc };
				if(auto it = std::find_if(imports.begin(), imports.end(), [&ithing](auto x) -> bool { return x.first.name == ithing.name; });
					it != imports.end())
				{
					exitless_error(ithing.loc, "Importing previously imported module '%s'", ithing.name);
					info(it->first.loc, "Previous import was here:");

					doTheExit();
				}


				imports.push_back({ ithing, stree });

				// debuglog("%s depends on %s\n", frontend::getFilenameFromPath(file).c_str(), frontend::getFilenameFromPath(d->name).c_str());
			}

			// debuglog("typecheck %s\n", file);
			dtrees[file] = sst::typecheck(&state, parsed[file], imports);
		}

		auto dtr = dtrees[full];
		iceAssert(dtr && dtr->topLevel);

		for(auto dt : dtrees)
		{
			for(auto def : dt.second->typeDefnMap)
			{
				if(auto it = dtr->typeDefnMap.find(def.first); it != dtr->typeDefnMap.end())
					iceAssert(it->second == def.second);

				dtr->typeDefnMap[def.first] = def.second;
			}
		}

		return cgn::codegen(dtr);
	}
}













































