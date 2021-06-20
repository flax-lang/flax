// constructor.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "memorypool.h"

fir::Value* cgn::CodegenState::getConstructedStructValue(fir::StructType* str, const std::vector<FnCallArgument>& args)
{
	fir::Value* value = this->irb.CreateValue(str);

	// set the arguments.
	if(args.size() > 0)
	{
		bool names = !args[0].name.empty();

		// i just keeps track of the index in case we're not using names.
		size_t i = 0;
		for(const auto& arg : args)
		{
			if(names)
			{
				iceAssert(str->hasElementWithName(arg.name));
				auto elmty = str->getElement(arg.name);

				value = this->irb.InsertValueByName(value, arg.name, arg.value->codegen(this, elmty).value);
			}
			else
			{
				iceAssert(str->getElementCount() > i);
				auto elmty = str->getElementN(i);

				value = this->irb.InsertValue(value, { i }, arg.value->codegen(this, elmty).value);
			}

			i++;
		}
	}

	return value;
}



CGResult sst::StructConstructorCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	this->target->codegen(cs);

	if(!this->target)
		error(this, "failed to find target type of constructor call");

	//* note: we don't need an explicit thing telling us whether we should use names or not
	//* if the first argument has no name, then we're not using names; if it has a name, then we are
	//* and ofc expect consistency, but we should have already typechecked that previously.

	StructDefn* str = dcast(StructDefn, this->target);
	if(!str) error(this, "non-struct type '%s' not supported in constructor call", this->target->id.name);

	// great. now we just make the thing.
	fir::Value* value = cs->getConstructedStructValue(str->type->toStructType(), this->arguments);

	return CGResult(value);
}





































