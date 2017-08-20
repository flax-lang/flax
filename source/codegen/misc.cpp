// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

namespace cgn
{
	void CodegenState::enterNamespace(std::string name)
	{
		if(auto it = this->stree->subtrees.find(name); it != this->stree->subtrees.end())
			this->stree = it->second;

		else
			error(this->loc(), "Tried to enter non-existent namespace '%s' in current scope '%s'", name.c_str(), this->stree->name.c_str());
	}

	void CodegenState::leaveNamespace()
	{
		if(!this->stree->parent)
			error(this->loc(), "Cannot leave the top-level namespace");

		this->stree = this->stree->parent;
	}







	void CodegenState::pushLoc(const Location& l)
	{
		this->locationStack.push_back(l);
	}

	void CodegenState::pushLoc(sst::Stmt* stmt)
	{
		this->locationStack.push_back(stmt->loc);
	}

	void CodegenState::popLoc()
	{
		iceAssert(this->locationStack.size() > 0);
		this->locationStack.pop_back();
	}

	Location CodegenState::loc()
	{
		iceAssert(this->locationStack.size() > 0);
		return this->locationStack.back();
	}
}











//
