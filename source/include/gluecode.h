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
		void printRuntimeError(CodegenState* cs, fir::Value* pos, std::string msg, std::vector<fir::Value*> args);

		namespace string
		{
			fir::Function* getCloneFunction(CodegenState* cs);
			fir::Function* getAppendFunction(CodegenState* cs);
			fir::Function* getCompareFunction(CodegenState* cs);
			fir::Function* getCharAppendFunction(CodegenState* cs);
			fir::Function* getUnicodeLengthFunction(CodegenState* cs);
			fir::Function* getConstructFromTwoFunction(CodegenState* cs);
			fir::Function* getConstructWithCharFunction(CodegenState* cs);
			fir::Function* getRefCountIncrementFunction(CodegenState* cs);
			fir::Function* getRefCountDecrementFunction(CodegenState* cs);
			fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isDecomp);
		}

		namespace array
		{
			fir::Function* getCloneFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isPerformingDecomposition);
			fir::Function* getElementAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf);
			fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype);

			fir::Function* getReserveExtraFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveAtLeastFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);

			fir::Function* getSetElementsToValueFunction(CodegenState* cs, fir::Type* elmType);
			fir::Function* getSetElementsToDefaultValueFunction(CodegenState* cs, fir::Type* elmType);

			fir::Function* getCallClassConstructorOnElementsFunction(CodegenState* cs, fir::ClassType* cls, sst::FunctionDefn* constr,
				const std::vector<FnCallArgument>& args);
		}

		namespace saa_common
		{
			fir::Function* generateCloneFunction(CodegenState* cs, fir::Type* saa);
			fir::Function* generateAppendFunction(CodegenState* cs, fir::Type* saa);
			fir::Function* generateElementAppendFunction(CodegenState* cs, fir::Type* saa);
			fir::Function* generateConstructFromTwoFunction(CodegenState* cs, fir::Type* saa);
			fir::Function* generateConstructWithElementFunction(CodegenState* cs, fir::Type* saa);

			fir::Function* generateAppropriateAppendFunction(CodegenState* cs, fir::Type* saa, fir::Type* appendee);

			fir::Function* generateBoundsCheckFunction(CodegenState* cs, bool isstring, bool isDecomp);

			fir::Function* generateReserveExtraFunction(CodegenState* cs, fir::Type* saa);
			fir::Function* generateReserveAtLeastFunction(CodegenState* cs, fir::Type* saa);

			fir::Value* makeNewRefCountPointer(CodegenState* cs, fir::Value* rc);
			fir::Value* initSAAWithRefCount(CodegenState* cs, fir::Value* str, fir::Value* rc);
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

			Identifier getCompare_FName(fir::Type* t);
			Identifier getSetElements_FName(fir::Type* t);
			Identifier getSetElementsDefault_FName(fir::Type* t);
			Identifier getCallClassConstructor_FName(fir::Type* t);

			Identifier getClone_FName(fir::Type* t);
			Identifier getAppend_FName(fir::Type* t);
			Identifier getPopBack_FName(fir::Type* t);
			Identifier getMakeFromTwo_FName(fir::Type* t);
			Identifier getMakeFromOne_FName(fir::Type* t);
			Identifier getReserveExtra_FName(fir::Type* t);
			Identifier getAppendElement_FName(fir::Type* t);
			Identifier getReserveEnough_FName(fir::Type* t);
			Identifier getRecursiveRefcount_FName(fir::Type* t, bool incr);

			Identifier getIncrRefcount_FName(fir::Type* t);
			Identifier getDecrRefcount_FName(fir::Type* t);

			Identifier getLoopIncrRefcount_FName(fir::Type* t);
			Identifier getLoopDecrRefcount_FName(fir::Type* t);

			Identifier getCreateAnyOf_FName(fir::Type* t);
			Identifier getGetValueFromAny_FName(fir::Type* t);

			Identifier getBoundsCheck_FName();
			Identifier getDecompBoundsCheck_FName();

			Identifier getMallocWrapper_FName();
			Identifier getRangeSanityCheck_FName();

			Identifier getUtf8Length_FName();
		}
	}
}





