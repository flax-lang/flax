// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "parser.h"
#include "frontend.h"
#include "typecheck.h"
#include "string_consts.h"

#include "ir/type.h"
#include "memorypool.h"

using namespace ast;

namespace sst
{
	static StateTree* cloneTree(StateTree* clonee, StateTree* surrogateParent, const std::string& filename)
	{
		auto clone = util::pool<StateTree>(clonee->name, filename, surrogateParent);
		clone->treeDefn = util::pool<TreeDefn>(Location());
		clone->treeDefn->tree = clone;

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
									if(fir::Type::areTypeListsEqual(util::map(fn->params, [](const auto& p) -> fir::Type* { return p.type; }),
										util::map(f->params, [](const auto& p) -> fir::Type* { return p.type; })))
									{
										SimpleError::make(fn->loc, "duplicate definition of function '%s' with identical signature", fn->id.name)
											->append(SimpleError::make(MsgType::Note, f->loc, "conflicting definition was here: (%p vs %p)",
												reinterpret_cast<void*>(f), reinterpret_cast<void*>(fn)))
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
								SimpleError::make(def->loc, "duplicate definition of %s '%s'", def->readableName, def->id.name)
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



	struct OsStrings
	{
		std::string name;
		std::string vendor;
	};

	static OsStrings getOsStrings()
	{
		// TODO: handle cygwin/msys/mingw???
		// like how do we want to expose these? at the end of the day the os is still windows...

		OsStrings ret;

		#if defined(_WIN32)
			ret.name = "windows";
			ret.vendor = "microsoft";
		#elif __MINGW__
			ret.name = "mingw";
		#elif __CYGWIN__
			ret.name = "cygwin";
		#elif __APPLE__
			ret.vendor = "apple";
			#include "TargetConditionals.h"
			#if TARGET_IPHONE_SIMULATOR
				ret.name = "iossimulator";
			#elif TARGET_OS_IOS
				ret.name = "ios";
			#elif TARGET_OS_WATCH
				ret.name = "watchos";
			#elif TARGET_OS_TV
				ret.name = "tvos";
			#elif TARGET_OS_OSX
				ret.name = "macos";
			#else
				#error "unknown apple operating system"
			#endif
		#elif __ANDROID__
			ret.vendor = "google";
			ret.name = "android";
		#elif __linux__ || __linux || linux
			ret.name = "linux";
		#elif __FreeBSD__
			ret.name = "freebsd";
		#elif __OpenBSD__
			ret.name = "openbsd";
		#elif __NetBSD__
			ret.name = "netbsd";
		#elif __DragonFly__
			ret.name = "dragonflybsd";
		#elif __unix__
			ret.name = "unix";
		#elif defined(_POSIX_VERSION)
			ret.name = "posix";
		#endif

		return ret;
	}

	static void generatePreludeDefinitions(TypecheckState* fs)
	{
		auto loc = Location();
		auto strings = getOsStrings();

		fs->pushTree("os");
		defer(fs->popTree());

		// manually add the definition, because we didn't typecheck a namespace or anything.
		fs->stree->parent->addDefinition(fs->stree->name, fs->stree->treeDefn);

		auto strty = fir::Type::getCharSlice(false);

		{
			// add the name
			auto name_def = util::pool<sst::VarDefn>(loc);
			name_def->id = Identifier("name", IdKind::Name);
			name_def->type = strty;
			name_def->global = true;
			name_def->immutable = true;

			auto s = util::pool<sst::LiteralString>(loc, strty);
			s->str = strings.name;

			name_def->init = s;
			fs->stree->addDefinition("name", name_def);
		}
		{
			// add the name
			auto vendor_def = util::pool<sst::VarDefn>(loc);
			vendor_def->id = Identifier("vendor", IdKind::Name);
			vendor_def->type = strty;
			vendor_def->global = true;
			vendor_def->immutable = true;

			auto s = util::pool<sst::LiteralString>(loc, strty);
			s->str = strings.vendor;

			vendor_def->init = s;
			fs->stree->addDefinition("vendor", vendor_def);
		}
	}








	using frontend::CollectorState;
	DefinitionTree* typecheck(CollectorState* cs, const parser::ParsedFile& file,
		const std::vector<std::pair<frontend::ImportThing, DefinitionTree*>>& imports, bool addPreludeDefinitions)
	{
		StateTree* tree = new StateTree(file.moduleName, file.name, 0);
		tree->treeDefn = util::pool<TreeDefn>(Location());
		tree->treeDefn->tree = tree;

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

				// iterate through the import-as list, which is a list of nested scopes to import into
				// eg we can `import foo as some::nested::namespace`, which means we need to create
				// the intermediate trees.

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
						auto newinspt = util::pool<sst::StateTree>(impas, file.name, curinspt);
						curinspt->subtrees[impas] = newinspt;

						auto treedef = util::pool<sst::TreeDefn>(cs->dtrees[ithing.name]->topLevel->loc);
						treedef->id = Identifier(impas, IdKind::Name);
						treedef->tree = newinspt;
						treedef->tree->treeDefn = treedef;
						treedef->visibility = VisibilityLevel::Public;

						curinspt->addDefinition(file.name, impas, treedef);

						curinspt = newinspt;
					}
				}

				insertPoint = curinspt;
			}

