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
	static StateTree* cloneTree(StateTree* clonee, StateTree* surrogateParent, const std::string& filename)
	{
		auto clone = new StateTree(clonee->name, filename, surrogateParent);
		for(auto sub : clonee->subtrees)
			clone->subtrees[sub.first] = cloneTree(sub.second, clone, filename);

		clone->definitions = clonee->definitions;
		clone->unresolvedGenericDefs = clonee->unresolvedGenericDefs;


		clone->infixOperatorOverloads = clonee->infixOperatorOverloads;
		clone->prefixOperatorOverloads = clonee->prefixOperatorOverloads;
		clone->postfixOperatorOverloads = clonee->postfixOperatorOverloads;

		return clone;
	}

	static StateTree* addTreeToExistingTree(const std::unordered_set<std::string>& thingsImported, StateTree* existing, StateTree* _tree,
		StateTree* commonParent)
	{
		// StateTree* tree = cloneTree(_tree, commonParent);
		StateTree* tree = _tree;

		// first merge all children -- copy whatever 1 has, plus what 1 and 2 have in common
		for(auto sub : tree->subtrees)
		{
			// debuglog("add subtree '%s' (%p) to tree '%s' (%p)\n", sub.first, sub.second, existing->name, existing);
			if(auto it = existing->subtrees.find(sub.first); it != existing->subtrees.end())
			{
				addTreeToExistingTree(thingsImported, existing->subtrees[sub.first], sub.second, existing);
			}
			else
			{
				existing->subtrees[sub.first] = cloneTree(sub.second, existing, tree->topLevelFilename);
			}
		}

		// then, add all functions and shit
		for(auto defs_with_src : tree->definitions)
		{
			auto filename = defs_with_src.first;
			if(thingsImported.find(filename) != thingsImported.end())
				continue;

			for(auto defs : defs_with_src.second)
			{
				auto name = defs.first;
				for(auto def : defs.second)
				{
					// info(def, "hello there (%s)", def->visibility);
					if(def->visibility == VisibilityLevel::Public)
					{
						auto others = existing->getDefinitionsWithName(name);

						for(auto ot : others)
						{
							if(auto fn = dynamic_cast<FunctionDecl*>(def))
							{
								if(auto v = dynamic_cast<VarDefn*>(ot))
								{
									SimpleError::make(fn, "Conflicting definition for function '%s'; was previously defined as a variable")
										.append(SimpleError::make(MsgType::Note, v, "Conflicting definition was here:"))
										.postAndQuit();
								}
								else if(auto f = dynamic_cast<FunctionDecl*>(ot))
								{
									if(fir::Type::areTypeListsEqual(util::map(fn->params, [](auto p) -> fir::Type* { return p.type; }),
										util::map(f->params, [](auto p) -> fir::Type* { return p.type; })))
									{
										SimpleError::make(fn, "Duplicate definition of function '%s' with identical signature", fn->id.name)
											.append(SimpleError::make(MsgType::Note, f, "Conflicting definition was here: (%p vs %p)", f, fn))
											.postAndQuit();
									}
								}
								else
								{
									error(def, "??");
								}
							}
							else if(auto vr = dynamic_cast<sst::VarDefn*>(def))
							{
								auto err = SimpleError::make(vr, "Duplicate definition for variable '%s'");

								for(auto ot : others)
									err.append(SimpleError::make(MsgType::Note, ot, "Previously defined here:"));

								err.postAndQuit();
							}
							else
							{
								// probably a class or something

								SimpleError::make(def, "Duplicate definition of '%s'", def->id.name)
									.append(SimpleError::make(MsgType::Note, ot, "Conflicting definition was here:"))
									.postAndQuit();
							}
						}

						existing->addDefinition(tree->topLevelFilename, name, def);
						// debuglog("add def '%s' / %s into tree '%s' (%p)\n", def->id.name, name, existing->name, existing);
					}
					else
					{
						// warn(def, "skipping def %s because it is not public", def->id.name);
					}
				}
			}
		}


		for(auto f : tree->unresolvedGenericDefs)
		{
			existing->unresolvedGenericDefs[f.first].insert(existing->unresolvedGenericDefs[f.first].end(),
				f.second.begin(), f.second.end());
		}


		for(auto f : tree->infixOperatorOverloads)
		{
			existing->infixOperatorOverloads[f.first].insert(existing->infixOperatorOverloads[f.first].end(),
				f.second.begin(), f.second.end());
		}

		for(auto f : tree->prefixOperatorOverloads)
		{
			existing->prefixOperatorOverloads[f.first].insert(existing->prefixOperatorOverloads[f.first].end(),
				f.second.begin(), f.second.end());
		}

		for(auto f : tree->postfixOperatorOverloads)
		{
			existing->postfixOperatorOverloads[f.first].insert(existing->postfixOperatorOverloads[f.first].end(),
				f.second.begin(), f.second.end());
		}

		return existing;
	}



	using frontend::CollectorState;
	DefinitionTree* typecheck(CollectorState* cs, const parser::ParsedFile& file, const std::vector<std::pair<frontend::ImportThing, StateTree*>>& imports)
	{
		StateTree* tree = new sst::StateTree(file.moduleName, file.name, 0);
		auto fs = new TypecheckState(tree);


		for(auto [ ithing, import ] : imports)
		{
			StateTree* insertPoint = tree;

			auto ias = ithing.importAs;
			if(ias.empty())
				ias = cs->parsed[ithing.name].moduleName;

			if(ias != "_")
			{
				// ok, make tree the new tree thing.
				StateTree* newinspt = 0;

				if(auto it = insertPoint->subtrees.find(ias); it != insertPoint->subtrees.end())
				{
					newinspt = it->second;
				}
				else
				{
					newinspt = new sst::StateTree(ias, ithing.name, insertPoint);
					insertPoint->subtrees[ias] = newinspt;

					auto treedef = new sst::TreeDefn(cs->dtrees[ithing.name]->topLevel->loc);
					treedef->id = Identifier(ias, IdKind::Name);
					treedef->tree = newinspt;

					insertPoint->addDefinition(ias, treedef);
				}

				insertPoint = newinspt;
			}

			addTreeToExistingTree(fs->dtree->thingsImported, insertPoint, import, 0);
			fs->dtree->thingsImported.insert(ithing.name);
		}

		auto tns = dynamic_cast<NamespaceDefn*>(file.root->typecheck(fs).stmt());
		iceAssert(tns);

		tns->name = file.moduleName;

		fs->dtree->topLevel = tns;

		fs->dtree->typeDefnMap = fs->typeDefnMap;
		return fs->dtree;
	}
}


