// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "typecheck.h"

#include "ir/type.h"

using namespace ast;

namespace sst
{
	static StateTree* cloneTree(StateTree* clonee, StateTree* surrogateParent)
	{
		auto clone = new StateTree(clonee->name, surrogateParent);
		for(auto sub : clonee->subtrees)
			clone->subtrees[sub.first] = cloneTree(sub.second, clone);

		clone->definitions = clonee->definitions;
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
		StateTree* tree = cloneTree(_tree, commonParent);
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
		for(auto defs : tree->definitions)
		{
			auto name = defs.first;
			for(auto def : defs.second)
			{
				if(def->privacy == PrivacyLevel::Public)
				{
					// check functions
					if(auto fn = dynamic_cast<sst::FunctionDecl*>(def))
					{
						auto others = existing->definitions[name];
						for(auto ot : others)
						{
							if(auto v = dynamic_cast<VarDefn*>(ot))
							{
								exitless_error(fn, "Conflicting definition for function '%s'; was previously defined as a variable");
								info(ot, "Conflicting definition was here:");

								doTheExit();
							}
							else if(auto f = dynamic_cast<FunctionDecl*>(ot))
							{
								using Param = sst::FunctionDecl::Param;
								if(fir::Type::areTypeListsEqual(util::map(fn->params, [](Param p) -> fir::Type* { return p.type; }),
									util::map(f->params, [](Param p) -> fir::Type* { return p.type; })))
								{
									exitless_error(fn, "Duplicate definition of function '%s' with identical signature");
									info(ot, "Conflicting definition was here:");

									doTheExit();
								}
							}
							else
							{
								error(def, "??");
							}
						}
					}
					else if(auto vr = dynamic_cast<sst::VarDefn*>(def))
					{
						auto others = existing->definitions[name];
						if(others.size() > 0)
						{
							exitless_error(def, "Duplicate definition for variable '%s'");

							for(auto ot : others)
								info(ot, "Previously defined here:");

							doTheExit();
						}
					}
					else
					{
						error(def, "what is this?");
					}

					existing->definitions[name].push_back(def);
				}
			}
		}


		for(auto f : tree->unresolvedGenericFunctions)
		{
			existing->unresolvedGenericFunctions[f.first].insert(existing->unresolvedGenericFunctions[f.first].end(),
				f.second.begin(), f.second.end());
		}

		return existing;
	}




	DefinitionTree* typecheck(const parser::ParsedFile& file, std::vector<std::pair<std::string, StateTree*>> imports)
	{
		StateTree* tree = new sst::StateTree(file.moduleName, 0);
		auto fs = new TypecheckState(tree);

		for(auto [ filename, import ] : imports)
		{
			fs->dtree->thingsImported.push_back(filename);
			addTreeToExistingTree(tree, import, 0);
		}

		auto tns = dynamic_cast<NamespaceDefn*>(file.root->typecheck(fs));
		iceAssert(tns);

		tns->id = Identifier(file.moduleName, IdKind::Name);

		fs->dtree->topLevel = tns;
		return fs->dtree;
	}
}


static void visitFunctions(sst::TypecheckState* fs, ast::TopLevelBlock* ns)
{
	for(auto stmt : ns->statements)
	{
		if(auto fd = dynamic_cast<ast::FuncDefn*>(stmt))
			fd->generateDeclaration(fs);

		else if(auto ffd = dynamic_cast<ast::ForeignFuncDefn*>(stmt))
			ffd->typecheck(fs);

		else if(auto ns = dynamic_cast<ast::TopLevelBlock*>(stmt))
		{
			fs->pushTree(ns->name);
			visitFunctions(fs, ns);
			fs->popTree();
		}
	}
}


sst::Stmt* ast::TopLevelBlock::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	auto ret = new sst::NamespaceDefn(this->loc);

	if(this->name == "")	fs->topLevelNamespace = ret;
	else					fs->pushTree(this->name);

	sst::StateTree* tree = fs->stree;

	if(!fs->isInFunctionBody())
	{
		// visit all functions first, to get out-of-order calling -- but only at the namespace level, not inside functions.
		// once we're in function-body-land, everything should be imperative-driven, and you shouldn't
		// be able to see something after yourself.

		visitFunctions(fs, this);
	}


	for(auto stmt : this->statements)
	{
		if(dynamic_cast<ast::ImportStmt*>(stmt))
			continue;

		ret->statements.push_back(stmt->typecheck(fs));
	}

	if(tree->parent)
		tree->parent->definitions[this->name].push_back(ret);

	if(this->name != "")
		fs->popTree();

	ret->id = Identifier(this->name, IdKind::Name);
	ret->id.scope = fs->getCurrentScope();

	return ret;
}
















