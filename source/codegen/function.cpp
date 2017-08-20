// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#include "ir/irbuilder.h"

CGResult sst::FunctionDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	std::vector<fir::Type*> ptypes;
	for(auto p : this->params)
		ptypes.push_back(p.type);

	auto ft = fir::FunctionType::get(ptypes, this->returnType);

	auto ident = this->id;
	if(this->id.name == "main" && this->privacy == PrivacyLevel::Public && this->id.scope.size() == 1
		&& this->id.scope[0] == cs->module->getModuleName())
	{
		ident = Identifier(this->id.name, IdKind::Name);
	}

	auto fn = cs->module->getOrCreateFunction(ident, ft,
		this->privacy == PrivacyLevel::Private ? fir::LinkageType::Internal : fir::LinkageType::External);

	auto restore = cs->irb.getCurrentBlock();
	defer(cs->irb.setCurrentBlock(restore));

	cs->enterNamespace(this->id.mangled());
	defer(cs->leaveNamespace());

	auto block = cs->irb.addNewBlockInFunction(this->id.name + "_entry", fn);
	cs->irb.setCurrentBlock(block);

	this->body->codegen(cs);

	// todo:
	cs->irb.CreateReturnVoid();

	return CGResult(0);
}


CGResult sst::ForeignFuncDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::FunctionType* ft = 0;
	std::vector<fir::Type*> ptypes;
	for(auto p : this->params)
		ptypes.push_back(p.type);

	if(this->isVarArg)
		ft = fir::FunctionType::getCVariadicFunc(ptypes, this->returnType);

	else
		ft = fir::FunctionType::get(ptypes, this->returnType);

	if(cs->module->getFunction(this->id) != 0)
		error(this, "Foreign function '%s' already defined elsewhere; overloading not possible", this->id.str().c_str());

	auto fn = cs->module->getOrCreateFunction(this->id, ft, fir::LinkageType::External);

	return CGResult(fn);
}






CGResult sst::Block::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	for(auto stmt : this->statements)
		stmt->codegen(cs);

	// then do the defers
	for(auto stmt : this->deferred)
		stmt->codegen(cs);

	return CGResult(0);
}
















