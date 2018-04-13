// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

sst::Stmt* ast::IfStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Case = sst::IfStmt::Case;
	auto ret = new sst::IfStmt(this->loc);

	auto n = fs->getAnonymousScopeName();

	fs->pushTree(n);
	defer(fs->popTree());

	for(auto c : this->cases)
	{
		auto inits = util::map(c.inits, [fs](Stmt* s) -> auto { return s->typecheck(fs); });
		auto cs = Case {
							c.cond->typecheck(fs),
							dynamic_cast<sst::Block*>(c.body->typecheck(fs)),
							inits
						};

		if(!cs.cond->type->isBoolType() && !cs.cond->type->isPointerType())
			error(cs.cond, "Non-boolean expression with type '%s' cannot be used as a conditional", cs.cond->type);

		ret->cases.push_back(cs);

		iceAssert(ret->cases.back().body);
	}

	if(this->elseCase)
	{
		ret->elseCase = dynamic_cast<sst::Block*>(this->elseCase->typecheck(fs));
		iceAssert(ret->elseCase);
	}

	return ret;
}

sst::Stmt* ast::ReturnStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ret = new sst::ReturnStmt(this->loc);

	if(fs->isInDeferBlock())
		error(this, "Cannot 'return' while inside a deferred block");


	// ok, get the current function
	auto fn = fs->getCurrentFunction();
	auto retty = fn->returnType;

	if(this->value)
	{
		ret->value = this->value->typecheck(fs, retty);

		if(ret->value->type != retty && fs->getCastDistance(ret->value->type, retty) < 1)
		{
			HighlightOptions hs;
			hs.underlines.push_back(this->value->loc);

			error(this, hs, "Cannot return a value of type '%s' in a function returning type '%s'",
				ret->value->type, retty);
		}

		// ok
	}
	else if(!retty->isVoidType())
	{
		error(this, "Expected value after 'return'; function return type is '%s'", retty);
	}

	ret->expectedType = retty;
	return ret;
}


static bool checkBlockPathsReturn(sst::TypecheckState* fs, sst::Block* block, fir::Type* retty, std::vector<sst::Block*>* faulty)
{
	// return value is whether or not the block had a return value;
	// true if all paths explicitly returned, false if not
	// this return value is used to determine whether we need to insert a
	// 'return void' thing.

	bool ret = false;
	for(size_t i = 0; i < block->statements.size(); i++)
	{
		auto& s = block->statements[i];
		if(auto hb = dcast(sst::HasBlocks, s))
		{
			const auto& blks = hb->getBlocks();
			for(auto b : blks)
			{
				auto r = checkBlockPathsReturn(fs, b, retty, faulty);
				if(!r) faulty->push_back(b);

				ret &= r;
			}
		}


		// check for returns
		else if(auto retstmt = dcast(sst::ReturnStmt, s))
		{
			// ok...
			ret = true;
			auto t = retstmt->expectedType;
			iceAssert(t);

			if(t != retty)
			{
				if(retstmt->expectedType->isVoidType())
				{
					error(retstmt, "Expected value after 'return'; function return type is '%s'", retty);
				}
				else
				{
					HighlightOptions hs;
					hs.underlines.push_back(retstmt->value->loc);

					error(retstmt, hs, "Cannot return a value of type '%s' in a function returning type '%s'",
						retstmt->expectedType, retty);
				}
			}

			// ok, pass

			if(i != block->statements.size() - 1)
			{
				exitless_error(block->statements[i + 1], "Unreachable code after return statement");
				info(retstmt, "Return statement was here:");

				doTheExit();
			}
		}
	}

	return ret;
}

bool sst::TypecheckState::checkAllPathsReturn(FunctionDefn* fn)
{
	fir::Type* expected = fn->returnType;

	if(fn->body->isSingleExpr)
	{
		// special things.
		iceAssert(fn->body->statements.size() == 1);

		auto stmt = fn->body->statements.front();
		if(auto expr = dcast(sst::Expr, stmt); expr)
		{
			if(this->getCastDistance(expr->type, expected) < 0)
				error(expr, "Found expression of type '%s' in single-expression function, when function returns type '%s'", expr->type, expected);

			// ok.
			return false;
		}
		else
		{
			error(stmt, "Expected expression (of type '%s') for single-expression function, found statement instead.", expected);
		}
	}

	std::vector<sst::Block*> faults { fn->body };
	auto ret = checkBlockPathsReturn(this, fn->body, expected, &faults);

	if(!expected->isVoidType() && !ret)
	{
		exitless_error(fn, "Not all paths return a value; expected value of type '%s'",
			expected);

		for(auto b : faults)
			info(b->closingBrace, "Potentially missing return statement here:");

		doTheExit();
	}

	return ret;
}


























