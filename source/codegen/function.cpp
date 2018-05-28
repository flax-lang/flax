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
	if(this->isEntry || this->noMangle)
		ident = Identifier(this->id.name, IdKind::Name);

	auto fn = cs->module->getOrCreateFunction(ident, ft,
		this->visibility == VisibilityLevel::Private ? fir::LinkageType::Internal : fir::LinkageType::External);

	// manually set the names, I guess
	for(size_t i = 0; i < this->params.size(); i++)
		fn->getArguments()[i]->setName(this->params[i].name);



	// special case for functions, to enable recursive calling:
	// set our cached result *before* we generate our body, in case we contain either
	// a: a call to ourselves, or
	// b: a call to someone that call us eventually

	this->cachedResult = CGResult(fn);

	auto restore = cs->irb.getCurrentBlock();
	defer(cs->irb.setCurrentBlock(restore));

	cs->enterFunction(fn);
	defer(cs->leaveFunction());

	if(this->parentTypeForMethod)
		cs->enterMethodBody(fn, fn->getArguments()[0]);

	for(auto a : this->arguments)
		a->codegen(cs);

	auto block = cs->irb.addNewBlockInFunction(this->id.name + "_entry", fn);
	cs->irb.setCurrentBlock(block);

	// special thing here:
	// push a breakable block (ControlFlowPoint) so that a manual 'return' can know how to
	// do refcounting and deferred things.


	cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, 0, 0));
	{
		this->body->codegen(cs);
	}
	cs->leaveBreakableBody();

	// note that we *trust* in the typechecker
	// that all paths return the correct type.


	if(this->needReturnVoid)
		cs->irb.ReturnVoid();

	if(this->parentTypeForMethod)
		cs->leaveMethodBody();

	if(this->isEntry)
	{
		if(cs->entryFunction.first != 0)
		{
			SimpleError::make(this, "Redefinition of entry function with '@entry'")
				.append(SimpleError::make(MsgType::Note, cs->entryFunction.second, "Previous entry function marked here:"))
				.postAndQuit();
		}

		cs->entryFunction = { fn, this->loc };
	}

	cs->valueMap[this] = CGResult(fn);
	return CGResult(fn);
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

	auto ef = cs->module->getFunction(this->id);
	if(ef && ef->getType() != ft)
	{
		error(this, "Foreign function '%s' already defined elsewhere (with signature %s); overloading not possible",
			this->id.str(), ef->getType());
	}

	auto fn = cs->module->getOrCreateFunction(this->id, ft, fir::LinkageType::External);

	cs->valueMap[this] = CGResult(fn);
	// cs->vtree->values[this->id.name].push_back(CGResult(fn));

	return CGResult(fn);
}




CGResult sst::ArgumentDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto fn = cs->getCurrentFunction();

	// get the arguments
	auto arg = fn->getArgumentWithName(this->id.name);

	// ok...
	cs->valueMap[this] = CGResult(arg);
	return CGResult(arg);
}


















