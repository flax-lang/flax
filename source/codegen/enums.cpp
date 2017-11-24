// enums.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::EnumDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->memberType);

	auto rsn = cs->setNamespace(this->id.scope);
	defer(cs->restoreNamespace(rsn));

	cs->enterNamespace(this->id.name);
	defer(cs->leaveNamespace());


	// make the runtime array
	auto values = std::vector<fir::ConstantValue*>(this->cases.size());
	auto names = std::vector<fir::ConstantValue*>(this->cases.size());

	// ok.
	for(auto [ n, c ] : this->cases)
	{
		// do the case...? should we codegen this shit separately or what
		auto val = c->codegen(cs).value;
		iceAssert(val);

		// ok, then.
		auto cv = dcast(fir::ConstantValue, val);
		iceAssert(cv);

		// values[c->index] = fir::ConstantStruct(cv);
		names[c->index] = fir::ConstantString::get(c->id.name);
	}


	auto et = this->type->toEnumType();

	// this is for the actual case
	{
		// auto array = fir::ConstantArray::get(fir::ArrayType::get(this->type, values.size()), values);
		// et->setCaseArray(cs->module->createGlobalVariable(Identifier("_FV_ENUM_ARR_" + this->id.str(), IdKind::Name),
		// 	array->getType(), array, true, fir::LinkageType::Internal));
	}

	// this is for the names... I guess?
	{
		auto array = fir::ConstantArray::get(fir::ArrayType::get(fir::StringType::get(), names.size()), names);
		et->setNameArray(cs->module->createGlobalVariable(Identifier("_FV_ENUM_NAME_ARR_" + this->id.str(), IdKind::Name),
			array->getType(), array, true, fir::LinkageType::Internal));
	}


	return CGResult(0);
}


CGResult sst::EnumCaseDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto base = this->parentEnum->memberType;

	fir::Value* v = 0;
	if(this->value)
	{
		v = this->val->codegen(cs, base).value;
		iceAssert(v);

		if(dcast(fir::ConstantValue, v) == 0)
			error(this, "Enumeration case value ('%s' of type '%s') must be constant", this->id.name, v->getType());
	}
	else
	{
		v = fir::ConstantInt::getInt64(this->index);
	}

	this->value = dcast(fir::ConstantValue, v);
	return CGResult(this->value);
}


CGResult sst::EnumDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto enr = this->enumeration;
	iceAssert(enr);

	auto ecd = enr->cases[this->caseName];
	iceAssert(ecd);

	// ok, return the thing
	auto ty = enr->type;

	auto ret = cs->irb.CreateValue(ty);
	ret = cs->irb.CreateSetEnumCaseIndex(ret, fir::ConstantInt::getInt64(ecd->index));
	ret = cs->irb.CreateSetEnumCaseValue(ret, ecd->value);

	return CGResult(ret);
}





















