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

		auto [ priv, tix ] = std::make_tuple(PrivacyLevel::Invalid, -1);
		while(st.hasTokens() && st.front() != TT::EndOfFile)
		{
			switch(st.front())
			{
				case TT::Import:
					root->statements.push_back(parseImport(st));
					break;

				case TT::Namespace: {
					st.eat();
					Token tok = st.front();
					if(tok != TT::Identifier)
						expectedAfter(st, "identifier", "'namespace'", st.front().str());

					auto ns = parseTopLevel(st, tok.str());
					if(priv != PrivacyLevel::Invalid)
						ns->privacy = priv, priv = PrivacyLevel::Invalid, tix = -1;

					root->statements.push_back(ns);

				} break;

				case TT::Public:
					priv = PrivacyLevel::Public;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Private:
					priv = PrivacyLevel::Private;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Internal:
					priv = PrivacyLevel::Internal;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Comment:
				case TT::NewLine:
					break;

				case TT::RBrace:
					goto out;

				default:
					if(priv != PrivacyLevel::Invalid)
					{
						st.rewindTo(tix);

						tix = -1;
						priv = PrivacyLevel::Invalid;
					}

					root->statements.push_back(parseStmt(st));
					break;
			}

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
		const TokenList& tokens = frontend::getFileTokens(filename);
		auto state = State(tokens);

		auto toplevel = parseTopLevel(state, "");

		return ParsedFile {
			.name = filename,
			.root = toplevel
		};
	}







}
















