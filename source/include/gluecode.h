// gluecode.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "stcommon.h"
#include "string_consts.h"

#define DEBUG_RUNTIME_GLUE_MASTER	0

#define DEBUG_STRING_MASTER			(0 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_STRING_ALLOCATION		(1 & DEBUG_STRING_MASTER)
#define DEBUG_STRING_REFCOUNTING	(1 & DEBUG_STRING_MASTER)

#define DEBUG_ARRAY_MASTER			(0 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_ARRAY_ALLOCATION		(1 & DEBUG_ARRAY_MASTER)
#define DEBUG_ARRAY_REFCOUNTING		(1 & DEBUG_ARRAY_MASTER)

#define DEBUG_ANY_MASTER			(1 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_ANY_ALLOCATION		(1 & DEBUG_ANY_MASTER)
#define DEBUG_ANY_REFCOUNTING		(1 & DEBUG_ANY_MASTER)

#define BUILTIN_ANY_DATA_BYTECOUNT  32
#define BUILTIN_ANY_FLAG_MASK       0x8000000000000000


namespace fir
{
	struct Value;
	struct Function;
	struct ClassType;
	struct UnionType;
	struct ArraySliceType;
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
		void printRuntimeError(CodegenState* cs, fir::Value* pos, const std::string& msg, const std::vector<fir::Value*>& args);

		namespace array
		{
			fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf);
		}

		namespace any
		{
			fir::Function* getRefCountIncrementFunction(CodegenState* cs);
			fir::Function* getRefCountDecrementFunction(CodegenState* cs);

			fir::Function* generateCreateAnyWithValueFunction(CodegenState* cs, fir::Type* type);
			fir::Function* generateGetValueFromAnyFunction(CodegenState* cs, fir::Type* type);
		}

		namespace misc
		{
			fir::Function* getMallocWrapperFunction(CodegenState* cs);
			fir::Function* getRangeSanityCheckFunction(CodegenState* cs);

			fir::Name getCompare_FName(fir::Type* t);
			fir::Name getSetElements_FName(fir::Type* t);
			fir::Name getSetElementsDefault_FName(fir::Type* t);
			fir::Name getCallClassConstructor_FName(fir::Type* t);

			fir::Name getClone_FName(fir::Type* t);
			fir::Name getAppend_FName(fir::Type* t);
			fir::Name getPopBack_FName(fir::Type* t);
			fir::Name getMakeFromTwo_FName(fir::Type* t);
			fir::Name getMakeFromOne_FName(fir::Type* t);
			fir::Name getReserveExtra_FName(fir::Type* t);
			fir::Name getAppendElement_FName(fir::Type* t);
			fir::Name getReserveEnough_FName(fir::Type* t);
			fir::Name getRecursiveRefcount_FName(fir::Type* t, bool incr);

			fir::Name getIncrRefcount_FName(fir::Type* t);
			fir::Name getDecrRefcount_FName(fir::Type* t);

			fir::Name getLoopIncrRefcount_FName(fir::Type* t);
			fir::Name getLoopDecrRefcount_FName(fir::Type* t);

			fir::Name getCreateAnyOf_FName(fir::Type* t);
			fir::Name getGetValueFromAny_FName(fir::Type* t);

			fir::Name getBoundsCheck_FName();
			fir::Name getDecompBoundsCheck_FName();

			fir::Name getMallocWrapper_FName();
			fir::Name getRangeSanityCheck_FName();

			fir::Name getUtf8Length_FName();
		}
	}
}





