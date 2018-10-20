// import.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <sys/stat.h>

#include "errors.h"
#include "frontend.h"

namespace frontend
{
	std::string resolveImport(const std::string& imp, const Location& loc, const std::string& fullPath)
	{
		std::string ext = ".flx";
		if(imp.find(".flx") == imp.size() - 4)
			ext = "";

		std::string curpath = getPathFromFile(fullPath);
		std::string fullname = curpath + "/" + imp + ext;

		if(fullname == fullPath)
			error(loc, "Cannot import module from within itself");

		auto fname = platform::getFullPath(fullname);

		// a file here
		if(!fname.empty())
		{
			return fname;
		}
		else
		{
			std::string builtinlib = frontend::getParameter("sysroot") + "/" + frontend::getParameter("prefix") + "/lib/flaxlibs/" + imp + ext;

			if(platform::checkFileExists(builtinlib))
			{
				return getFullPathOfFile(builtinlib);
			}
			else
			{
				SimpleError::make(loc, "No module or library at the path '%s' could be found", imp)
					->append(BareError::make(MsgType::Note, "'%s' does not exist", fullname))
					->append(BareError::make(MsgType::Note, "'%s' does not exist", builtinlib))
					->postAndQuit();
			}
		}
	}
}






namespace parser
{
	std::vector<frontend::ImportThing> parseImports(const std::string& filename, const lexer::TokenList& tokens)
	{
		using Token = lexer::Token;
		using TT = lexer::TokenType;

		std::vector<frontend::ImportThing> imports;

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

				Location impLoc;
				std::string name;
				std::string impAs;

				if(tokens[i] == TT::StringLiteral)
				{
					name = tokens[i].str();
					impLoc = tokens[i].loc;
					i++;
				}
				else
				{
					expectedAfter(tokens[i].loc, "string literal for path", "'import'", tokens[i].str());
				}

				// check for 'import as foo'
				if(tokens[i] == TT::As)
				{
					i++;
					if(tokens[i] == TT::Identifier)
						impAs = tokens[i].str();

					else
						expectedAfter(tokens[i - 1].loc, "identifier", "'import-as'", tokens[i - 1].str());

					i++;
				}


				if(tokens[i] != TT::NewLine && tokens[i] != TT::Semicolon && tokens[i] != TT::Comment)
				{
					error(tokens[i].loc, "Expected newline or semicolon to terminate import statement, found '%s'", tokens[i].str());
				}

				frontend::ImportThing it { name, impAs, impLoc };
				imports.push_back(it);

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










