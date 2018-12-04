// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "frontend.h"
#include "typecheck.h"

#include "ir/type.h"
#include "mpool.h"

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

	static StateTree* _addTreeToExistingTree(const std::unordered_set<std::string>& thingsImported, StateTree* existing, StateTree* _tree,
		StateTree* commonParent, bool pubImport, bool ignoreVis, const std::string& importer)
	{
		// StateTree* tree = cloneTree(_tree, commonParent);
		StateTree* tree = _tree;

		// first merge all children -- copy whatever 1 has, plus what 1 and 2 have in common
		for(auto sub : tree->subtrees)
		{
			// debuglog("add subtree '%s' (%p) to tree '%s' (%p)\n", sub.first, sub.second, existing->name, existing);
			if(auto it = existing->subtrees.find(sub.first); it != existing->subtrees.end())
			{
				_addTreeToExistingTree(thingsImported, existing->subtrees[sub.first], sub.second, existing, pubImport, ignoreVis, importer);
			}
			else
			{
				existing->subtrees[sub.first] = cloneTree(sub.second, existing, tree->topLevelFilename);
			}
		}

		// then, add all functions and shit
		for(auto& defs_with_src : tree->definitions)
		{
			auto filename = defs_with_src.first;

			// if we've already seen it, don't waste time re-importing it (and checking for dupes and stuff)
			if(thingsImported.find(filename) != thingsImported.end())
				continue;

			for(auto defs : defs_with_src.second.defns)
			{
				auto name = defs.first;
				for(auto def : defs.second)
				{
					// info(def, "hello there (%s)", def->visibility);
					if(ignoreVis || ((pubImport || existing->topLevelFilename == importer) && def->visibility == VisibilityLevel::Public))
					{
						auto others = existing->getDefinitionsWithName(name);

						for(auto ot : others)
						{
							if(auto fn = dcast(FunctionDecl, def))
							{
								if(auto v = dcast(VarDefn, ot))
								{
									SimpleError::make(fn->loc, "conflicting definition for function '%s'; was previously defined as a variable")
										->append(SimpleError::make(MsgType::Note, v->loc, "conflicting definition was here:"))
										->postAndQuit();
								}
								else if(auto f = dcast(FunctionDecl, ot))
								{
									if(fir::Type::areTypeListsEqual(util::map(fn->params, [](auto p) -> fir::Type* { return p.type; }),
										util::map(f->params, [](auto p) -> fir::Type* { return p.type; })))
									{
										SimpleError::make(fn->loc, "duplicate definition of function '%s' with identical signature", fn->id.name)
											->append(SimpleError::make(MsgType::Note, f->loc, "conflicting definition was here: (%p vs %p)", f, fn))
											->postAndQuit();
									}
								}
								else
								{
									error(def, "??");
								}
							}
							else if(auto vr = dcast(VarDefn, def))
							{
								auto err = SimpleError::make(vr->loc, "duplicate definition for variable '%s'");

								for(auto ot : others)
									err->append(SimpleError::make(MsgType::Note, ot->loc, "previously defined here:"));

								err->postAndQuit();
							}
							else if(auto uvd = dcast(UnionVariantDefn, def))
							{
								// these just... don't conflict.
								if(auto ovd = dcast(UnionVariantDefn, ot); ovd)
								{
									if(ovd->parentUnion->original != uvd->parentUnion->original)
										goto conflict;
								}
								else
								{
									// ! GOTO !
									goto conflict;
								}
							}
							else
							{
								// probably a class or something
								conflict:
								SimpleError::make(def->loc, "duplicate definition of '%s'", def->id.name)
									->append(SimpleError::make(MsgType::Note, ot->loc, "conflicting definition was here:"))
									->postAndQuit();
							}
						}

						existing->addDefinition(tree->topLevelFilename, name, def);
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
			// do a proper thing!
			auto& ex = existing->unresolvedGenericDefs[f.first];
			for(auto x : f.second)
			{
				if(x->visibility == VisibilityLevel::Public && std::find(ex.begin(), ex.end(), x) == ex.end())
					ex.push_back(x);
			}
		}

		for(auto& pair : {
			std::make_pair(&existing->infixOperatorOverloads, tree->infixOperatorOverloads),
			std::make_pair(&existing->prefixOperatorOverloads, tree->prefixOperatorOverloads),
			std::make_pair(&existing->postfixOperatorOverloads, tree->postfixOperatorOverloads)
		})
		{
			auto& exst = *pair.first;
			auto& add = pair.second;

			for(const auto& f : add)
			{
				// do a proper thing!
				auto& ex = exst[f.first];
				for(auto x : f.second)
				{
					if(x->visibility == VisibilityLevel::Public && std::find(ex.begin(), ex.end(), x) == ex.end())
						ex.push_back(x);
				}
			}
		}

		return existing;
	}

	StateTree* addTreeToExistingTree(StateTree* existing, StateTree* _tree, StateTree* commonParent, bool pubImport, bool ignoreVis)
	{
		return _addTreeToExistingTree({ }, existing, _tree, commonParent, pubImport, ignoreVis, existing->topLevelFilename);
	}



	using frontend::CollectorState;
	DefinitionTree* typecheck(CollectorState* cs, const parser::ParsedFile& file, const std::vector<std::pair<frontend::ImportThing, StateTree*>>& imports)
	{
		StateTree* tree = new sst::StateTree(file.moduleName, file.name, 0);
		auto fs = new TypecheckState(tree);

		for(auto [ ithing, import ] : imports)
		{
			auto ias = ithing.importAs;
			if(ias.empty())
				ias = cs->parsed[ithing.name].modulePath + cs->parsed[ithing.name].moduleName;

			StateTree* insertPoint = tree;
			if(ias.size() == 1 && ias[0] == "_")
			{
				// do nothing.
				// insertPoint = tree;
			}
			else
			{
				StateTree* curinspt = insertPoint;
				for(const auto& impas : ias)
				{
					if(impas == curinspt->name)
					{
						// skip it.
					}
					else if(auto it = curinspt->subtrees.find(impas); it != curinspt->subtrees.end())
					{
						curinspt = it->second;
					}
					else
					{
						auto newinspt = new sst::StateTree(impas, file.name, curinspt);
						curinspt->subtrees[impas] = newinspt;

						auto treedef = util::pool<sst::TreeDefn>(cs->dtrees[ithing.name]->topLevel->loc);
						treedef->id = Identifier(impas, IdKind::Name);
						treedef->tree = newinspt;
						treedef->visibility = VisibilityLevel::Public;

						curinspt->addDefinition(file.name, impas, treedef);

						curinspt = newinspt;
					}
				}

				insertPoint = curinspt;
			}

			iceAssert(insertPoint);

			_addTreeToExistingTree(fs->dtree->thingsImported, insertPoint, import, /* commonParent: */ nullptr, ithing.pubImport,
				/* ignoreVis: */ false, file.name);

			fs->dtree->thingsImported.insert(ithing.name);
		}

		auto tns = dcast(NamespaceDefn, file.root->typecheck(fs).stmt());
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
		if(auto decl = dcast(ast::Parameterisable, stmt))
		{
			decl->realScope = fs->getCurrentScope();
			decl->generateDeclaration(fs, 0, { });
		}

		else if(auto ffd = dcast(ast::ForeignFuncDefn, stmt))
			ffd->typecheck(fs);

		else if(auto ns = dcast(ast::TopLevelBlock, stmt))
		{
			fs->pushTree(ns->name);
			visitDeclarables(fs, ns);
			fs->popTree();
		}
	}
}


TCResult ast::TopLevelBlock::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	auto ret = util::pool<sst::NamespaceDefn>(this->loc);

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
		auto td = util::pool<sst::TreeDefn>(this->loc);
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
















