// function.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "memorypool.h"

#include "ir/irbuilder.h"

CGResult sst::FunctionDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->type->containsPlaceholders())
		return CGResult(0);

	std::vector<fir::Type*> ptypes;

	for(auto p : this->params)
		ptypes.push_back(p.type);

	auto ft = fir::FunctionType::get(ptypes, this->returnType);

	auto ident = this->id;
	if(this->attrs.hasAny(attr::FN_ENTRYPOINT, attr::NO_MANGLE))
		ident = Identifier(this->id.name, IdKind::Name);

	auto fn = cs->module->getOrCreateFunction(ident, ft,
		this->visibility == VisibilityLevel::Private ? fir::LinkageType::Internal : fir::LinkageType::External);

	// manually set the names, I guess
	{
		for(size_t i = 0; i < this->params.size(); i++)
			fn->getArguments()[i]->setName(this->params[i].name);
	}


	// special case for functions, to enable recursive calling:
	// set our cached result *before* we generate our body, in case we contain either
	// a: a call to ourselves, or
	// b: a call to someone that call us eventually

	this->cachedResult = CGResult(fn);

	auto restore = cs->irb.getCurrentBlock();
	defer(cs->irb.setCurrentBlock(restore));

	cs->enterFunction(fn);
	defer(cs->leaveFunction());

	auto block = cs->irb.addNewBlockInFunction(this->id.name + "_entry", fn);
	cs->irb.setCurrentBlock(block);

	if(this->parentTypeForMethod)
		cs->enterMethodBody(fn, cs->irb.Dereference(fn->getArguments()[0]));


	// special thing here:
	// push a breakable block (ControlFlowPoint) so that a manual 'return' can know how to
	// do refcounting and deferred things.

	cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, 0, 0));
	{
		this->body->preBodyCode = [cs, this]() {
			for(auto arg : this->arguments)
				arg->codegen(cs);
		};

		this->body->codegen(cs);
	}
	cs->leaveBreakableBody();

	// note that we *trust* in the typechecker
	// that all paths return the correct type.


	if(this->needReturnVoid)
		cs->irb.ReturnVoid();

	if(this->parentTypeForMethod)
		cs->leaveMethodBody();

	if(this->attrs.has(attr::FN_ENTRYPOINT))
	{
		if(cs->entryFunction.first != 0)
		{
			SimpleError::make(this->loc, "redefinition of entry function with '@entry'")
				->append(SimpleError::make(MsgType::Note, cs->entryFunction.second, "previous entry function marked here:"))
				->postAndQuit();
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

	auto realId = Identifier(this->realName, IdKind::Name);

	auto ef = cs->module->getFunction(realId);
	if(ef && ef->getType() != ft)
	{
		error(this, "foreign function '%s' already defined elsewhere (with signature %s); overloading not possible",
			this->id.str(), ef->getType());
	}

	auto fn = cs->module->getOrCreateFunction(realId, ft, fir::LinkageType::External);

	if(this->isIntrinsic)
		fn->setIsIntrinsic();

	cs->valueMap[this] = CGResult(fn);
	return CGResult(fn);
}




CGResult sst::ArgumentDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto fn = cs->getCurrentFunction();

	auto arg = cs->irb.CreateLValue(this->type, this->id.name);
	cs->irb.Store(fn->getArgumentWithName(this->id.name), arg);
	arg->makeConst();

	cs->addRAIIOrRCValueIfNecessary(arg);

	// ok...
	cs->valueMap[this] = CGResult(arg);
	return CGResult(arg);
}


















