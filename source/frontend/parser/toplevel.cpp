// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "errors.h"
#include "parser.h"
#include "frontend.h"

#include "parser_internal.h"

using namespace lexer;
using namespace ast;

namespace parser
{

	static std::string parseModuleName(std::string fullpath)
	{
		using TT = TokenType;
		auto tokens = frontend::getFileTokens(fullpath);

		std::string ret;

		// basically, this is how it goes:
		// only allow comments to occur before the module name.
		for(size_t i = 0; i < tokens.size(); i++)
		{
			const Token& tok = tokens[i];
			if(tok.type == TT::Export)
			{
				i++;

				if(tokens[i].type == TT::Identifier)
				{
					ret = tokens[i].str();
					i++;
				}
				else
				{
					expectedAfter(tokens[i].loc, "identifier for export declaration", "'module'", tokens[i].str());
				}


				if(tokens[i].type != TT::NewLine && tokens[i].type != TT::Semicolon && tokens[i].type != TT::Comment)
				{
					error(tokens[i].loc, "Expected newline or semicolon to terminate import statement, found '%s'",
						tokens[i].str());
				}

				// i++ handled by loop
			}
			else if(tok.type == TT::Comment || tok.type == TT::NewLine)
			{
				// skipped
			}
			else
			{
				// stop
				break;
			}
		}

		if(ret == "")
			ret = frontend::removeExtensionFromFilename(frontend::getFilenameFromPath(fullpath));

		return ret;
	}



	TopLevelBlock* parseTopLevel(State& st, std::string name)
	{
		using TT = TokenType;
		TopLevelBlock* root = new TopLevelBlock(st.loc(), name);

		// if it's not empty, then it's an actual user-defined namespace
		if(name != "")
		{
			// expect "namespace FOO { ... }"
			iceAssert(st.eat() == TT::Identifier);
			if(st.eat() != TT::LBrace)
				expected(st.ploc(), "'{' to start namespace declaration", st.prev().str());
		}

		bool isFirst = true;
		auto [ priv, tix ] = std::make_tuple(VisibilityLevel::Invalid, -1);
		while(st.hasTokens() && st.front() != TT::EndOfFile)
		{
			switch(st.front())
			{
				case TT::Import:
					if(name != "")
						error(st, "Import statements are not allowed here");

					root->statements.push_back(parseImport(st));
					break;

				case TT::Namespace: {
					st.eat();
					Token tok = st.front();
					if(tok != TT::Identifier)
						expectedAfter(st, "identifier", "'namespace'", st.front().str());

					auto ns = parseTopLevel(st, tok.str());
					if(priv != VisibilityLevel::Invalid)
						ns->visibility = priv, priv = VisibilityLevel::Invalid, tix = -1;

					root->statements.push_back(ns);

				} break;

				case TT::Attr_NoMangle: {
					st.pop();
					auto stmt = parseStmt(st);
					if(!dynamic_cast<FuncDefn*>(stmt) && !dynamic_cast<VarDefn*>(stmt))
						error(st, "'@nomangle' can only be applied on function and variable declarations");

					else if(dynamic_cast<ForeignFuncDefn*>(stmt))
						warn(st, "Attribute '@nomangle' is redundant on 'ffi' functions");

					else if(auto fd = dynamic_cast<FuncDefn*>(stmt))
						fd->noMangle = true;

					else if(auto vd = dynamic_cast<VarDefn*>(stmt))
						vd->noMangle = true;

					root->statements.push_back(stmt);
				} break;

				case TT::Attr_EntryFn: {
					st.pop();
					auto stmt = parseStmt(st);
					if(auto fd = dynamic_cast<FuncDefn*>(stmt))
						fd->isEntry = true;

					else
						error(st, "'@entry' attribute is only applicable to function definitions");

					root->statements.push_back(stmt);
				} break;

				case TT::Public:
					priv = VisibilityLevel::Public;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Private:
					priv = VisibilityLevel::Private;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Internal:
					priv = VisibilityLevel::Internal;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Comment:
				case TT::NewLine:
					isFirst = true;
					st.skipWS();
					continue;

				case TT::RBrace:
					goto out;

				default:
					if(priv != VisibilityLevel::Invalid)
					{
						st.rewindTo(tix);

						tix = -1;
						priv = VisibilityLevel::Invalid;
					}

					if(st.front() == TT::Export)
					{
						if(!isFirst || name != "")
						{
							error(st, "Export declaration not allowed here (%s / %d)", name, isFirst);
						}
						else
						{
							st.eat();
							st.eat();

							break;
						}
					}

					root->statements.push_back(parseStmt(st));
					break;
			}

			isFirst = false;
			st.skipWS();
		}

		out:
		if(name != "")
		{
			if(st.front() != TT::RBrace)
				expected(st, "'}' to close namespace declaration", st.front().str());

			st.eat();
		}

		return root;
	}

	ParsedFile parseFile(std::string filename)
	{
		auto full = frontend::getFullPathOfFile(filename);
		const TokenList& tokens = frontend::getFileTokens(full);
		auto state = State(tokens);
		state.currentFilePath = full;

		auto modname = parseModuleName(full);
		auto toplevel = parseTopLevel(state, "");

		// debuglog("module -> %s\n", modname);

		return ParsedFile {
			.name = filename,
			.root = toplevel,
			.moduleName = modname
		};
	}
}





































