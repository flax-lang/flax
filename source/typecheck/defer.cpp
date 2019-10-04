// defer.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "memorypool.h"


TCResult ast::DeferredStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// disallow certain things from being deferred
	auto ret = this->actual->typecheck(fs, infer).stmt();

	std::function<void (const Location&, std::vector<sst::Stmt*>)> checkRecursively
		= [&checkRecursively](const Location& loc, std::vector<sst::Stmt*> stmts) -> void
	{
		for(auto stmt : stmts)
		{
			if(dcast(sst::Defn, stmt))
			{
				SimpleError::make(stmt->loc, "definitions cannot be deferred")
					->append(SimpleError::make(MsgType::Note, loc, "in deferred block here:"))
					->postAndQuit();
			}
			else if(dcast(sst::BreakStmt, stmt) || dcast(sst::ContinueStmt, stmt) || dcast(sst::ReturnStmt, stmt))
			{
				SimpleError::make(stmt->loc, "control flow cannot be deferred")
					->append(SimpleError::make(MsgType::Note, loc, "in deferred block here:"))
					->postAndQuit();
			}
			else if(auto hb = dcast(sst::HasBlocks, stmt))
			{
				for(auto b : hb->getBlocks())
					checkRecursively(loc, b->statements);
			}
		}
	};

	if(auto b = dcast(sst::Block, ret))
		checkRecursively(this->loc, b->statements);

	else
		checkRecursively(this->loc, { ret });

	return TCResult(ret);
}




































