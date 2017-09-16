// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

namespace cgn
{
	std::pair<sst::StateTree*, ValueTree*> CodegenState::setNamespace(std::vector<std::string> scope)
	{
		auto ret = std::make_pair(this->stree, this->vtree);

		// get the root tree.
		while(this->stree->parent)
			this->stree = this->stree->parent,
			this->vtree = this->vtree->parent;

		// skip the first one, because it'll be the actual root.
		for(size_t i = 1; i < scope.size(); i++)
			this->enterNamespace(scope[i]);

		return ret;
	}

	void CodegenState::restoreNamespace(std::pair<sst::StateTree*, ValueTree*> rsn)
	{
		iceAssert(rsn.first && rsn.second);

		this->stree = rsn.first;
		this->vtree = rsn.second;
	}


	void CodegenState::enterNamespace(std::string name)
	{
		if(auto it = this->stree->subtrees.find(name); it != this->stree->subtrees.end())
			this->stree = it->second;

		else
			error(this->loc(), "Tried to enter non-existent namespace '%s' in current scope '%s'", name, this->stree->name);

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

	void CodegenState::enterFunction(fir::Function* fn)
	{
		this->functionStack.push_back(fn);
	}

	void CodegenState::leaveFunction()
	{
		if(this->functionStack.empty())
			error(this->loc(), "Not a in function");

		this->functionStack.pop_back();
	}

	fir::Function* CodegenState::getCurrentFunction()
	{
		if(this->functionStack.empty())
			error(this->loc(), "Not a in function");

		return this->functionStack.back();
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



	fir::Function* CodegenState::getOrDeclareLibCFunction(std::string name)
	{
		if(name == ALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(ALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64() }, fir::Type::getInt8Ptr()), fir::LinkageType::External);
		}
		else if(name == FREE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(FREE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == REALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(REALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr(), fir::Type::getInt64() }, fir::Type::getInt8Ptr()), fir::LinkageType::External);
		}
		else if(name == "printf")
		{
			return this->module->getOrCreateFunction(Identifier("printf", IdKind::Name),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getInt8Ptr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else if(name == "abort")
		{
			return this->module->getOrCreateFunction(Identifier("abort", IdKind::Name),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == "strlen")
		{
			return this->module->getOrCreateFunction(Identifier("strlen", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getInt64()), fir::LinkageType::External);
		}
		else
		{
			error("enotsup: %s", name);
		}
	}


	bool CodegenState::isRefCountedType(fir::Type* type)
	{
		return false;
	}

	void CodegenState::incrementRefCount(fir::Value* ptr)
	{
	}

	void CodegenState::decrementRefCount(fir::Value* ptr)
	{
	}

}






CGResult sst::TypeExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	return CGResult(fir::ConstantValue::getZeroValue(this->type));
}





