// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include "mpool.h"

TCResult ast::IfStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Case = sst::IfStmt::Case;
	auto ret = util::pool<sst::IfStmt>(this->loc);

	auto n = fs->getAnonymousScopeName();

	fs->pushTree(n);
	defer(fs->popTree());

	for(auto c : this->cases)
	{
		auto inits = util::map(c.inits, [fs](Stmt* s) -> auto { return s->typecheck(fs).stmt(); });
		auto cs = Case(c.cond->typecheck(fs).expr(), dynamic_cast<sst::Block*>(c.body->typecheck(fs).stmt()), inits);

		if(!cs.cond->type->isBoolType() && !cs.cond->type->isPointerType())
			error(cs.cond, "Non-boolean expression with type '%s' cannot be used as a conditional", cs.cond->type);

		ret->cases.push_back(cs);

		iceAssert(ret->cases.back().body);
	}

	if(this->elseCase)
	{
		ret->elseCase = dynamic_cast<sst::Block*>(this->elseCase->typecheck(fs).stmt());
		iceAssert(ret->elseCase);
	}

	return TCResult(ret);
}

TCResult ast::ReturnStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ret = util::pool<sst::ReturnStmt>(this->loc);

	if(fs->isInDeferBlock())
		error(this, "Cannot 'return' while inside a deferred block");


	// ok, get the current function
	auto fn = fs->getCurrentFunction();
	auto retty = fn->returnType;

	if(this->value)
	{
		ret->value = this->value->typecheck(fs, retty).expr();

		if(ret->value->type != retty)
		{
			SpanError::make(SimpleError::make(this->loc, "Mismatched type in return statement; function returns '%s', value has type '%s'",
				retty, ret->value->type))->add(util::ESpan(this->value->loc, strprintf("type '%s'", ret->value->type)))
				->append(SimpleError::make(MsgType::Note, fn->loc, "Function definition is here:"))
				->postAndQuit();
		}

		// ok
	}
	else if(!retty->isVoidType())
	{
		error(this, "Expected value after 'return'; function return type is '%s'", retty);
	}

	ret->expectedType = retty;
	return TCResult(ret);
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
		if(auto retstmt = dcast(sst::ReturnStmt, s))
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
					std::string msg;
					if(block->isSingleExpr) msg = "Invalid single-expression with type '%s' in function returning '%s'";
					else                    msg = "Mismatched type in return statement; function returns '%s', value has type '%s'";

					SpanError::make(SimpleError::make(retstmt->loc, msg.c_str(), retty, retstmt->expectedType))
						->add(util::ESpan(retstmt->value->loc, strprintf("type '%s'", retstmt->expectedType)))
						->append(SimpleError::make(MsgType::Note, fs->getCurrentFunction()->loc, "Function definition is here:"))
						->postAndQuit();
				}
			}

			if(i != block->statements.size() - 1)
			{
				SimpleError::make(block->statements[i + 1]->loc, "Unreachable code after return statement")
					->append(SimpleError::make(MsgType::Note, retstmt->loc, "Return statement was here:"))
					->postAndQuit();;

				doTheExit();
			}
		}
		else if(i == block->statements.size() - 1)
		{
			bool exhausted = false;

			// we can only be exhaustive if we have an else case.
			if(auto ifstmt = dcast(sst::IfStmt, s); ifstmt && ifstmt->elseCase)
			{
				bool all = true;
				for(const auto& c: ifstmt->cases)
					all &= checkBlockPathsReturn(fs, c.body, retty, faulty);

				exhausted = all & checkBlockPathsReturn(fs, ifstmt->elseCase, retty, faulty);
			}
			else if(auto whileloop = dcast(sst::WhileLoop, s); whileloop && whileloop->isDoVariant)
			{
				exhausted = checkBlockPathsReturn(fs, whileloop->body, retty, faulty);
			}

			ret = exhausted;
			if(exhausted) block->elideMergeBlock = true;
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
		auto err = SimpleError::make(fn->loc, "Not all paths return a value; expected value of type '%s'", expected);

		for(auto b : faults)
			err->append(SimpleError::make(MsgType::Note, b->closingBrace, "Potentially missing return statement here:"));

		err->postAndQuit();
	}

	return ret;
}


























