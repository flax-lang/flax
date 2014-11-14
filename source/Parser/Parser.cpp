// Parser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <fstream>
#include <cassert>
#include "../include/ast.h"
#include "../include/parser.h"

using namespace Ast;

namespace Parser
{
	PosInfo pos;
	Root* rootNode;

	// why bother with allocating a std::string
	static Expr* error(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		char* alloc = nullptr;
		vasprintf(&alloc, msg, ap);

		fprintf(stderr, "Error (%s:%lld): %s\n\n", pos.file->c_str(), pos.line, alloc);

		va_end(ap);
		exit(1);
	}


	// woah shit it's forward declarations
	// note: all these are expected to pop at least one token from the front of the list.

	Var* parseVar(std::deque<Token*>& tokens);
	Expr* parseExpr(std::deque<Token*>& tokens);
	Expr* parseIdExpr(std::deque<Token*>& tokens);
	Expr* parsePrimary(std::deque<Token*>& tokens);
	Number* parseNumber(std::deque<Token*>& tokens);
	Closure* parseClosure(std::deque<Token*>& tokens);
	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens);
	Expr* parseParenthesised(std::deque<Token*>& tokens);
	Expr* parseRhs(std::deque<Token*>& tokens, Expr* expr, int prio);
	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id);

	void Parse(std::string filename)
	{
		// open the file.
		std::ifstream file = std::ifstream(filename);
		std::stringstream stream;

		stream << file.rdbuf();
		std::string str = stream.str();

		Token* t = nullptr;
		pos.file = new std::string(filename);
		pos.line = 0;

		std::deque<Token*> tokens;

		while((t = getNextToken(str, pos)) != nullptr)
		{
			// printf("TOKEN: TYPE(%02d), TEXT(%s)\n", t->type, t->text.c_str());
			tokens.push_back(t);
		}

		rootNode = new Root();
		parsePrimary(tokens);


		printf("\n\n\n");
		rootNode->print();
	}

	// helpers
	static Token* eat(std::deque<Token*>& tokens)
	{
		// returns the current front, then pops front.
		Token* t = tokens.front();
		tokens.pop_front();

		return t;
	}

	static void skipNewline(std::deque<Token*>& tokens)
	{
		while(tokens.front()->type == TType::NewLine)
			eat(tokens);
	}

	static int getOpPrec(Token* tok)
	{
		switch(tok->type)
		{
			case TType::Plus:
			case TType::Minus:
				return 20;

			case TType::Asterisk:
			case TType::Divide:
				return 40;

			case TType::Equal:
				return 1;

			default:
				return -1;
		}
	}























	Expr* parsePrimary(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
		{
			printf("Done parsing.\n\n");
			return nullptr;
		}

		printf("parsePrimary\n");
		while(Token* tok = tokens.front())
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				case TType::Var:
				case TType::Val:
					return parseVar(tokens);

				case TType::Func:
					rootNode->functions.push_back(parseFuncDecl(tokens));
					return parsePrimary(tokens);

				case TType::LParen:
					return parseParenthesised(tokens);

				case TType::Identifier:
					return parseIdExpr(tokens);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(tokens);

				case TType::NewLine:
				case TType::Comment:
					tokens.pop_front();
					return parsePrimary(tokens);

				default:	// wip: skip shit we don't know/care about for now
					fprintf(stderr, "Warning: unknown token type %d, not handled\n", tok->type);
					tokens.pop_front();
					break;
			}
		}

		return nullptr;
	}

	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens)
	{
		printf("parseFuncDecl\n");


		assert(eat(tokens)->type == TType::Func);
		if(tokens.front()->type != TType::Identifier)
			error("Expected identifier, but got token of type %d", tokens.front()->type);

		std::string id = tokens.front()->text;
		eat(tokens);

		// expect a left bracket
		if(eat(tokens)->type != TType::LParen)
			error("Expected '(' in function declaration");

		// get the parameter list
		// expect an identifer, colon, type
		std::deque<Var*> params;
		while(tokens.size() > 0 && tokens.front()->type != TType::RParen)
		{
			Token* tok_id;
			if((tok_id = eat(tokens))->type != TType::Identifier)
				error("Expected identifier");

			std::string id = tok_id->text;
			Var* v = new Var(id, true);

			// expect a colon
			if(eat(tokens)->type != TType::Colon)
				error("Expected ':' followed by a type");

			Token* tok_type;
			if((tok_type = eat(tokens))->type != TType::Identifier)
				error("Expected type after parameter");

			v->type = tok_type->text;
			params.push_back(v);
		}

		// consume the closing paren
		eat(tokens);

		// get return type.
		std::string ret;
		if(tokens.front()->type != TType::LBrace && tokens.front()->type != TType::NewLine)
		{
			if(eat(tokens)->type != TType::Arrow)
				error("Expected '->' to indicate return type when not returning void.");

			Token* tok_type;
			if((tok_type = eat(tokens))->type != TType::Identifier)
				error("Expected type after parameter");

			ret = tok_type->text;
		}
		else
		{
			ret = "void";
		}

		skipNewline(tokens);
		return new FuncDecl(new Id(id), params, parseClosure(tokens), ret);
	}

	Closure* parseClosure(std::deque<Token*>& tokens)
	{
		printf("parseClosure\n");

		Closure* c = new Closure();

		// make sure the first token is a left brace.
		if(eat(tokens)->type != TType::LBrace)
			error("Expected '{'");

		skipNewline(tokens);

		// get the stuff inside.
		while(tokens.size() > 0 && tokens.front()->type != TType::RBrace)
		{
			c->statements.push_back(parseExpr(tokens));
			skipNewline(tokens);
		}

		if(eat(tokens)->type != TType::RBrace)
			error("Expected '}'");

		return c;
	}

	Var* parseVar(std::deque<Token*>& tokens)
	{
		printf("parseVar\n");
		assert(tokens.front()->type == TType::Var || tokens.front()->type == TType::Val);

		bool immutable = tokens.front()->type == TType::Val;
		eat(tokens);

		// get the identifier.
		Token* tok_id;
		if((tok_id = eat(tokens))->type != TType::Identifier)
			error("Expected identifier for variable declaration.");

		std::string id = tok_id->text;
		Var* v = new Var(id, immutable);

		// check the type.
		// todo: type inference
		if(eat(tokens)->type != TType::Colon)
			error("Expected colon to indicate type for variable declaration");

		Token* tok_type;
		if((tok_type = eat(tokens))->type != TType::Identifier)
			error("Expected type for variable declaration");

		v->type = tok_type->text;

		// TODO:
		// check if we have a default value

		return v;
	}

	Expr* parseParenthesised(std::deque<Token*>& tokens)
	{
		printf("parseParenthesised\n");
		assert(tokens.front()->type == TType::LParen);
		eat(tokens);

		Expr* within = parseExpr(tokens);

		if(eat(tokens)->type != TType::RParen)
			error("Expected ')'");

		return within;
	}

	Expr* parseExpr(std::deque<Token*>& tokens)
	{
		printf("parseExpr\n");
		Expr* lhs = parsePrimary(tokens);
		if(!lhs)
			return nullptr;

		return parseRhs(tokens, lhs, 0);
	}

	Expr* parseRhs(std::deque<Token*>& tokens, Expr* lhs, int prio)
	{
		printf("parseRhs\n");
		while(true)
		{
			int prec = getOpPrec(tokens.front());
			if(prec < prio)
				return lhs;

			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything
			Token* tok_op = eat(tokens);

			Expr* rhs = parsePrimary(tokens);
			if(!rhs)
				return nullptr;

			int next = getOpPrec(tokens.front());
			if(prec < next)
			{
				rhs = parseRhs(tokens, rhs, prec + 1);
				if(!rhs)
					return nullptr;
			}

			lhs = new BinOp(lhs, tok_op->text[0], rhs);
		}
	}

	Expr* parseIdExpr(std::deque<Token*>& tokens)
	{
		printf("parseIdExpr\n");
		assert(tokens.front()->type == TType::Identifier);
		std::string id = eat(tokens)->text;

		// todo: handle function calling
		skipNewline(tokens);

		if(tokens.front()->type != TType::LParen)
			return new Var(id, false);
		else
			return parseFunctionCall(tokens, id);
	}

	Number* parseNumber(std::deque<Token*>& tokens)
	{
		printf("parseNumber\n");
		Number* n;
		if(tokens.front()->type == TType::Integer)
		{
			n = new Number((int64_t) std::stoll(eat(tokens)->text));
		}
		else if(tokens.front()->type == TType::Decimal)
		{
			n = new Number(std::stod(eat(tokens)->text));
		}
		else
		{
			error("What!????");
			assert(false);
			return nullptr;
		}

		return n;
	}

	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id)
	{
		printf("parseFunctionCall\n");
		assert(eat(tokens)->type == TType::LParen);


		std::deque<Expr*> args;
		if(tokens.front()->type != TType::RParen)
		{
			while(true)
			{
				Expr* arg = parseExpr(tokens);
				if(arg == nullptr)
					return nullptr;

				args.push_back(arg);

				if(tokens.front()->type == TType::RParen)
				{
					eat(tokens);
					break;
				}

				if(eat(tokens)->type != TType::Comma)
					error("Expected either ',' or ')' in parameter list.");
			}
		}
		else
		{
			eat(tokens);
		}

		return new FuncCall(new Id(id), args);
	}
}

