			iceAssert(insertPoint);

			_addTreeToExistingTree(fs->dtree->thingsImported, insertPoint, import->base, /* commonParent: */ nullptr, ithing.pubImport,
				/* ignoreVis: */ false, file.name);

			fs->dtree->thingsImported.insert(ithing.name);
			fs->dtree->typeDefnMap.insert(import->typeDefnMap.begin(), import->typeDefnMap.end());

			// merge the things. hopefully there are no conflicts????
			// TODO: check for conflicts!
			fs->dtree->compilerSupportDefinitions.insert(import->compilerSupportDefinitions.begin(),
				import->compilerSupportDefinitions.end());
		}

		if(addPreludeDefinitions)
			generatePreludeDefinitions(fs);

		// handle exception here:
		try {
			auto tns = dcast(NamespaceDefn, file.root->typecheck(fs).stmt());
			iceAssert(tns);

			tns->name = file.moduleName;

			fs->dtree->topLevel = tns;
		}
		catch (ErrorException& ee)
		{
			ee.err->postAndQuit();
		}

		return fs->dtree;
	}
}


static void visitDeclarables(sst::TypecheckState* fs, ast::TopLevelBlock* top)
{
	for(auto stmt : top->statements)
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
		// be able to see something before it is defined/declared

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

		// check for compiler support so we can add it to the big list of things.
		if((tcr.isStmt() || tcr.isDefn()) && tcr.stmt()->attrs.has(strs::attrs::COMPILER_SUPPORT))
		{
			if(!tcr.isDefn())
				error(tcr.stmt(), "@compiler_support can only be applied to definitions");

			auto ua = tcr.stmt()->attrs.get(strs::attrs::COMPILER_SUPPORT);
			iceAssert(!ua.name.empty() && ua.args.size() == 1);

			fs->dtree->compilerSupportDefinitions[ua.args[0]] = tcr.defn();
		}
	}

	if(tree->parent)
	{
		auto td = util::pool<sst::TreeDefn>(this->loc);

		td->tree = tree;
		td->tree->treeDefn = td;
		td->id = Identifier(this->name, IdKind::Name);
		td->id.scope = tree->parent->getScope();

		td->visibility = this->visibility;

		if(auto err = fs->checkForShadowingOrConflictingDefinition(td, [](auto, auto) -> bool { return true; }, tree->parent))
			return TCResult(err);

		tree->parent->addDefinition(tree->topLevelFilename, td->id.name, td);
	}


	if(this->name != "")
		fs->popTree();

	ret->name = this->name;

	return TCResult(ret);
}
















