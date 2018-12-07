// import.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <sys/stat.h>

#include "errors.h"
#include "frontend.h"
#include "parser_internal.h"

namespace frontend
{
	// map from (imp, fullPath) -> resolvedPath
	static ska::flat_hash_map<std::pair<std::string, std::string>, std::string> importCache;

	// 'imp' is always a path
	std::string resolveImport(const std::string& imp, const Location& loc, const std::string& fullPath)
	{
		if(auto it = importCache.find({ imp, fullPath }); it != importCache.end())
			return it->second;

		// std::string ext = ".flx";
		// if(imp.size() > ext.size() && imp.find(".flx") == imp.size() - ext.size())
		// 	ext = "";

		std::string curpath = getPathFromFile(fullPath);
		std::string fullname = curpath + "/" + imp;

		if(fullname == fullPath)
			error(loc, "cannot import module from within itself");

		std::string resolved;

		// a file here
		if(auto fname = platform::getFullPath(fullname); !fname.empty())
		{
			resolved = fname;
		}
		else
		{
			std::string builtinlib = frontend::getParameter("sysroot") + "/" + frontend::getParameter("prefix") + "/lib/flaxlibs/" + imp;

			if(platform::checkFileExists(builtinlib))
			{
				resolved = getFullPathOfFile(builtinlib);
			}
			else
			{
				SimpleError::make(loc, "no module or library at the path '%s' could be found", imp)
					->append(BareError::make(MsgType::Note, "'%s' does not exist", fullname))
					->append(BareError::make(MsgType::Note, "'%s' does not exist", builtinlib))
					->postAndQuit();
			}
		}


		importCache[{ imp, fullPath }] = resolved;
		return resolved;
	}
}






namespace parser
{
	// TODO: do we want to combine this "pre-parsing" with the actual import parsing??
	// note that the 'real' parse that we do (to make an AST) makes a useless AST, because we have no
	// use for imports after the files are collected.

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
			if(tok == TT::Import || ((tok == TT::Public || tok == TT::Private) && tokens[i + 1] == TT::Import))
			{
				bool pub = false;
				if(tok == TT::Public)       i++, pub = true;
				else if(tok == TT::Private) i++, warn(tok.loc, "imports are private by default, 'private import' is redundant");

				i++;

				Location impLoc;
				std::string name;
				std::vector<std::string> impAs;

				if(tokens[i] == TT::StringLiteral)
				{
					name = tokens[i].str();
					impLoc = tokens[i].loc;
					i++;
				}
				else if(tokens[i] == TT::Identifier)
				{
					std::vector<std::string> bits = parseIdentPath(tokens, &i);

					//* we concatanate the thing, using '/' as the path separator, and appending '.flx' to the end.
					name = util::join(bits, "/") + ".flx";
				}
				else
				{
					expectedAfter(tokens[i].loc, "string literal or identifier path", "'import'", tokens[i].str());
				}

				// check for 'import as foo'
				if(tokens[i] == TT::As)
				{
					i++;
					if(tokens[i] == TT::Identifier)
						impAs = parseIdentPath(tokens, &i);

					else
						expectedAfter(tokens[i - 1].loc, "identifier", "'import-as'", tokens[i - 1].str());
				}


				if(tokens[i] != TT::NewLine && tokens[i] != TT::Semicolon && tokens[i] != TT::Comment)
				{
					error(tokens[i].loc, "expected newline or semicolon to terminate import statement, found '%s'", tokens[i].str());
				}

				frontend::ImportThing it { name, impAs, pub, impLoc };
				imports.push_back(it);

				// i++ handled by loop
			}
			else if(tok == TT::Export)
			{
				// skip until a newline.
				while(tokens[i] != TT::Comment && tokens[i] != TT::NewLine)
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










