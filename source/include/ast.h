// ast.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

#include <vector>

namespace ast
{
	struct Stmt
	{
		Stmt(const Location& l) : loc(l) { }
		virtual ~Stmt();

		Location loc;
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { }
		~Expr();
	};

	struct ImportStmt : Stmt
	{
		ImportStmt(const Location& l, std::string p) : Stmt(l), path(p) { }
		~ImportStmt();

		std::string path;
	};








	struct Ident : Expr
	{
		Ident(const Location& l, std::string n) : Expr(l), name(n) { }
		~Ident();

		std::string name;
	};





	struct TopLevelBlock : Expr
	{
		TopLevelBlock(const Location& l, std::string n) : Expr(l), name(n) { }
		~TopLevelBlock();

		std::string name;
		std::vector<Stmt*> statements;
	};
}






