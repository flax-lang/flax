// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

namespace cgn
{
	void CodegenState::enterNamespace(std::string name)
	{
		if(auto it = this->stree->subtrees.find(name); it != this->stree->subtrees.end())
			this->stree = it->second;

		else
			error(this->loc(), "Tried to enter non-existent namespace '%s' in current scope '%s'", name.c_str(), this->stree->name.c_str());

		// because we haven't created the vtree, it's fine to "make" one when we enter
		auto it = this->vtree->subs.find(name);

		if(it == this->vtree->subs.end())
			this->vtree->subs[name] = new ValueTree(name, this->vtree);

		this->vtree = this->vtree->subs[name];
	}

	void CodegenState::leaveNamespace()
	{
		if(!this->stree->parent)
			error(this->loc(), "Cannot leave the top-level namespace");

		this->stree = this->stree->parent;
		this->vtree = this->vtree->parent;
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


	fir::Value* CodegenState::getDefaultValue(fir::Type* type)
	{
		return fir::ConstantValue::getZeroValue(type);
	}

}






CGResult sst::TypeExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	return CGResult(fir::ConstantValue::getZeroValue(this->type));
}





