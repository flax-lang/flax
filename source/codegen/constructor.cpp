// constructor.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::ConstructorCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(!this->target)
		error(this, "Failed to find target type of constructor call");

	//* note: we don't need an explicit thing telling us whether we should use names or not
	//* if the first argument has no name, then we're not using names; if it has a name, then we are
	//* and ofc expect consistency, but we should have already typechecked that previously.

	StructDefn* str = dcast(StructDefn, this->target);
	if(!str) error(this, "Non-struct type '%s' not supported in constructor call (yet?)", this->target->id.name);

	// great. now we just make the thing.
	fir::StructType* stry = str->type->toStructType();
	fir::Value* value = cs->irb.CreateValue(str->type);

	// set the arguments.
	if(this->arguments.size() > 0)
	{
		bool names = !this->arguments[0].name.empty();

		// i just keeps track of the index in case we're not using names.
		size_t i = 0;
		for(const auto& arg : this->arguments)
		{
			if(names)
			{
				iceAssert(stry->hasElementWithName(arg.name));
				auto elmty = stry->getElement(arg.name);

				value = cs->irb.InsertValueByName(value, arg.name, arg.value->codegen(cs, elmty).value);
			}
			else
			{
				iceAssert(stry->getElementCount() > i);
				auto elmty = stry->getElementN(i);

				value = cs->irb.InsertValue(value, { i }, arg.value->codegen(cs, elmty).value);
			}

			i++;
		}

		if(names) iceAssert(i == stry->getElementCount());
	}

	return CGResult(value);
}