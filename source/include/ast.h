// ast.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <string>
#include <deque>
#include "parser.h"

namespace Ast
{
	// rant:
	// fuck this. c++ structs are exactly the same as classes, except with public visibility by default
	// i'm lazy so this is the way it'll be.

	struct Expr
	{
		virtual ~Expr() { }
		virtual void print() = 0;
		std::string type;
	};

	struct Number : Expr
	{
		~Number() { }
		Number(double val) : dval(val) { this->decimal = true; }
		Number(int64_t val) : ival(val) { this->decimal = false; }

		virtual void print() override
		{
			if(this->decimal)
				printf("%f", this->dval);

			else
				printf("%lld", this->ival);
		}

		bool decimal = false;
		union
		{
			int64_t ival;
			double dval;
		};
	};

	struct String : Expr
	{
		~String() { }
		String(std::string& s) : val(s) { }

		virtual void print() override
		{
			printf("string: %s", this->val.c_str());
		}

		std::string val;
	};

	struct Id : Expr
	{
		~Id() { }
		Id(std::string name) : name(name) { }

		virtual void print() override
		{
			printf("%s", this->name.c_str());
		}

		std::string name;
	};

	struct Var : Expr
	{
		~Var() { }
		Var(std::string& name, bool immut) : name(name), immutable(immut) { }

		virtual void print() override
		{
			printf("var(%s, %s)", this->name.c_str(), this->type.c_str());
		}

		std::string name;
		bool immutable;

		Number* nval;
		String* sval;
	};

	struct BinOp : Expr
	{
		~BinOp() { }
		BinOp(Expr* lhs, char operation, Expr* rhs) : left(lhs), op(operation), right(rhs) { }

		virtual void print() override
		{
			printf("binop(");
			left->print();
			printf(" %c ", op);
			right->print();
			printf(")");
		}

		Expr* left;
		Expr* right;

		char op;
	};

	struct Closure : Expr
	{
		~Closure() { }

		virtual void print() override
		{
			printf("{\n");
			for(Expr* e : this->statements)
			{
				printf("\t");
				e->print();
				printf("\n");
			}
			printf("}");
		}

		std::deque<Expr*> statements;
	};

	struct FuncDecl : Expr
	{
		~FuncDecl() { }
		FuncDecl(Id* id, std::deque<Var*> params, Closure* body, std::string& ret) : name(id), params(params), body(body)
		{
			this->type = ret;
		}

		virtual void print() override
		{
			printf("func: ");
			this->name->print();

			printf("(");
			for(Var* v : this->params)
			{
				v->print();
				printf(", ");
			}

			printf(") -> %s\n", this->type.c_str());
			this->body->print();
		}

		Id* name;
		std::deque<Var*> params;
		Closure* body;
	};


	struct FuncCall : Expr
	{
		~FuncCall() { }
		FuncCall(Id* target, std::deque<Expr*> args) : name(target), params(args) { }

		virtual void print() override
		{
			printf("call %s(", this->name->name.c_str());
			for(Expr* arg : this->params)
			{
				arg->print();
				printf(", fin");
			}

			printf(")");
		}

		Id* name;
		std::deque<Expr*> params;
	};

	struct Import : Expr
	{
		~Import() { }
		Import(std::string name) : module(name) { }

		virtual void print() override
		{
			printf("import '%s'", this->module.c_str());
		}

		std::string module;
	};

	struct Root : Expr
	{
		~Root() { }

		virtual void print() override
		{
			printf("ast:\n");
			for(FuncDecl* f : this->functions)
			{
				f->print();
				printf("\n");
			}
		}

		// todo: add stuff like imports, etc.
		std::deque<FuncDecl*> functions;
		std::deque<Import*> imports;
	};
}










