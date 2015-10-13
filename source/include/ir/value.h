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

		NullValue,

		Constant,
		Normal,
		Global,
	};

	struct ConstantValue;
	struct GlobalValue;

	struct Value
	{
		friend struct ConstantValue;

		// virtual funcs
		virtual Type* getType();


		// methods
		void setName(std::string name);
		std::string getName();

		void addUser(Value* user);
		void transferUsesTo(Value* other);

		// protected shit
		protected:
		Value(Type* type);
		virtual ~Value() { }

		// fields
		Type* valueType;
		std::string valueName;
		FValueKind valueKind;
		std::deque<Value*> users;
	};

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
		static ConstantInt* getConstantSIntValue(Type* intType, ssize_t val);
		static ConstantInt* getConstantUIntValue(Type* intType, size_t val);

		protected:
		ConstantInt(Type* type, ssize_t val);
		ConstantInt(Type* type, size_t val);

		size_t value;
	};






	struct GlobalValue : Value
	{

		protected:
		GlobalValue();
	};
}




























