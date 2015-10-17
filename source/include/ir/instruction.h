// instruction.h
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

#include "errors.h"

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

#include "value.h"

namespace Ast
{
	enum class ArithmeticOp;
}


namespace fir
{
	enum class OpKind
	{
		Invalid,

		// signed
		Signed_Add,
		Signed_Sub,
		Signed_Mul,
		Signed_Div,
		Signed_Mod,
		Signed_Neg,

		// unsigned
		Unsigned_Add,
		Unsigned_Sub,
		Unsigned_Mul,
		Unsigned_Div,
		Unsigned_Mod,

		// floating point
		Floating_Add,
		Floating_Sub,
		Floating_Mul,
		Floating_Div,
		Floating_Mod,
		Floating_Neg,

		Floating_Truncate,
		Floating_Extend,

		// comparison
		ICompare_Equal,
		ICompare_NotEqual,
		ICompare_Greater,
		ICompare_Less,
		IComapre_GreaterEqual,
		ICompare_LessEqual,

		FCompare_Equal_ORD,
		FCompare_Equal_UNORD,
		FCompare_NotEqual_ORD,
		FCompare_NotEqual_UNORD,
		FCompare_Greater,
		FCompare_Less,
		FComapre_GreaterEqual,
		FCompare_LessEqual,

		// logical
		Logical_And,
		Logical_Or,

		// bitwise
		Bitwise_Not,
		Bitwise_Xor,
		Bitwise_Arithmetic_Shr,
		Bitwise_Logical_Shr,
		Bitwise_Shl,
		Bitwise_And,
		Bitwise_Or,

		// casting
		Cast_Bitcast,
		Cast_IntSize,
		Cast_Signedness,
		Cast_FloatToInt,
		Cast_IntToFloat,
		Cast_PointerType,
		Cast_PointerToInt,
		Cast_IntToPointer,

		// values
		Value_Store,


		// unary
		Logical_Not,

		// values
		Value_Load,
		Value_StackAlloc,
		Value_CallFunction,
		Value_Return,
		Value_GetPointerToStructMember,		// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
		Value_GetStructMember,				// equivalent to GEP(ptr*, 0, memberIndex)
		Value_GetPointer,					// equivalent to GEP(ptr*, index)

		Branch_UnCond,
		Branch_Cond,
	};




	struct IRBuilder;
	struct Instruction : Value
	{
		friend struct IRBuilder;

		Instruction(OpKind kind, Type* out, std::deque<Value*> vals);

		Value* getResult();

		void setValue(Value* v);
		void clearValue();

		static Instruction* GetBinaryOpInstruction(Ast::ArithmeticOp ao, Value* lhs, Value* rhs);


		protected:
		OpKind opKind;

		Value* realOutput;
		std::deque<Value*> operands;
	};
}











































