// constant.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include "errors.h"

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

#include "value.h"

namespace fir
{
	struct Value;

	// base class implicitly stores null
	struct ConstantValue : Value
	{
		// static stuff
		static ConstantValue* getNullValue(Type* type);


		protected:
		ConstantValue(Type* type);
	};

	struct ConstantInt : ConstantValue
	{
		static ConstantInt* getSigned(Type* intType, ssize_t val);
		static ConstantInt* getUnsigned(Type* intType, size_t val);

		protected:
		ConstantInt(Type* type, ssize_t val);
		ConstantInt(Type* type, size_t val);

		size_t value;
	};


	struct ConstantFP : ConstantValue
	{
		static ConstantFP* get(Type* intType, float val);
		static ConstantFP* get(Type* intType, double val);

		protected:
		ConstantFP(Type* type, float val);
		ConstantFP(Type* type, double val);

		double value;
	};

	struct ConstantArray : ConstantValue
	{
		static ConstantArray* get(Type* type, std::vector<ConstantValue*> vals);

		protected:
		ConstantArray(Type* type, size_t sz);

		std::vector<ConstantValue*> values;
	};
}




























