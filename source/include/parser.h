// parser.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "lexer.h"

namespace ast
{
	struct TopLevelBlock;
}

namespace frontend
{
	struct ImportThing;
	struct CollectorState;
}

namespace parser
{
	struct ParsedFile
	{
		std::string name;
		std::string moduleName;

		ast::TopLevelBlock* root = 0;
	};


	struct CustomOperatorDecl
	{
		Location loc;
		std::string symbol;
		int precedence = 0;

		enum class Kind { Invalid, Infix, Prefix, Postfix };
		Kind kind = Kind::Invalid;
	};

	std::tuple<std::unordered_map<std::string, parser::CustomOperatorDecl>,	std::unordered_map<std::string, parser::CustomOperatorDecl>,
		std::unordered_map<std::string, parser::CustomOperatorDecl>> parseOperators(const lexer::TokenList& tokens);

	// strange api
	size_t parseOperatorDecl(const lexer::TokenList& tokens, size_t i, int* kind, CustomOperatorDecl* out);

	std::vector<frontend::ImportThing> parseImports(const std::string& filename, const lexer::TokenList& tokens);
	ParsedFile parseFile(std::string filename, frontend::CollectorState& cs);
}












