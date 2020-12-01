// value.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "type.h"
#include "errors.h"
#include "container.h"

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

		template <typename, size_t> friend struct util::MemoryPool;
		template <typename, size_t> friend struct util::FastInsertVector;

		// congratulations, i fucking played myself.
		enum class Kind
		{
			lvalue,     // has identity, cannot be moved from
			xvalue,     // has identity, can be moved from
			prvalue,    // no identity, can be moved from
		};

		// virtual funcs
		virtual Type* getType();
		void setType(Type* t)   { this->valueType = t; }
		void setKind(Kind k)    { this->kind = k; }

		bool islvalue()     { return this->kind == Kind::lvalue; }
		bool canmove()      { return this->kind == Kind::xvalue || this->kind == Kind::prvalue; }

		bool isConst()      { return this->isconst; }
		void makeConst()    { this->isconst = true; }

		// methods
		void setName(const Name& idt);
		void setName(const std::string& s);
		const Name& getName();
		bool hasName();

		static size_t getCurrentValueId();

		// protected shit
		size_t id;
		protected:

		virtual ~Value() { }
		Value(Type* type, Kind k = Kind::prvalue);


		// fields
		Name ident;
		Type* valueType;
		Kind kind;
		bool isconst = false;
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




























