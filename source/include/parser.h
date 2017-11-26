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
}

namespace parser
{
	struct ParsedFile
	{
		std::string name;
		std::string moduleName;

		ast::TopLevelBlock* root = 0;
	};


	std::vector<frontend::ImportThing> parseImports(const std::string& filename, const lexer::TokenList& tokens);
	ParsedFile parseFile(std::string filename);
}
