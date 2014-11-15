// AstImpl.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"

namespace Ast
{
	// printing shit
	void Number::print()
	{
		if(this->decimal)
			printf("%f", this->dval);

		else
			printf("%lld", this->ival);
	}

	void String::print()
	{
		printf("string: %s", this->val.c_str());
	}

	void Var::print()
	{
		printf("var(%s, %s)", this->name.c_str(), this->type.c_str());
	}

	void BinOp::print()
	{
		printf("binop(");
		left->print();
		printf(" %d ", op);
		right->print();
		printf(")");
	}

	void Func::print()
	{
		this->decl->print();
		printf("{\n");
		for(Expr* e : this->statements)
		{
			printf("\t");
			e->print();
			printf("\n");
		}
		printf("}");
	}

	void FuncDecl::print()
	{
		printf("funcdecl: %s", this->name.c_str());

		printf("(");
		if(this->params.size() > 0)
			this->params[0]->print();

		for(int i = 1; i < this->params.size(); i++)
		{
			printf(", ");
			this->params[i]->print();
		}

		printf(") -> %s\n", this->type.c_str());
	}

	void FuncCall::print()
	{
		printf("call %s(", this->name.c_str());
		for(Expr* arg : this->params)
		{
			arg->print();
			printf(", fin");
		}

		printf(")");
	}

	void Return::print()
	{
		printf("return ");
		this->val->print();
	}

	void Import::print()
	{
		printf("import '%s'", this->module.c_str());
	}

	void Root::print()
	{
		printf("ast:\n");
		for(Func* f : this->functions)
		{
			f->print();
			printf("\n");
		}
	}
}










