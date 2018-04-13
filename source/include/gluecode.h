// gluecode.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "stcommon.h"

#define DEBUG_RUNTIME_GLUE_MASTER	0

#define DEBUG_STRING_MASTER			(0 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_STRING_ALLOCATION		(0 & DEBUG_STRING_MASTER)
#define DEBUG_STRING_REFCOUNTING	(0 & DEBUG_STRING_MASTER)

#define DEBUG_ARRAY_MASTER			(1 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_ARRAY_ALLOCATION		(1 & DEBUG_ARRAY_MASTER)
#define DEBUG_ARRAY_REFCOUNTING		(0 & DEBUG_ARRAY_MASTER)


namespace fir
{
	struct Value;
	struct Function;
	struct ClassType;
	struct ArraySliceType;
	struct DynamicArrayType;
}

namespace sst
{
	struct FunctionDefn;
}

namespace cgn
{
	struct CodegenState;

	namespace glue
	{
		void printError(CodegenState* cs, fir::Value* pos, std::string msg, std::vector<fir::Value*> args);

		namespace string
		{
			fir::Function* getCloneFunction(CodegenState* cs);
			fir::Function* getAppendFunction(CodegenState* cs);
			fir::Function* getCompareFunction(CodegenState* cs);
			fir::Function* getCharAppendFunction(CodegenState* cs);
			fir::Function* getBoundsCheckFunction(CodegenState* cs);
			fir::Function* getUnicodeLengthFunction(CodegenState* cs);
			fir::Function* getConstructFromTwoFunction(CodegenState* cs);
			fir::Function* getConstructWithCharFunction(CodegenState* cs);
			fir::Function* getRefCountIncrementFunction(CodegenState* cs);
			fir::Function* getRefCountDecrementFunction(CodegenState* cs);
			fir::Function* getCheckLiteralWriteFunction(CodegenState* cs);
		}

		namespace array
		{
			fir::Function* getCloneFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getCloneFunction(CodegenState* cs, fir::ArraySliceType* arrtype);
			fir::Function* getCloneFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isPerformingDecomposition);
			fir::Function* getElementAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf);
			fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getReserveSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveExtraSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);

			fir::Function* getSetElementsToValueFunction(CodegenState* cs, fir::Type* elmType);
			fir::Function* getSetElementsToDefaultValueFunction(CodegenState* cs, fir::Type* elmType);

			fir::Function* getCallClassConstructorOnElementsFunction(CodegenState* cs, fir::ClassType* cls, sst::FunctionDefn* constr,
				const std::vector<FnCallArgument>& args);
		}

		namespace misc
		{
			fir::Function* getRangeSanityCheckFunction(CodegenState* cs);
		}
	}
}





