// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include "memorypool.h"

TCResult ast::IfStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Case = sst::IfStmt::Case;
	auto ret = util::pool<sst::IfStmt>(this->loc);

	fs->pushAnonymousTree();
	defer(fs->popTree());

	for(auto c : this->cases)
	{
		//* here, it is implicit that all the inits of every case live in the same scope.
		//? we might want to change this eventually? i'm not sure.

		auto inits = zfu::map(c.inits, [fs](Stmt* s) -> auto { return s->typecheck(fs).stmt(); });
		auto cs = Case(c.cond->typecheck(fs).expr(), dcast(sst::Block, c.body->typecheck(fs).stmt()), inits);

		if(!cs.cond->type->isBoolType() && !cs.cond->type->isPointerType())
			error(cs.cond, "non-boolean expression with type '%s' cannot be used as a conditional", cs.cond->type);

		ret->cases.push_back(cs);

		iceAssert(ret->cases.back().body);
	}

	if(this->elseCase)
	{
		ret->elseCase = dcast(sst::Block, this->elseCase->typecheck(fs).stmt());
		iceAssert(ret->elseCase);
	}

	return TCResult(ret);
}

TCResult ast::ReturnStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ret = util::pool<sst::ReturnStmt>(this->loc);

	if(fs->isInDeferBlock())
		error(this, "cannot 'return' while inside a deferred block");


	// ok, get the current function
	auto fn = fs->getCurrentFunction();
	auto retty = fn->returnType;

	if(this->value)
	{
		ret->value = this->value->typecheck(fs, retty).expr();

		if(fir::getCastDistance(ret->value->type, retty) < 0)
		{
			SpanError::make(SimpleError::make(this->loc, "mismatched type in return statement; function returns '%s', value has type '%s'",
				retty, ret->value->type))->add(util::ESpan(this->value->loc, strprintf("type '%s'", ret->value->type)))
				->append(SimpleError::make(MsgType::Note, fn->loc, "function definition is here:"))
				->postAndQuit();
		}

		// ok
	}
	else if(!retty->isVoidType())
	{
		error(this, "expected value after 'return'; function return type is '%s'", retty);
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

			if(fir::getCastDistance(t, retty) < 0)
			{
				if(retstmt->expectedType->isVoidType())
				{
					error(retstmt, "expected value after 'return'; function return type is '%s'", retty);
				}
				else
				{
					std::string msg;
					if(block->isSingleExpr) msg = "invalid single-expression with type '%s' in function returning '%s'";
					else                    msg = "mismatched type in return statement; function returns '%s', value has type '%s'";

					SpanError::make(SimpleError::make(retstmt->loc, msg.c_str(), retty, retstmt->expectedType))
						->add(util::ESpan(retstmt->value->loc, strprintf("type '%s'", retstmt->expectedType)))
						->append(SimpleError::make(MsgType::Note, fs->getCurrentFunction()->loc, "function definition is here:"))
						->postAndQuit();
				}
			}

			if(i != block->statements.size() - 1)
			{
				SimpleError::make(block->statements[i + 1]->loc, "unreachable code after return statement")
					->append(SimpleError::make(MsgType::Note, retstmt->loc, "return statement was here:"))
					->postAndQuit();;
			}
		}
		else /* if(i == block->statements.size() - 1) */
		{
			//* it's our duty to check the internals of these things regardless of their exhaustiveness
			//* so that we can check for the elision of the merge block.
			//? eg: if 's' itself does not have an else case, but say one of its branches is exhaustive (all arms return),
			//? then we can elide the merge block for that branch, even though we can't for 's' itself.
			//* this isn't strictly necessary (the program is still correct without it), but we generate nicer IR this way.

			bool exhausted = false;
			if(auto ifstmt = dcast(sst::IfStmt, s); ifstmt)
			{
				bool all = true;
				for(const auto& c: ifstmt->cases)
					all = all && checkBlockPathsReturn(fs, c.body, retty, faulty);

				exhausted = all && ifstmt->elseCase && checkBlockPathsReturn(fs, ifstmt->elseCase, retty, faulty);
				ifstmt->elideMergeBlock = exhausted;
			}
			else if(auto whileloop = dcast(sst::WhileLoop, s); whileloop)
			{
				exhausted = checkBlockPathsReturn(fs, whileloop->body, retty, faulty) && whileloop->isDoVariant;
				whileloop->elideMergeBlock = exhausted;
			}
			else if(auto feloop = dcast(sst::ForeachLoop, s); feloop)
			{
				// we don't set 'exhausted' here beacuse for loops cannot be guaranteed to be exhaustive.
				// but we still want to check the block inside for elision.
				feloop->elideMergeBlock = checkBlockPathsReturn(fs, feloop->body, retty, faulty);
			}

			ret = exhausted;
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
		auto err = SimpleError::make(fn->loc, "not all paths return a value; expected value of type '%s'", expected);

		for(auto b : faults)
			err->append(SimpleError::make(MsgType::Note, b->closingBrace, "potentially missing return statement here:"));

		err->postAndQuit();
	}

	return ret;
}


























