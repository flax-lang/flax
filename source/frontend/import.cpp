// import.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <sys/stat.h>

#include "errors.h"
#include "frontend.h"

namespace frontend
{
	std::string resolveImport(std::string imp, const Location& loc, std::string fullPath)
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
}






namespace parser
{
	std::vector<std::pair<std::string, Location>> parseImports(const std::string& filename, const lexer::TokenList& tokens)
	{
		using Token = lexer::Token;
		using TT = lexer::TokenType;

		std::vector<std::pair<std::string, Location>> imports;

		// basically, this is how it goes:
		// only allow comments to occur before imports
		// all imports must happen before anything else in the file
		// comments can be interspersed between import statements, of course.
		for(size_t i = 0; i < tokens.size(); i++)
		{
			const Token& tok = tokens[i];
			if(tok == TT::Import)
			{
				i++;
				if(tokens[i] == TT::StringLiteral)
				{
					imports.push_back({ tokens[i].str(), tokens[i].loc });
					i++;
				}
				else
				{
					expectedAfter(tokens[i].loc, "string literal for path", "'import'", tokens[i].str());
				}

				if(tokens[i] != TT::NewLine && tokens[i] != TT::Semicolon && tokens[i] != TT::Comment)
				{
					error(tokens[i].loc, "Expected newline or semicolon to terminate import statement, found '%s'",
						tokens[i].str().c_str());
				}

				// i++ handled by loop
			}
			else if(tok == TT::Export)
			{
				// skip the name as well
				i++;
			}
			else if(tok == TT::Comment || tok == TT::NewLine)
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










