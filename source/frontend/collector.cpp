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
			parsed[file] = parser::parseFile(file);

			// note that we're guaranteed (because that's the whole point)
			// that any module we encounter here will have had all of its dependencies checked already

			std::vector<std::pair<ImportThing, sst::StateTree*>> imports;
			for(auto d : graph->getDependenciesOf(file))
			{
				auto to = d->to;

				auto stree = dtrees[to->name]->base;
				// debuglog("stree = %p\n", stree);
				iceAssert(stree);

				ImportThing ithing { to->name, d->ithing.importAs, d->ithing.loc };
				imports.push_back({ ithing, stree });
				// debuglog("%s depends on %s\n", frontend::getFilenameFromPath(file).c_str(), frontend::getFilenameFromPath(d->name).c_str());
			}

			// debuglog("typecheck %s\n", file);
			dtrees[file] = sst::typecheck(&state, parsed[file], imports);
		}

		auto dtr = dtrees[full];
		iceAssert(dtr && dtr->topLevel);

		return cgn::codegen(dtr);





		// std::function<void (sst::StateTree*, size_t)> recursivelyPrintTree = [&](sst::StateTree* tr, size_t i) {
		// 	auto tab = std::string(i * 4, ' ');
		// 	auto tab2 = std::string((i + 1) * 4, ' ');

		// 	debuglog("%stree %s\n", tab.c_str(), tr->name.c_str());
		// 	debuglog("%s{\n", tab.c_str());

		// 	for(auto f : tr->foreignFunctions)
		// 		debuglog("%sffi fn %s()\n", tab2.c_str(), f.first.c_str());

		// 	for(auto sub : tr->subtrees)
		// 		recursivelyPrintTree(sub.second, i + 1);

		// 	debuglog("%s}\n", tab.c_str());
		// };

		// recursivelyPrintTree(dtr->base, 0);
	}
}













































