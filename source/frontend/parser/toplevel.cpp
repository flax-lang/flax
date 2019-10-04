// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "errors.h"
#include "parser.h"
#include "frontend.h"

#include "parser_internal.h"

#include "memorypool.h"

using namespace lexer;
using namespace ast;

namespace parser
{
	std::vector<std::string> parseIdentPath(const lexer::TokenList& tokens, size_t* idx)
	{
		using TT = lexer::TokenType;
		std::vector<std::string> path;

		size_t i = *idx;
		while(tokens[i] == TT::Identifier)
		{
			path.push_back(tokens[i].str());
			i++;

			if(tokens[i] == TT::DoubleColon)
			{
				i++;
				if(tokens[i] != TT::Identifier)
					expectedAfter(tokens[i - 1].loc, "identifier", "'::' in path", tokens[i].str());
			}
			else
			{
				break;
			}
		}

		*idx = i;
		return path;
	}


	static std::pair<std::string, std::vector<std::string>> parseModuleName(const std::string& fullpath)
	{
		using TT = lexer::TokenType;
		auto tokens = frontend::getFileTokens(fullpath);

		std::vector<std::string> path;

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
					path = parseIdentPath(tokens, &i);
				}
				else
				{
					expectedAfter(tokens[i].loc, "identifier for export declaration", "'module'", tokens[i].str());
				}

				if(tokens[i].type != TT::NewLine && tokens[i].type != TT::Semicolon && tokens[i].type != TT::Comment)
				{
					expected(tokens[i].loc, "newline or semicolon to terminate export statement", tokens[i].str());
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

		if(path.empty())
			path = { frontend::removeExtensionFromFilename(frontend::getFilenameFromPath(fullpath)) };

		return { path.back(), util::take(path, path.size() - 1) };
	}



	TopLevelBlock* parseTopLevel(State& st, const std::string& name)
	{
		using TT = lexer::TokenType;
		TopLevelBlock* root = util::pool<TopLevelBlock>(st.loc(), name);

		// if it's not empty, then it's an actual user-defined namespace
		bool hadLBrace = false;
		if(name != "")
		{
			// expect "namespace FOO { ... }"

			hadLBrace = true;
			iceAssert(st.front() == TT::Identifier);
			st.eat();

			if(st.eat() != TT::LBrace)
				expected(st.ploc(), "'{' to start namespace declaration", st.prev().str());
		}

		bool isFirst = true;
		auto priv = VisibilityLevel::Invalid;
		size_t tix = (size_t) -1;


		while(st.hasTokens() && st.front() != TT::EndOfFile)
		{
			switch(st.front())
			{
				case TT::Import: {
					if(name != "" || !st.importsStillValid)
						error(st, "import statements are not allowed here");

					root->statements.push_back(parseImport(st));
				} break;

				case TT::Attr_Operator: {
					if(name != "" || !st.operatorsStillValid)
						error(st, "custom operator declarations are not allowed here");

					// just skip it.
					st.setIndex(parseOperatorDecl(st.getTokenList(), st.getIndex(), 0, 0));

					st.importsStillValid = false;
					st.nativeWordSizeStillValid = false;
				} break;

				case TT::Attr_Platform: {

					auto ret = parsePlatformDefn(st);

					if(ret) // sometimes we set a setting, but it doesn't need to have an AST node.
						root->statements.push_back(ret);

					st.importsStillValid = false;
					st.nativeWordSizeStillValid = false;
				} break;

				case TT::Namespace: {
					st.eat();
					Token tok = st.front();
					if(tok != TT::Identifier)
						expectedAfter(st, "identifier", "'namespace'", st.front().str());

					auto ns = parseTopLevel(st, tok.str());
					if(priv != VisibilityLevel::Invalid)
						ns->visibility = priv, priv = VisibilityLevel::Invalid, tix = (size_t) -1;

					root->statements.push_back(ns);

					st.importsStillValid = false;
					st.operatorsStillValid = false;
					st.nativeWordSizeStillValid = false;

				} break;

				case TT::Attr_NoMangle: {
					st.pop();
					auto stmt = parseStmt(st);
					if(!dcast(FuncDefn, stmt) && !dcast(VarDefn, stmt))
						error(st, "attribute '@nomangle' can only be applied on function and variable declarations");

					else if(dcast(ForeignFuncDefn, stmt))
						warn(st, "attribute '@nomangle' is redundant on 'ffi' functions");

					else if(auto fd = dcast(FuncDefn, stmt))
						fd->noMangle = true;

					else if(auto vd = dcast(VarDefn, stmt))
						vd->noMangle = true;

					root->statements.push_back(stmt);

					st.importsStillValid = false;
					st.operatorsStillValid = false;
					st.nativeWordSizeStillValid = false;

				} break;

				case TT::Attr_EntryFn: {
					st.pop();
					auto stmt = parseStmt(st);
					if(auto fd = dcast(FuncDefn, stmt))
						fd->isEntry = true;

					else
						error(st, "'@entry' attribute is only applicable to function definitions");

					root->statements.push_back(stmt);

					st.importsStillValid = false;
					st.operatorsStillValid = false;
					st.nativeWordSizeStillValid = false;

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
					if(!hadLBrace) error(st, "unexpected '}'");
					goto out;

				default: {
					if(priv != VisibilityLevel::Invalid)
					{
						st.rewindTo(tix);

						tix = (size_t) -1;
						priv = VisibilityLevel::Invalid;
					}

					if(st.front() == TT::Export)
					{
						if(!isFirst || name != "")
						{
							error(st, "export declaration not allowed here (%s / %d)", name, isFirst);
						}
						else
						{
							st.eat();

							size_t i = st.getIndex();
							parseIdentPath(st.getTokenList(), &i);
							st.setIndex(i);

							break;
						}
					}

					st.importsStillValid = false;
					st.operatorsStillValid = false;
					st.nativeWordSizeStillValid = false;

					root->statements.push_back(parseStmt(st, /* allowExprs: */ false));
				} break;
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

		// throw in all anonymous types to the top level
		root->statements.insert(root->statements.begin(), st.anonymousTypeDefns.begin(), st.anonymousTypeDefns.end());

		return root;
	}

	ParsedFile parseFile(std::string filename, frontend::CollectorState& cs)
	{
		auto full = frontend::getFullPathOfFile(filename);
		const TokenList& tokens = frontend::getFileTokens(full);
		auto state = State(tokens);
		state.currentFilePath = full;

		// copy this stuff over.
		state.binaryOps = cs.binaryOps;
		state.prefixOps = cs.prefixOps;
		state.postfixOps = cs.postfixOps;

		state.cState = &cs;

		auto [ modname, modpath ] = parseModuleName(full);
		auto toplevel = parseTopLevel(state, "");


		return ParsedFile {
			filename,
			modname,
			modpath,
			toplevel,
		};
	}
}





































