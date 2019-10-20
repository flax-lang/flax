// enums.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::EnumDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->memberType);

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
		names[c->index] = fir::ConstantCharSlice::get(n);
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
		auto array = fir::ConstantArray::get(fir::ArrayType::get(fir::Type::getCharSlice(false), names.size()), names);
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
	if(this->val)
	{
		v = this->val->codegen(cs, base).value;
		iceAssert(v);

		if(dcast(fir::ConstantValue, v) == 0)
			error(this, "enumeration case value ('%s' of type '%s') must be constant", this->id.name, v->getType());
	}
	else
	{
		v = fir::ConstantInt::getNative(this->index);
	}

	this->value = dcast(fir::ConstantValue, v);

	{
		auto ty = this->parentEnum->type;
		auto ret = fir::ConstantEnumCase::get(ty->toEnumType(), fir::ConstantInt::getNative(this->index), this->value);

		cs->valueMap[this] = CGResult(ret);
	}

	return cs->valueMap[this];
}


CGResult sst::EnumDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	error("unsupported");
}






















