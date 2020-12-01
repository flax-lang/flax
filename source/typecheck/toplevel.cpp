// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include <optional>

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
	static void generatePreludeDefinitions(TypecheckState* fs, const std::string& filename);

	static bool definitionsConflict(const sst::Defn* a, const sst::Defn* b)
	{
		auto fda = dcast(const sst::FunctionDecl, a);
		auto fdb = dcast(const sst::FunctionDecl, b);

		auto uva = dcast(const sst::UnionVariantDefn, a);
		auto uvb = dcast(const sst::UnionVariantDefn, b);

		if(fda && fdb)
		{
			return sst::isDuplicateOverload(fda->params, fdb->params);
		}
		else if(uva && uvb)
		{
			return uva->parentUnion == uvb->parentUnion;
		}
		else
		{
			return true;
		}
	}

	struct ExportMetadata
	{
		Location loc;
		std::string module;
		std::string imported;
	};

	static void checkConflictingDefinitions(Location loc, const char* kind, const sst::StateTree* base, const sst::StateTree* branch,
		std::optional<Location> importer = { }, std::optional<ExportMetadata> exporter = { })
	{
		for(const auto& [ name, defns ] : base->definitions)
		{
			if(auto it = branch->definitions.find(name); it != branch->definitions.end())
			{
				for(auto d1 : defns)
				{
					for(auto d2 : it->second)
					{
						if(!definitionsConflict(d1, d2))
							continue;

						auto error = SimpleError::make(MsgType::Error, loc, "'%s' here introduces duplicate definitions:", kind);

						if(d1 == d2)
						{
							if(importer && exporter)
							{
								error->append(SimpleError::make(MsgType::Note, exporter->loc,
									"this public import (from the imported module '%s') brings '%s' into scope ...",
									exporter->module, exporter->imported));

								error->append(SimpleError::make(MsgType::Note, *importer,
									"... which conflicts with this using/import statement here:"));
							}
							else if(importer)
							{
								error->append(SimpleError::make(MsgType::Note, *importer,
									"most likely caused by this import here:"));
							}
							else if(exporter)
							{
								error->append(SimpleError::make(MsgType::Note, exporter->loc,
									"most likely caused by this public import here, in module '%s':", exporter->module));
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

	static std::optional<ExportMetadata> getExportInfo(const sst::StateTree* base, const sst::StateTree* branch)
	{
		if(auto it = base->reexportMetadata.find(branch); it != base->reexportMetadata.end())
		{
			return ExportMetadata {
				.loc = it->second,
				.module = base->moduleName,
				.imported = branch->name
			};
		}
		return { };
	}

	static void checkExportsRecursively(const Location& loc, const char* kind, sst::StateTree* base, sst::StateTree* branch,
		std::optional<Location> importer = { }, std::optional<ExportMetadata> exporter = { })
	{
		if(branch->isAnonymous || branch->isCompilerGenerated)
			return;

		checkConflictingDefinitions(loc, kind, base, branch, importer, exporter);

		for(auto exp : base->reexports)
			checkExportsRecursively(loc, kind, exp, branch, importer, getExportInfo(base, exp));
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
			std::optional<Location> importer;
			if(auto it = base->importMetadata.find(import); it != base->importMetadata.end())
				importer = it->second;

			// check that the new tree doesn't trample over it.
			checkConflictingDefinitions(loc, kind, import, branch, importer);

			// then, recursively check every single re-export on *our* side for conflicts:
			for(auto rexp : import->reexports)
				checkExportsRecursively(loc, kind, rexp, branch, importer, getExportInfo(import, rexp));

			// then, also check that, for every one of *their* public imports:
			for(auto rexp : branch->reexports)
			{
				auto exportInfo = getExportInfo(import, rexp);

				// it doesn't trample with anything in our tree,
				checkConflictingDefinitions(loc, kind, base, rexp, importer, exportInfo);

				// and it doesn't conflict with anything in our imports.
				checkConflictingDefinitions(loc, kind, import, rexp, importer, exportInfo);

				// finally, also check that, for every one of *our* imports' re-exports,
				for(auto rexp2 : import->reexports)
				{
					auto exportInfo = getExportInfo(import, rexp2);

					// check that *they* don't trample anything:
					checkConflictingDefinitions(loc, kind, rexp2, branch, importer, exportInfo);

					// and neither does their reexport:
					checkConflictingDefinitions(loc, kind, rexp2, rexp, importer, exportInfo);
				}
			}
		}

		// no problem -- attach the trees
		base->imports.push_back(branch);
		base->importMetadata[branch] = loc;

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
		auto tree = new StateTree(file.moduleName, nullptr);
		tree->moduleName = file.moduleName;

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
						auto newinspt = curinspt->findOrCreateSubtree(impas);
						curinspt->subtrees[impas] = newinspt;

						curinspt = newinspt;
					}
				}

				insertPoint = curinspt;
				insertPoint->proxyOf = import->base;
			}

			iceAssert(insertPoint);
			mergeExternalTree(ithing.loc, "import", insertPoint, import->base);

			if(ithing.pubImport)
			{
				insertPoint->reexports.push_back(import->base);
				insertPoint->reexportMetadata[import->base] = ithing.loc;
			}

			fs->dtree->thingsImported.insert(ithing.name);
			fs->dtree->typeDefnMap.insert(import->typeDefnMap.begin(), import->typeDefnMap.end());

			// merge the things. hopefully there are no conflicts????
			// TODO: check for conflicts!
			fs->dtree->compilerSupportDefinitions.insert(import->compilerSupportDefinitions.begin(),
				import->compilerSupportDefinitions.end());
		}

		if(addPreludeDefinitions)
			generatePreludeDefinitions(fs, file.name);

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

	static void generatePreludeDefinitions(TypecheckState* fs, const std::string& filename)
	{
		auto loc = Location();
		loc.fileID = frontend::getFileIDFromFilename(filename);

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
			decl->enclosingScope = fs->scope();
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
















