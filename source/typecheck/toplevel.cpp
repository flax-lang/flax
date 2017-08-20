// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "typecheck.h"

using namespace ast;

namespace sst
{
	static StateTree* cloneTree(StateTree* clonee, StateTree* surrogateParent)
	{
		auto clone = new StateTree(clonee->name, surrogateParent);
		for(auto sub : clonee->subtrees)
			clone->subtrees[sub.first] = cloneTree(sub.second, clone);

		clone->functions = clonee->functions;
		clone->variables = clonee->variables;
		clone->foreignFunctions = clonee->foreignFunctions;
		clone->unresolvedGenericFunctions = clonee->unresolvedGenericFunctions;

		return clone;
	}

	static void deleteTree(StateTree* tree)
	{
		for(auto sub : tree->subtrees)
			deleteTree(sub.second);

		delete tree;
	}

	static StateTree* addTreeToExistingTree(StateTree* existing, StateTree* _tree, StateTree* commonParent)
	{
		auto tree = cloneTree(_tree, commonParent);
		deleteTree(_tree);

		// if(existing->name != tree->name)
		// 	error("Cannot merge two StateTrees with differing names ('%s' and '%s')", existing->name.c_str(), tree->name.c_str());


		// first merge all children -- copy whatever 1 has, plus what 1 and 2 have in common
		for(auto sub : tree->subtrees)
		{
			if(auto it = existing->subtrees.find(sub.first); it != existing->subtrees.end())
				addTreeToExistingTree(existing->subtrees[sub.first], sub.second, existing);

			else
				existing->subtrees[sub.first] = cloneTree(sub.second, existing);
		}

		// then, add all functions and shit
		// todo: check for duplicates
		for(auto fs : tree->functions)
		{
			for(auto fn : fs.second)
			{
				if(fn->privacy == PrivacyLevel::Public)
					existing->functions[fs.first].push_back(fn);
			}
		}


		for(auto f : tree->foreignFunctions)
		{
			if(auto it = existing->foreignFunctions.find(f.first); it != existing->foreignFunctions.end())
			{
				auto fn = it->second;
				exitless_error(f.second, "Function '%s' already exists; foreign functions cannot be overloaded", fn->id.str().c_str());
				info(fn, "Previously declared here:");

				doTheExit();
			}

			if(f.second->privacy == PrivacyLevel::Public)
				existing->foreignFunctions[f.first] = f.second;
		}

		for(auto f : tree->unresolvedGenericFunctions)
		{
			existing->unresolvedGenericFunctions[f.first].insert(existing->unresolvedGenericFunctions[f.first].end(),
				f.second.begin(), f.second.end());
		}


		for(auto f : tree->variables)
		{
			if(auto it = existing->variables.find(f.first); it != existing->variables.end())
			{
				auto fn = it->second;
				exitless_error(f.second, "Variable '%s' already exists", fn->name.c_str());
				info(fn, "Previously declared here:");

				doTheExit();
			}

			existing->variables[f.first] = f.second;
		}

		return existing;
	}




	DefinitionTree* typecheck(const parser::ParsedFile& file, std::vector<std::pair<std::string, StateTree*>> imports)
	{
		auto tree = new sst::StateTree(file.moduleName, 0);
		auto fs = new TypecheckState(tree);

		for(auto [ filename, import ] : imports)
		{
			fs->dtree->thingsImported.push_back(filename);
			addTreeToExistingTree(tree, import, 0);
		}

		auto tns = dynamic_cast<NamespaceDefn*>(file.root->typecheck(fs));
		iceAssert(tns);
		tns->name = file.moduleName;

		fs->dtree->topLevel = tns;
		return fs->dtree;
	}
}


sst::Stmt* ast::TopLevelBlock::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	auto ret = new sst::NamespaceDefn(this->loc);

	if(this->name == "")	fs->topLevelNamespace = ret;
	else					fs->pushTree(this->name);

	for(auto stmt : this->statements)
	{
		if(dynamic_cast<ast::ImportStmt*>(stmt))
			continue;

		ret->statements.push_back(stmt->typecheck(fs));
	}

	if(this->name != "")
		fs->popTree();

	ret->name = this->name;
	return ret;
}
