static void visitDeclarables(sst::TypecheckState* fs, ast::TopLevelBlock* ns)
{
	for(auto stmt : ns->statements)
	{
		if(auto decl = dynamic_cast<ast::Parameterisable*>(stmt))
			decl->generateDeclaration(fs, 0, { });

		else if(auto ffd = dynamic_cast<ast::ForeignFuncDefn*>(stmt))
			ffd->typecheck(fs);

		else if(auto ns = dynamic_cast<ast::TopLevelBlock*>(stmt))
		{
			fs->pushTree(ns->name);
			visitDeclarables(fs, ns);
			fs->popTree();
		}
	}
}


TCResult ast::TopLevelBlock::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	auto ret = new sst::NamespaceDefn(this->loc);

	if(this->name != "")
		fs->pushTree(this->name);

	sst::StateTree* tree = fs->stree;

	if(!fs->isInFunctionBody())
	{
		// visit all functions first, to get out-of-order calling -- but only at the namespace level, not inside functions.
		// once we're in function-body-land, everything should be imperative-driven, and you shouldn't
		// be able to see something before it is defined/declared.

		//* but, we need all types to be visible, so we can use them in the function declarations
		//* we'll see next -- if not then it breaks
		// visitTypes(fs, this, ret);

		visitDeclarables(fs, this);
	}


	for(auto stmt : this->statements)
	{
		if(dcast(ast::ImportStmt, stmt))
			continue;

		auto tcr = stmt->typecheck(fs);
		if(tcr.isError())
			return TCResult(tcr.error());

		else if(!tcr.isParametric() && !tcr.isDummy())
			ret->statements.push_back(tcr.stmt());
	}

	if(tree->parent)
	{
		auto td = new sst::TreeDefn(this->loc);
		td->tree = tree;
		td->id = Identifier(this->name, IdKind::Name);
		td->id.scope = tree->parent->getScope();

		td->visibility = this->visibility;

		fs->checkForShadowingOrConflictingDefinition(td, [](sst::TypecheckState* fs, sst::Defn* other) -> bool { return true; }, tree->parent);

		tree->parent->addDefinition(tree->topLevelFilename, td->id.name, td);
	}


	if(this->name != "")
		fs->popTree();

	ret->name = this->name;

	return TCResult(ret);
}
















