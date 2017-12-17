// defer.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::DeferredStmt::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());

	// disallow certain things from being deferred
	auto ret = this->actual->typecheck(fs, infer);

	std::function<void (const Location&, std::vector<sst::Stmt*>)> checkRecursively
		= [&fs, &checkRecursively](const Location& loc, std::vector<sst::Stmt*> stmts) -> void
	{
		for(auto stmt : stmts)
		{
			if(dcast(sst::Defn, stmt))
			{
				exitless_error(stmt, "Definitions cannot be deferred");
				info(loc, "In deferred block here:");
				doTheExit();
			}
			else if(dcast(sst::BreakStmt, stmt) || dcast(sst::ContinueStmt, stmt) || dcast(sst::ReturnStmt, stmt))
			{
				exitless_error(stmt, "Control flow cannot be deferred");
				info(loc, "In deferred block here:");
				doTheExit();
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

	return ret;
}




































