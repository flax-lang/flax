// value.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "errors.h"
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
		const Identifier& getName();
		bool hasName();

		void addUser(Value* user);
		void transferUsesTo(Value* other);

		bool isImmutable() { return this->immut; }
		void makeImmutable() { this->immut = true; }
		void makeNotImmutable() { this->immut = false; }

		std::vector<Value*>& getUsers() { return this->users; }

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
		std::vector<Value*> users;
	};

	struct PHINode : Value
	{
		friend struct IRBuilder;
		void addIncoming(Value* v, IRBlock* block);

		std::map<IRBlock*, Value*> getValues();

		protected:
		PHINode(Type* type);

		std::map<IRBlock*, Value*> incoming;
	};
}




























