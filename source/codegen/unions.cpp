// unions.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::UnionDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	// there's actually nothing to do.
	// nothing at all.

	return CGResult(0);
}

CGResult sst::UnionVariantDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	return CGResult(0);
}

CGResult sst::RawUnionDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	// again, does nothing.
	return CGResult(0);
}

CGResult sst::UnionVariantConstructor::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto ut = this->parentUnion->type->toUnionType();
	iceAssert(ut);

	auto vt = ut->getVariant(this->variantId)->getInteriorType();
	iceAssert(vt);

	auto uv = cs->irb.CreateValue(ut);

	if(this->args.size() > 0)
	{
		fir::Value* data = 0;
		if(this->args.size() == 1)
		{
			data = this->args[0].value->codegen(cs, vt).value;
			data = cs->oneWayAutocast(data, vt);
			iceAssert(data);
		}
		else
		{
			auto tupt = fir::TupleType::get(zfu::map(this->args, [](const FnCallArgument& fca) -> fir::Type* {
				return fca.value->type;
			}));

			auto tup = cs->irb.CreateValue(tupt);

			size_t i = 0;
			for(const auto& arg : this->args)
			{
				auto v = arg.value->codegen(cs, tupt->getElementN(i)).value;
				tup = cs->irb.InsertValue(tup, { i }, v);

				i++;
			}

			tup = cs->oneWayAutocast(tup, vt);
			iceAssert(tup);

			data = tup;
		}

		uv = cs->irb.SetUnionVariantData(uv, this->variantId, data);
	}

	uv = cs->irb.SetUnionVariantID(uv, this->variantId);
	return CGResult(uv);
}



















