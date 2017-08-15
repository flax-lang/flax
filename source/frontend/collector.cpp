// collector.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <sys/stat.h>

#include <unordered_map>

#include "errors.h"
#include "frontend.h"
#include "typecheck.h"

namespace frontend
{
	static std::string resolveImport(std::string imp, const Location& loc, std::string fullPath);
	static std::vector<std::string> checkForCycles(std::string topmod, frontend::DependencyGraph* graph);


	static frontend::DependencyGraph* buildDependencyGraph(frontend::DependencyGraph* graph, std::string full,
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

	void collectFiles(std::string filename)
	{
		// first, collect and parse the first file
		std::string full = getFullPathOfFile(filename);

		auto graph = new DependencyGraph();

		std::unordered_map<std::string, bool> visited;
		auto files = checkForCycles(full, buildDependencyGraph(graph, full, visited));

		auto wholess = new sst::WholeSemanticState();

		std::map<std::string, parser::ParsedFile> parsed;
		for(auto file : files)
		{
			// parse it all
			debuglog("parsed %s\n", getFilenameFromPath(file).c_str());
			parsed[file] = parser::parseFile(file);

			auto fss = sst::typecheck(parsed[file]);
			sst::addFileToWhole(wholess, fss);
		}
	}








	static std::string resolveImport(std::string imp, const Location& loc, std::string fullPath)
	{
		std::string ext = ".flx";
		if(imp.find(".flx") == imp.size() - 4)
			ext = "";

		std::string curpath = getPathFromFile(fullPath);
		std::string fullname = curpath + "/" + imp + ext;
		char* fname = realpath(fullname.c_str(), 0);

		if(fullname == fullPath)
			error(loc, "Cannot import module from within itself");

		// a file here
		if(fname != NULL)
		{
			auto ret = std::string(fname);
			free(fname);

			return getFullPathOfFile(ret);
		}
		else
		{
			free(fname);
			std::string builtinlib = frontend::getParameter("sysroot") + "/" + frontend::getParameter("prefix") + "/lib/flaxlibs/" + imp + ext;

			struct stat buffer;
			if(stat(builtinlib.c_str(), &buffer) == 0)
			{
				return getFullPathOfFile(builtinlib);
			}
			else
			{
				exitless_error(loc, "No module or library at the path '%s' could be found", imp.c_str());
				info("'%s' does not exist", fullname.c_str());
				info("'%s' does not exist", builtinlib.c_str());

				doTheExit();
			}
		}
	}

	static std::vector<std::string> checkForCycles(std::string topmod, frontend::DependencyGraph* graph)
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

						info("here '%s'", u.first->name.c_str());

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
}


namespace parser
{

	std::vector<std::pair<std::string, Location>> parseImports(const std::string& filename, const lexer::TokenList& tokens)
	{
		using Token = lexer::Token;
		using TokenType = lexer::TokenType;

		std::vector<std::pair<std::string, Location>> imports;

		// basically, this is how it goes:
		// only allow comments to occur before imports
		// all imports must happen before anything else in the file
		// comments can be interspersed between import statements, of course.
		for(size_t i = 0; i < tokens.size(); i++)
		{
			const Token& tok = tokens[i];
			if(tok.type == TokenType::Import)
			{
				i++;

				if(tokens[i].type == TokenType::Identifier)
				{
					std::string name;
					while(tokens[i].type == TokenType::Identifier)
					{
						name += tokens[i].text.to_string();
						i++;

						if(tokens[i].type == TokenType::Period)
						{
							name += "/", i++;
						}
						else if(tokens[i].type == TokenType::NewLine || tokens[i].type == TokenType::Semicolon
							|| tokens[i].type == TokenType::Comment)
						{
							break;
						}
						else
						{
							error(tokens[i].loc, "Unexpected token '%s' (%d) in module specifier for import statement",
								tokens[i].text.to_string().c_str(), tokens[i].type);
						}
					}

					// i hope this works.
					imports.push_back({ name, tok.loc });
				}
				else if(tokens[i].type == TokenType::StringLiteral)
				{
					imports.push_back({ tokens[i].text.to_string(), tokens[i].loc });
					i++;
				}
				else
				{
					error(tokens[i].loc, "Expected path or module specifier after 'import'");
				}

				if(tokens[i].type != TokenType::NewLine && tokens[i].type != TokenType::Semicolon && tokens[i].type != TokenType::Comment)
				{
					error(tokens[i].loc, "Expected newline or semicolon to terminate import statement, found '%s'",
						tokens[i].text.to_string().c_str());
				}

				// i++ handled by loop
			}
			else if(tok.type == TokenType::Comment || tok.type == TokenType::NewLine)
			{
				// skipped
			}
			else
			{
				// stop imports.
				break;
			}
		}

		return imports;
	}
}








