// runtimefuncs.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "ir/value.h"

namespace Codegen
{
	struct CodegenInstance;
}

namespace RuntimeFuncs
{
	fir::Function* getArrayBoundsCheckFunction(Codegen::CodegenInstance* cgi);

	fir::Function* getDynamicArrayAppendFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
	fir::Function* getDynamicArrayCompareFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
	fir::Function* getDynamicArrayElementAppendFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
	fir::Function* getDynamicArrayConstructFromTwoFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
	fir::Function* getDynamicArrayPopElementFromBackFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
	fir::Function* getDynamicArrayReserveSpaceForElementsFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
	fir::Function* getDynamicArrayReserveExtraSpaceForElementsFunction(Codegen::CodegenInstance* cgi, fir::DynamicArrayType* arrtype);

	fir::Function* getStringRefCountIncrementFunction(Codegen::CodegenInstance* cgi);
	fir::Function* getStringRefCountDecrementFunction(Codegen::CodegenInstance* cgi);
	fir::Function* getStringCompareFunction(Codegen::CodegenInstance* cgi);
	fir::Function* getStringAppendFunction(Codegen::CodegenInstance* cgi);
	fir::Function* getStringCharAppendFunction(Codegen::CodegenInstance* cgi);

	fir::Function* getStringBoundsCheckFunction(Codegen::CodegenInstance* cgi);
	fir::Function* getStringCheckLiteralWriteFunction(Codegen::CodegenInstance* cgi);
}
