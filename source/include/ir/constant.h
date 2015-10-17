// constant.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <limits.h>

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
		friend struct Module;

		// static stuff
		static ConstantValue* getNullValue(Type* type);


		protected:
		ConstantValue(Type* type);
	};

	struct ConstantInt : ConstantValue
	{
		friend struct Module;

		static ConstantInt* getSigned(Type* intType, ssize_t val);
		static ConstantInt* getUnsigned(Type* intType, size_t val);

		ssize_t getSignedValue();
		size_t getUnsignedValue();

		protected:
		ConstantInt(Type* type, ssize_t val);
		ConstantInt(Type* type, size_t val);

		size_t value;
	};


	struct ConstantFP : ConstantValue
	{
		friend struct Module;

		static ConstantFP* get(Type* intType, float val);
		static ConstantFP* get(Type* intType, double val);

		double getValue();

		protected:
		ConstantFP(Type* type, float val);
		ConstantFP(Type* type, double val);

		double value;
	};

	struct ConstantArray : ConstantValue
	{
		friend struct Module;

		static ConstantArray* get(Type* type, std::vector<ConstantValue*> vals);

		std::vector<ConstantValue*> getValues() { return this->values; }

		protected:
		ConstantArray(Type* type, std::vector<ConstantValue*> vals);

		std::vector<ConstantValue*> values;
	};
}




























