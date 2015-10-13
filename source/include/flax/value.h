// value.h
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

#include "type.h"

namespace flax
{
	enum class FValueKind
	{
		Invalid,

		Constant,
		Normal
	};

	struct ConstantValue;

	struct Value
	{
		// static stuff
		static ConstantValue* getNullValue(Type* type);

		friend struct ConstantValue;

		// virtual funcs
		virtual Type* getType() const;


		// methods
		void setName(std::string name);
		std::string getName();


		// protected shit
		protected:
		Value(Type* type);

		// fields
		Type* valueType;
		std::string valueName;
		FValueKind valueKind;
	};

	struct ConstantValue : Value
	{

		protected:
		ConstantValue();
	};
}




























