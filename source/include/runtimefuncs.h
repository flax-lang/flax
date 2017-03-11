// runtimefuncs.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "ir/value.h"


#define ALLOCATE_MEMORY_FUNC						"malloc"
#define REALLOCATE_MEMORY_FUNC						"realloc"
#define FREE_MEMORY_FUNC							"free"


namespace Codegen
{
	struct CodegenInstance;

	namespace RuntimeFuncs
	{
		namespace String
		{
			fir::Function* getRefCountIncrementFunction(CodegenInstance* cgi);
			fir::Function* getRefCountDecrementFunction(CodegenInstance* cgi);
			fir::Function* getCompareFunction(CodegenInstance* cgi);
			fir::Function* getAppendFunction(CodegenInstance* cgi);
			fir::Function* getCloneFunction(CodegenInstance* cgi);
			fir::Function* getCharAppendFunction(CodegenInstance* cgi);

			fir::Function* getBoundsCheckFunction(CodegenInstance* cgi);
			fir::Function* getCheckLiteralWriteFunction(CodegenInstance* cgi);
		}

		namespace Array
		{
			fir::Function* getBoundsCheckFunction(CodegenInstance* cgi, bool isPerformingDecomposition = false);

			fir::Function* getIncrementArrayRefCountFunction(CodegenInstance* cgi, fir::Type* elmtype);
			fir::Function* getDecrementArrayRefCountFunction(CodegenInstance* cgi, fir::Type* elmtype);

			fir::Function* getConstructFromTwoFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
			fir::Function* getPopElementFromBackFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveSpaceForElementsFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveExtraSpaceForElementsFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);

			fir::Function* getAppendFunction(CodegenInstance* cgi, fir::ArraySliceType* arrtype);
			fir::Function* getAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);

			fir::Function* getElementAppendFunction(CodegenInstance* cgi, fir::ArraySliceType* arrtype);
			fir::Function* getElementAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);



			// well. this is stupid.
			// unfortunately templates don't work the way i want them to
			// and it's not possible to get dynamic dispatch on non-methods
			// ie. the real type of a fir::Type cannot be known at compile time and used to dispatch.
			// sadface.

			fir::Function* getCompareFunction(CodegenInstance* cgi, fir::Type* arrtype, fir::Function* cmpf);

			// no shit for this, because slices don't work like dynamic arrays (not exactly)
			fir::Function* getAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
			fir::Function* getElementAppendFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);

			fir::Function* getCloneFunction(CodegenInstance* cgi, fir::Type* arrtype);
			fir::Function* getCloneFunction(CodegenInstance* cgi, fir::ArraySliceType* arrtype);
			fir::Function* getCloneFunction(CodegenInstance* cgi, fir::DynamicArrayType* arrtype);
		}

		namespace Any
		{
			fir::Function* getRefCountIncrementFunction(CodegenInstance* cgi);
			fir::Function* getRefCountDecrementFunction(CodegenInstance* cgi);
		}
	}
}




