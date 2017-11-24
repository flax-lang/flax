// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::IfStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Case = sst::IfStmt::Case;
	auto ret = new sst::IfStmt(this->loc);

	auto n = fs->getAnonymousScopeName();

	ret->generatedScopeName = n;
	ret->scope = fs->getCurrentScope();

	fs->pushTree(n);
	defer(fs->popTree());

	for(auto c : this->cases)
	{
		auto inits = util::map(c.inits, [fs](Stmt* s) -> auto { return s->typecheck(fs); });
		ret->cases.push_back(Case {
									c.cond->typecheck(fs),
									dynamic_cast<sst::Block*>(c.body->typecheck(fs)),
									inits
								});

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

		if(ret->value->type != retty)
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

		// check for things with bodies
		if(auto ifstmt = dcast(sst::IfStmt, s))
		{
			for(auto c : ifstmt->cases)
			{
				auto r = checkBlockPathsReturn(fs, c.body, retty, faulty);
				if(!r) faulty->push_back(c.body);

				ret &= r;
			}

			if(ifstmt->elseCase)
			{
				auto r = checkBlockPathsReturn(fs, ifstmt->elseCase, retty, faulty);
				if(!r) faulty->push_back(ifstmt->elseCase);

				ret &= r;
			}
		}
		else if(auto whileloop = dcast(sst::WhileLoop, s))
		{
			auto r = checkBlockPathsReturn(fs, whileloop->body, retty, faulty);
			if(!r) faulty->push_back(whileloop->body);

			ret &= r;
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



sst::Stmt* ast::WhileLoop::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	sst::WhileLoop* ret = new sst::WhileLoop(this->loc);
	ret->isDoVariant = this->isDoVariant;

	auto n = fs->getAnonymousScopeName();

	ret->generatedScopeName = n;
	ret->scope = fs->getCurrentScope();

	fs->pushTree(n);
	defer(fs->popTree());

	fs->enterBreakableBody();
	{
		ret->body = dcast(sst::Block, this->body->typecheck(fs));
		iceAssert(ret->body);
	}
	fs->leaveBreakableBody();

	if(this->cond)
	{
		ret->cond = this->cond->typecheck(fs, fir::Type::getBool());
		if(ret->cond->type != fir::Type::getBool())
			error(this->cond, "Non-boolean expression with type '%s' cannot be used as a conditional", ret->cond->type);
	}

	return ret;
}


sst::Stmt* ast::BreakStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "Cannot 'break' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "Cannot 'break' while inside a deferred block");

	return new sst::BreakStmt(this->loc);
}

sst::Stmt* ast::ContinueStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "Cannot 'continue' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "Cannot 'continue' while inside a deferred block");

	return new sst::ContinueStmt(this->loc);
}


















