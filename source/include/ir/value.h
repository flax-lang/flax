// value.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "errors.h"
#include "type.h"

namespace util
{
	template <typename T> struct MemoryPool;
	template <typename T> struct FastInsertVector;
}

namespace fir
{
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

		friend struct util::MemoryPool<Value>;
		friend struct util::FastInsertVector<Value>;

		//? might be renamed when we need more value categories.
		enum class Kind
		{
			lvalue,     // lvalue. same as c
			rvalue,     // same as c.
			clvalue,    // const lvalue
			literal     // literal value. mostly simplifies reference counting.
		};

		// virtual funcs
		virtual Type* getType();
		void setType(Type* t)   { this->valueType = t; }
		void setKind(Kind k)    { this->kind = k; }

		bool islvalue()     { return this->kind == Kind::lvalue; }
		bool isrvalue()     { return this->kind == Kind::rvalue; }
		bool isclvalue()    { return this->kind == Kind::clvalue; }
		bool islorclvalue() { return this->islvalue() || this->isclvalue(); }
		bool isLiteral()    { return this->kind == Kind::literal; }

		void makeConst()    { iceAssert(this->islvalue()); this->kind = Kind::clvalue; }

		// methods
		void setName(const Identifier& idt);
		void setName(std::string s);
		const Identifier& getName();
		bool hasName();

		// protected shit
		size_t id;
		protected:
		Value(Type* type, Kind k = Kind::rvalue);
		virtual ~Value() { }

		// fields
		Identifier ident;
		Type* valueType;
		Kind kind;
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




























