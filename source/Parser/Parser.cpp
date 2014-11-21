// Parser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <cfloat>
#include <fstream>
#include <cassert>
#include "../include/ast.h"
#include "../include/parser.h"

using namespace Ast;


namespace Parser
{
	PosInfo pos;
	Root* rootNode;
	Token* curtok;
	std::string modname;

	// todo: hack
	bool isParsingStruct;

	void error(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		char* alloc = nullptr;
		vasprintf(&alloc, msg, ap);

		fprintf(stderr, "Error (%s:%lld): %s\n\n", curtok->posinfo.file.c_str(), curtok->posinfo.line, alloc);

		va_end(ap);
		exit(1);
	}


	// woah shit it's forward declarations
	// note: all these are expected to pop at least one token from the front of the list.

	Expr* parseIf(std::deque<Token*>& tokens);
	void parseAll(std::deque<Token*>& tokens);
	Func* parseFunc(std::deque<Token*>& tokens);
	Expr* parseExpr(std::deque<Token*>& tokens);
	Expr* parseUnary(std::deque<Token*>& tokens);
	Expr* parseIdExpr(std::deque<Token*>& tokens);
	Expr* parsePrimary(std::deque<Token*>& tokens);
	Struct* parseStruct(std::deque<Token*>& tokens);
	Import* parseImport(std::deque<Token*>& tokens);
	Return* parseReturn(std::deque<Token*>& tokens);
	Number* parseNumber(std::deque<Token*>& tokens);
	std::string parseType(std::deque<Token*>& tokens);
	VarDecl* parseVarDecl(std::deque<Token*>& tokens);
	Closure* parseClosure(std::deque<Token*>& tokens);
	Func* parseTopLevelExpr(std::deque<Token*>& tokens);
	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens);
	Expr* parseParenthesised(std::deque<Token*>& tokens);
	OpOverload* parseOpOverload(std::deque<Token*>& tokens);
	StringLiteral* parseStringLiteral(std::deque<Token*>& tokens);
	ForeignFuncDecl* parseForeignFunc(std::deque<Token*>& tokens);
	Expr* parseRhs(std::deque<Token*>& tokens, Expr* expr, int prio);
	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id);


	std::string getModuleName()
	{
		return modname;
	}

	Root* Parse(std::string filename, std::string str)
	{
		Token* t = nullptr;
		pos.file = filename;
		pos.line = 1;

		std::deque<Token*> tokens;

		while((t = getNextToken(str, pos)) != nullptr)
			tokens.push_back(t);

		rootNode = new Root();
		parseAll(tokens);


		size_t lastdot = filename.find_last_of(".");
		modname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));

		size_t sep = modname.find_last_of("\\/");
		if(sep != std::string::npos)
			modname = modname.substr(sep + 1, modname.length() - sep - 1);

		return rootNode;
	}

	// helpers

	static void skipNewline(std::deque<Token*>& tokens)
	{
		while(tokens.size() > 0 && tokens.front()->type == TType::NewLine)
			tokens.pop_front();
	}

	static Token* eat(std::deque<Token*>& tokens)
	{
		// returns the current front, then pops front.
		if(tokens.size() == 0)
			error("Unexpected end of input");

		Token* t = tokens.front();
		tokens.pop_front();
		skipNewline(tokens);

		curtok = t;
		return t;
	}

	static bool checkHasMore(std::deque<Token*>& tokens)
	{
		return tokens.size() > 0;
	}

	static int getOpPrec(Token* tok)
	{
		switch(tok->type)
		{
			case TType::As:
				return 200;

			case TType::DoublePlus:
			case TType::DoubleMinus:
				return 100;

			case TType::Asterisk:
			case TType::Divide:
			case TType::Percent:
				return 90;

			case TType::Plus:
			case TType::Minus:
				return 80;

			case TType::ShiftLeft:
			case TType::ShiftRight:
				return 70;

			case TType::LAngle:
			case TType::RAngle:
			case TType::LessThanEquals:
			case TType::GreaterEquals:
				return 60;

			case TType::EqualsTo:
			case TType::NotEquals:
				return 50;

			case TType::Ampersand:
				return 30;

			case TType::Pipe:
				return 25;

			case TType::LogicalAnd:
				return 20;

			case TType::LogicalOr:
				return 15;

			case TType::Equal:
				return 10;

			default:
				return -1;
		}
	}

	VarType determineVarType(std::string type_id)
	{
		// kinda hardcoded
		if(type_id == "Int8")			return VarType::Int8;
		else if(type_id == "Int16")		return VarType::Int16;
		else if(type_id == "Int32")		return VarType::Int32;
		else if(type_id == "Int64")		return VarType::Int64;
		else if(type_id == "Uint8")		return VarType::Uint8;
		else if(type_id == "Uint16")	return VarType::Uint16;
		else if(type_id == "Uint32")	return VarType::Uint32;
		else if(type_id == "Uint64")	return VarType::Uint64;

		else if(type_id == "Int8Ptr")	return VarType::Int8Ptr;
		else if(type_id == "Int16Ptr")	return VarType::Int16Ptr;
		else if(type_id == "Int32Ptr")	return VarType::Int32Ptr;
		else if(type_id == "Int64Ptr")	return VarType::Int64Ptr;
		else if(type_id == "Uint8Ptr")	return VarType::Uint8Ptr;
		else if(type_id == "Uint16Ptr")	return VarType::Uint16Ptr;
		else if(type_id == "Uint32Ptr")	return VarType::Uint32Ptr;
		else if(type_id == "Uint64Ptr")	return VarType::Uint64Ptr;

		else if(type_id == "AnyPtr")	return VarType::AnyPtr;

		else if(type_id == "Float32")	return VarType::Float32;
		else if(type_id == "Float64")	return VarType::Float64;
		else if(type_id == "Bool")		return VarType::Bool;
		else if(type_id == "Void")		return VarType::Void;
		else
		{
			// todo: risky
			if(type_id.back() == ']')
				return VarType::Array;

			else
				return VarType::UserDefined;
		}
	}












	void parseAll(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
			return;

		while(Token* tok = tokens.front())
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				case TType::Func:
					rootNode->functions.push_back(parseFunc(tokens));
					break;

				case TType::Import:
					rootNode->imports.push_back(parseImport(tokens));
					break;

				case TType::ForeignFunc:
					rootNode->foreignfuncs.push_back(parseForeignFunc(tokens));
					break;

				case TType::Struct:
					rootNode->structs.push_back(parseStruct(tokens));
					break;

				// shit you just skip
				case TType::NewLine:
				case TType::Comment:
				case TType::Semicolon:
					tokens.pop_front();
					break;

				default:	// wip: skip shit we don't know/care about for now
					parseTopLevelExpr(tokens);
					break;
			}
		}
	}

	Func* parseTopLevelExpr(std::deque<Token*>& tokens)
	{
		Expr* expr = parseExpr(tokens);
		FuncDecl* fakedecl = new FuncDecl("__anonymous_toplevel_0", std::deque<VarDecl*>(), "");
		Closure* cl = new Closure();
		cl->statements.push_back(expr);

		Func* fakefunc = new Func(fakedecl, cl);
		rootNode->functions.push_back(fakefunc);

		return fakefunc;
	}

	Expr* parseUnary(std::deque<Token*>& tokens)
	{
		// check for unary shit
		if(tokens.front()->type == TType::Exclamation || tokens.front()->type == TType::Plus || tokens.front()->type == TType::Minus)
		{
			TType tp = eat(tokens)->type;
			ArithmeticOp op = tp == TType::Exclamation ? ArithmeticOp::LogicalNot : (tp == TType::Plus ? ArithmeticOp::Plus : ArithmeticOp::Minus);

			return new UnaryOp(op, parseUnary(tokens));
		}
		else if(tokens.front()->type == TType::Deref)
		{
			eat(tokens);
			return new UnaryOp(ArithmeticOp::Deref, parseUnary(tokens));
		}
		else if(tokens.front()->type == TType::Addr)
		{
			eat(tokens);
			return new UnaryOp(ArithmeticOp::AddrOf, parseUnary(tokens));
		}
		else
		{
			return parsePrimary(tokens);
		}
	}

	Expr* parsePrimary(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
			return nullptr;

		while(Token* tok = tokens.front())
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				case TType::Var:
				case TType::Val:
					return parseVarDecl(tokens);

				case TType::Func:
					return parseFunc(tokens);

				case TType::LParen:
					return parseParenthesised(tokens);

				case TType::Identifier:
					if(tok->text == "init")
						return parseFunc(tokens);

					else if(tok->text == "operator")
						return parseOpOverload(tokens);

					return parseIdExpr(tokens);

				case TType::StringLiteral:
					return parseStringLiteral(tokens);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(tokens);

				case TType::Return:
					return parseReturn(tokens);

				case TType::If:
					return parseIf(tokens);

				// shit you just skip
				case TType::NewLine:
				case TType::Comment:
				case TType::Semicolon:
					eat(tokens);
					return new DummyExpr();

				case TType::True:
					tokens.pop_front();
					return (new BoolVal(true))->setPos(pos);

				case TType::False:
					tokens.pop_front();
					return (new BoolVal(false))->setPos(pos);

				default:	// wip: skip shit we don't know/care about for now
					fprintf(stderr, "Unknown token '%s', skipping\n", tok->text.c_str());
					eat(tokens);
					break;
			}
		}

		return nullptr;
	}







	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens)
	{
		// todo: better things? it's right now mostly hacks.
		if(tokens.front()->text != "init" && tokens.front()->text.find("operator") != 0)
			assert(eat(tokens)->type == TType::Func);

		if(tokens.front()->type != TType::Identifier)
			error("Expected identifier, but got token of type %d", tokens.front()->type);

		std::string id = eat(tokens)->text;

		// expect a left bracket
		Token* paren = eat(tokens);
		if(paren->type != TType::LParen)
			error("Expected '(' in function declaration, got '%s'", paren->text.c_str());

		// get the parameter list
		// expect an identifer, colon, type
		std::deque<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;
		while(tokens.size() > 0 && tokens.front()->type != TType::RParen)
		{
			Token* tok_id;
			if((tok_id = eat(tokens))->type != TType::Identifier)
				error("Expected identifier");

			std::string id = tok_id->text;
			VarDecl* v = (new VarDecl(id, true))->setPos(pos);

			// expect a colon
			if(eat(tokens)->type != TType::Colon)
				error("Expected ':' followed by a type");

			v->type = parseType(tokens);
			v->varType = determineVarType(v->type);

			if(!nameCheck[v->name])
			{
				params.push_back(v);
				nameCheck[v->name] = v;
			}
			else
			{
				error("Redeclared variable '%s' in argument list", v->name.c_str());
			}

			if(tokens.front()->type == TType::Comma)
				eat(tokens);
		}

		// consume the closing paren
		eat(tokens);

		// get return type.
		std::string ret;
		Token* tok_type = nullptr;
		if(checkHasMore(tokens) && tokens.front()->type == TType::Arrow)
		{
			eat(tokens);
			ret = parseType(tokens);
		}
		else
		{
			ret = "Void";
		}

		skipNewline(tokens);
		FuncDecl* f = (new FuncDecl(id, params, ret))->setPos(pos);
		f->varType = tok_type == nullptr ? VarType::Void : determineVarType(tok_type->text);

		return f;
	}

	ForeignFuncDecl* parseForeignFunc(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::ForeignFunc);
		eat(tokens);

		FuncDecl* decl = parseFuncDecl(tokens);
		decl->isFFI = true;

		return (new ForeignFuncDecl(decl))->setPos(pos);
	}

	Closure* parseClosure(std::deque<Token*>& tokens)
	{
		Closure* c = (new Closure())->setPos(pos);

		// make sure the first token is a left brace.
		if(eat(tokens)->type != TType::LBrace)
			error("Expected '{' to begin a block");

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

	Func* parseFunc(std::deque<Token*>& tokens)
	{
		FuncDecl* decl = parseFuncDecl(tokens);
		return (new Func(decl, parseClosure(tokens)))->setPos(pos);
	}



















	std::string parseType(std::deque<Token*>& tokens)
	{
		bool isPtr = false;
		bool isArr = false;
		int arrsize = 0;
		Token* tmp = nullptr;
		if((tmp = eat(tokens))->type != TType::Identifier)
			error("Expected type for variable declaration");

		if(tokens.size() > 0)
		{
			if(tokens.front()->type == TType::Ptr)
			{
				isPtr = true;
				eat(tokens);
			}
			else if(tokens.front()->type == TType::LSquare)
			{
				isArr = true;
				eat(tokens);

				Token* next = eat(tokens);
				if(next->type == TType::Integer)
					arrsize = std::stoi(next->text), next = eat(tokens);

				if(next->type != TType::RSquare)
					error("Expected either constant integer or ']' after array declaration and '['");
			}
		}

		std::string ret = tmp->text + (isPtr ? "Ptr" : (isArr ? "[" + std::to_string(arrsize) + "]" : ""));
		return ret;
	}

	VarDecl* parseVarDecl(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Var || tokens.front()->type == TType::Val);

		bool immutable = tokens.front()->type == TType::Val;
		eat(tokens);

		// get the identifier.
		Token* tok_id;
		if((tok_id = eat(tokens))->type != TType::Identifier)
			error("Expected identifier for variable declaration.");

		std::string id = tok_id->text;
		VarDecl* v = (new VarDecl(id, immutable))->setPos(pos);

		// check the type.
		// todo: type inference
		if(eat(tokens)->type != TType::Colon)
			error("Expected colon to indicate type for variable declaration");

		v->type = parseType(tokens);
		v->varType = determineVarType(v->type);

		// TODO:
		// check if we have a default value
		v->initVal = nullptr;
		if(tokens.front()->type == TType::Equal)
		{
			// we do
			eat(tokens);

			v->initVal = parseExpr(tokens);
			if(!v->initVal)
				error("Invalid initialiser for variable '%s'", v->name.c_str());
		}

		return v;
	}

	Expr* parseParenthesised(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::LParen);
		eat(tokens);

		Expr* within = parseExpr(tokens);

		if(eat(tokens)->type != TType::RParen)
			error("Expected ')'");

		return within;
	}

	Expr* parseExpr(std::deque<Token*>& tokens)
	{
		Expr* lhs = parseUnary(tokens);
		if(!lhs)
			return nullptr;

		return parseRhs(tokens, lhs, 0);
	}

	Expr* parseRhs(std::deque<Token*>& tokens, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = getOpPrec(tokens.front());
			if(prec < prio)
				return lhs;

			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything
			Token* tok_op = eat(tokens);

			Expr* rhs = parseUnary(tokens);
			if(!rhs)
				return nullptr;

			int next = getOpPrec(tokens.front());
			if(prec < next)
			{
				rhs = parseRhs(tokens, rhs, prec + 1);
				if(!rhs)
					return nullptr;
			}

			ArithmeticOp op;
			switch(tok_op->type)
			{
				case TType::Plus:			op = ArithmeticOp::Add;			break;
				case TType::Minus:			op = ArithmeticOp::Subtract;	break;
				case TType::Asterisk:		op = ArithmeticOp::Multiply;	break;
				case TType::Divide:			op = ArithmeticOp::Divide;		break;
				case TType::Percent:		op = ArithmeticOp::Modulo;		break;
				case TType::ShiftLeft:		op = ArithmeticOp::ShiftLeft;	break;
				case TType::ShiftRight:		op = ArithmeticOp::ShiftRight;	break;
				case TType::Equal:			op = ArithmeticOp::Assign;		break;

				case TType::LAngle:			op = ArithmeticOp::CmpLT;		break;
				case TType::RAngle:			op = ArithmeticOp::CmpGT;		break;
				case TType::LessThanEquals:	op = ArithmeticOp::CmpLEq;		break;
				case TType::GreaterEquals:	op = ArithmeticOp::CmpGEq;		break;
				case TType::EqualsTo:		op = ArithmeticOp::CmpEq;		break;
				case TType::NotEquals:		op = ArithmeticOp::CmpNEq;		break;

				case TType::Ampersand:		op = ArithmeticOp::BitwiseAnd;	break;
				case TType::Pipe:			op = ArithmeticOp::BitwiseOr;	break;
				case TType::LogicalOr:		op = ArithmeticOp::LogicalOr;	break;
				case TType::LogicalAnd:		op = ArithmeticOp::LogicalAnd;	break;
				case TType::As:				op = ArithmeticOp::Cast;		break;
				default:					error("Unknown operator '%s'", tok_op->text.c_str());
			}

			lhs = (new BinOp(lhs, op, rhs))->setPos(pos);
		}
	}

	Expr* parseIdExpr(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Identifier);
		std::string id = eat(tokens)->text;
		VarRef* idvr = (new VarRef(id))->setPos(pos);

		// check for dot syntax.
		if(tokens.front()->type == TType::Period)
		{
			eat(tokens);
			if(tokens.front()->type != TType::Identifier)
				error("Expected identifier after '.' operator");

			return (new MemberAccess(idvr, parseIdExpr(tokens)))->setPos(pos);
		}
		else if(tokens.front()->type == TType::LSquare)
		{
			eat(tokens);
			Expr* within = parseExpr(tokens);

			if(eat(tokens)->type != TType::RSquare)
				error("Expected ']'");

			return (new ArrayIndex(idvr, within))->setPos(pos);
		}
		else if(tokens.front()->type == TType::Ptr)
		{
			eat(tokens);
			id += "Ptr";
			return idvr;
		}
		else if(tokens.front()->type != TType::LParen)
		{
			return idvr;
		}
		else
		{
			delete idvr;
			return parseFunctionCall(tokens, id);
		}
	}

	Number* parseNumber(std::deque<Token*>& tokens)
	{
		Number* n;
		if(tokens.front()->type == TType::Integer)
		{
			Token* tok = eat(tokens);
			n = (new Number((int64_t) std::stoll(tok->text)))->setPos(pos);

			// set the type.
			// always used signed
			n->varType = VarType::Int64;
		}
		else if(tokens.front()->type == TType::Decimal)
		{
			Token* tok = eat(tokens);
			n = (new Number(std::stod(tok->text)))->setPos(pos);

			if(n->dval < FLT_MAX)	n->varType = VarType::Float32;
			else					n->varType = VarType::Float64;
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

				Token* t;
				if((t = eat(tokens))->type != TType::Comma)
					error("Expected either ',' or ')' in parameter list, got '%s'", t->text.c_str());
			}
		}
		else
		{
			eat(tokens);
		}

		return (new FuncCall(id, args))->setPos(pos);
	}

	Return* parseReturn(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Return);
		eat(tokens);

		return (new Return(parseExpr(tokens)))->setPos(pos);
	}

	Expr* parseIf(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::If);
		eat(tokens);

		typedef std::pair<Expr*, Closure*> CCPair;
		std::deque<CCPair> conds;

		Expr* cond = parseExpr(tokens);
		Closure* tcase = parseClosure(tokens);

		conds.push_back(CCPair(cond, tcase));

		// check for else and else if
		Closure* ecase = nullptr;
		while(tokens.front()->type == TType::Else)
		{
			eat(tokens);
			if(tokens.front()->type == TType::If)
			{
				eat(tokens);

				// parse an expr, then a closure
				Expr* c = parseExpr(tokens);
				Closure* cl = parseClosure(tokens);

				conds.push_back(CCPair(c, cl));
			}
			else
			{
				ecase = parseClosure(tokens);
			}
		}

		return (new If(conds, ecase))->setPos(pos);
	}

	Struct* parseStruct(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::Struct);
		isParsingStruct = true;

		// get the identifier (name)
		std::string id;
		if(tokens.front()->type != TType::Identifier)
			error("Expected name after 'struct'");

		id += eat(tokens)->text;
		Struct* str = (new Struct(id))->setPos(pos);

		// parse a clousure.
		Closure* body = parseClosure(tokens);
		int i = 0;
		for(Expr* stmt : body->statements)
		{
			// check for top-level statements
			VarDecl* var = nullptr;
			Func* func = nullptr;
			OpOverload* oo = nullptr;

			if((var = dynamic_cast<VarDecl*>(stmt)))
			{
				if(str->nameMap.find(var->name) != str->nameMap.end())
					error("Duplicate member '%s'", var->name.c_str());

				str->members.push_back(var);
				str->nameMap[var->name] = i;
			}
			else if((func = dynamic_cast<Func*>(stmt)))
			{
				if(str->nameMap.find(func->decl->name) != str->nameMap.end())
					error("Duplicate member '%s'", func->decl->name.c_str());

				str->funcs.push_back(func);
				str->nameMap[func->decl->name] = i;
			}
			else if((oo = dynamic_cast<OpOverload*>(stmt)))
			{
				oo->str = str;
				str->opmap[oo->op] = oo;

				if(str->nameMap.find(oo->func->decl->name) != str->nameMap.end())
					error("Duplicate member '%s'", oo->func->decl->name.c_str());

				str->funcs.push_back(oo->func);
				str->nameMap[oo->func->decl->name] = i;
			}
			else
			{
				error("Only variable and function declarations are allowed in structs");
			}

			i++;
		}

		return str;
	}

	Import* parseImport(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::Import);

		Token* tok_mod;
		if((tok_mod = eat(tokens))->type != TType::Identifier)
			error("Expected module name after 'import' statement.");

		return (new Import(tok_mod->text))->setPos(pos);
	}

	StringLiteral* parseStringLiteral(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::StringLiteral);
		Token* str = eat(tokens);

		return (new StringLiteral(str->text))->setPos(pos);
	}

	OpOverload* parseOpOverload(std::deque<Token*>& tokens)
	{
		if(!isParsingStruct)
			error("Can only overload operators in the context of a named aggregate type");

		assert(eat(tokens)->text == "operator");
		Token* op = eat(tokens);

		ArithmeticOp ao;
		switch(op->type)
		{
			case TType::Equal:
				ao = ArithmeticOp::Assign;
				break;

			default:
				error("Unsupported operator overload on operator '%s'", op->text.c_str());
		}

		OpOverload* oo = (new OpOverload(ao))->setPos(pos);

		Token* fake = new Token();
		fake->posinfo = pos;
		fake->text = "operator#" + op->text;
		fake->type = TType::Identifier;

		tokens.push_front(fake);

		// parse a func declaration.
		oo->func = parseFunc(tokens);
		return oo;
	}
}

























