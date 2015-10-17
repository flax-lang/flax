// value.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "errors.h"

#include <map>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>

#include "type.h"

namespace fir
{
	enum class FValueKind
	{
		Invalid,

		NullValue,

		Constant,
		Normal,
		Global,
	};

	enum class LinkageType
	{
		Internal,
		External,
		ExternalWeak,
	};

	struct ConstantValue;
	struct GlobalValue;
	struct IRBlock;

	struct Value
	{
		friend struct Module;
		friend struct Argument;
		friend struct IRBuilder;
		friend struct Instruction;
		friend struct ConstantValue;

		// virtual funcs
		virtual Type* getType();


		// methods
		void setName(std::string name);
		std::string getName();

		void addUser(Value* user);
		void transferUsesTo(Value* other);

		// protected shit
		size_t id;
		protected:
		Value(Type* type);
		virtual ~Value() { }

		// fields
		Type* valueType;
		std::string valueName;
		FValueKind valueKind;
		std::deque<Value*> users;
	};



	struct GlobalValue : Value
	{
		friend struct Module;

		protected:
		GlobalValue(Type* type, LinkageType linkage);
		LinkageType linkageType;
	};

	struct GlobalVariable : GlobalValue
	{
		friend struct Module;

		GlobalVariable(std::string name, Module* module, Type* type, bool immutable, LinkageType linkage, ConstantValue* initValue);
		void setInitialValue(ConstantValue* constVal);

		protected:
		bool isImmutable;
		Module* parentModule;

		ConstantValue* initValue = 0;
	};

	struct PHINode : Value
	{
		friend struct IRBuilder;
		void addIncoming(Value* v, IRBlock* block);

		protected:
		PHINode(Type* type);

		std::map<IRBlock*, Value*> incoming;
	};
}




























