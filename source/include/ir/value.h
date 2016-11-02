// value.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
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
		Invalid,

		Internal,
		External,
		ExternalWeak,
	};

	struct ConstantValue;
	struct GlobalValue;
	struct Instruction;
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
		void setType(Type* t) { this->valueType = t; }


		// methods
		void setName(const Identifier& idt);
		void setName(std::string s);
		Identifier getName();
		bool hasName();

		void addUser(Value* user);
		void transferUsesTo(Value* other);

		bool isImmutable() { return this->immut; }
		void makeImmutable() { this->immut = true; }
		void makeNotImmutable() { this->immut = false; }

		std::deque<Value*>& getUsers() { return this->users; }

		Instruction* getSource() { return this->source; }

		// protected shit
		size_t id;
		protected:
		Value(Type* type);
		virtual ~Value() { }

		// fields
		bool immut = 0;

		Identifier ident;
		Type* valueType;
		Instruction* source;
		FValueKind valueKind;
		std::deque<Value*> users;
	};

	struct GlobalValue : Value
	{
		friend struct Module;

		LinkageType linkageType;

		Module* getParentModule() { return this->parentModule; }

		protected:
		GlobalValue(Module* mod, Type* type, LinkageType linkage);

		Module* parentModule = 0;
	};

	struct GlobalVariable : GlobalValue
	{
		friend struct Module;

		GlobalVariable(const Identifier& idt, Module* module, Type* type, bool immutable, LinkageType linkage, ConstantValue* initValue);
		void setInitialValue(ConstantValue* constVal);

		protected:
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




























