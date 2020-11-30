// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
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
	struct OsStrings
	{
		std::string name;
		std::string vendor;
	};

	static OsStrings getOsStrings();
	static void generatePreludeDefinitions(TypecheckState* fs);

	static bool definitionsConflict(const sst::Defn* a, const sst::Defn* b)
	{
		auto fda = dcast(const sst::FunctionDecl, a);
		auto fdb = dcast(const sst::FunctionDecl, b);

		if(fda && fdb)
		{
			return sst::isDuplicateOverload(fda->params, fdb->params);
		}
		else
		{
			return true;
		}
	}

	static void checkConflictingDefinitions(Location loc, const char* kind, const sst::StateTree* base, const sst::StateTree* branch,
		const sst::StateTree* rootBase = nullptr)
	{
		for(const auto& [ name, defns ] : base->definitions2)
		{
			if(auto it = branch->definitions2.find(name); it != branch->definitions2.end())
			{
				for(auto d1 : defns)
				{
					for(auto d2 : it->second)
					{
						if(definitionsConflict(d1, d2))
						{
							auto error = SimpleError::make(MsgType::Error, loc, "'%s' here introduces duplicate definitions:", kind);

							if(d1 == d2 || d1->loc == d2->loc)
							{
								if(rootBase != nullptr)
								{
									if(auto it = rootBase->importMetadata.find(base); it != rootBase->importMetadata.end())
									{
										auto [ iloc, name ] = it->second;
										error->append(SimpleError::make(MsgType::Note, iloc, "most likely, the module '%s' was "
											"already brought into the current scope by this statement:", name));
									}
								}

								error->append(SimpleError::make(MsgType::Note, d1->loc, "for reference, here is the (first) "
									"conflicting definition:"));
							}
							else
							{
								error->append(SimpleError::make(MsgType::Note, d1->loc, "first definition here:"))
									->append(SimpleError::make(MsgType::Note, d2->loc, "second definition here:"));
							}

							error->postAndQuit();
						}
					}
				}
			}
		}
	}

	void mergeExternalTree(const Location& loc, const char* kind, sst::StateTree* base, sst::StateTree* branch)
	{
		if(branch->isAnonymous || branch->isCompilerGenerated)
			return;

		// first check conflicts for this level:
		checkConflictingDefinitions(loc, kind, base, branch);

		// then, for every one of *our* imports:
		for(auto import : base->imports)
		{
			// check that the new tree doesn't trample over it.
			checkConflictingDefinitions(loc, kind, import, branch, base);

			// then, also check that, for every one of *their* public imports:
			for(auto rexp : branch->reexports)
			{
				// it doesn't trample with anything in our tree,
				checkConflictingDefinitions(loc, kind, base, rexp);

				// and it doesn't conflict with anything in our imports.
				checkConflictingDefinitions(loc, kind, import, rexp, base);
			}
		}

		// no problem -- attach the trees
		base->imports.push_back(branch);
		base->importMetadata[branch] = { loc, branch->name };

		// merge the subtrees as well.
		for(const auto& [ name, tr ] : branch->subtrees)
		{
			if(tr->isCompilerGenerated || tr->isAnonymous)
				continue;

			mergeExternalTree(loc, kind, base->findOrCreateSubtree(name), tr);
		}
	}













	DefinitionTree* typecheck(frontend::CollectorState* cs, const parser::ParsedFile& file,
		const std::vector<std::pair<frontend::ImportThing, DefinitionTree*>>& imports, bool addPreludeDefinitions)
	{
		auto tree = new StateTree(file.moduleName, file.name, 0);
		auto fs = new TypecheckState(tree);

		for(auto [ ithing, import ] : imports)
		{
			// info(ithing.loc, "(%s) import: %s", file.name, ithing.name);

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

						curinspt = newinspt;
					}
				}

				insertPoint = curinspt;
			}

			iceAssert(insertPoint);
			mergeExternalTree(ithing.loc, "import", insertPoint, import->base);

			if(ithing.pubImport)
				insertPoint->reexports.push_back(import->base);

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
		catch(ErrorException& ee)
		{
			ee.err->postAndQuit();
		}

		return fs->dtree;
	}


	static OsStrings getOsStrings()
	{
		// TODO: handle cygwin/msys/mingw???
		// like how do we want to expose these? at the end of the day the os is still windows...

		// TODO: this should be set for the target we are compiling FOR, so it definitely
		// cannot be done using ifdefs!!!!!!!!!!

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
		loc.fileID = frontend::getFileIDFromFilename(fs->stree->topLevelFilename);

		auto strings = getOsStrings();

		fs->pushTree("os");
		fs->stree->isCompilerGenerated = true;

		defer(fs->popTree());

		auto strty = fir::Type::getCharSlice(false);

		{
			// add the name
			auto name_def = util::pool<sst::VarDefn>(loc);
			name_def->id = Identifier("name", IdKind::Name);
			name_def->type = strty;
			name_def->global = true;
			name_def->immutable = true;
			name_def->visibility = VisibilityLevel::Private;

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
			vendor_def->visibility = VisibilityLevel::Private;

			auto s = util::pool<sst::LiteralString>(loc, strty);
			s->str = strings.vendor;

			vendor_def->init = s;
			fs->stree->addDefinition("vendor", vendor_def);
		}
	}
}


static void visitDeclarables(sst::TypecheckState* fs, ast::TopLevelBlock* top)
{
	for(auto stmt : top->statements)
	{
		if(auto decl = dcast(ast::Parameterisable, stmt))
		{
			decl->enclosingScope = fs->getCurrentScope2();
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

		if(tcr.isDefn() && tcr.defn()->visibility == VisibilityLevel::Public)
			tree->exports.push_back(tcr.defn());

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

	if(this->name != "")
		fs->popTree();

	ret->name = this->name;

	return TCResult(ret);
}
















