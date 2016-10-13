// instruction.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

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
		ICompare_GreaterEqual,
		ICompare_LessEqual,

		FCompare_Equal_ORD,
		FCompare_Equal_UNORD,
		FCompare_NotEqual_ORD,
		FCompare_NotEqual_UNORD,
		FCompare_Greater_ORD,
		FCompare_Greater_UNORD,
		FCompare_Less_ORD,
		FCompare_Less_UNORD,
		FCompare_GreaterEqual_ORD,
		FCompare_GreaterEqual_UNORD,
		FCompare_LessEqual_ORD,
		FCompare_LessEqual_UNORD,

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
		Cast_IntSignedness,

		Integer_ZeroExt,
		Integer_Truncate,

		// unary
		Logical_Not,

		// values
		Value_Load,
		Value_Store,
		Value_StackAlloc,
		Value_CallFunction,
		Value_CallFunctionPointer,
		Value_Return,
		Value_PointerAddition,
		Value_PointerSubtraction,
		Value_GetPointerToStructMember,		// equivalent to GEP(ptr*, ptrIndex, memberIndex) -- for structs.
		Value_GetStructMember,				// equivalent to GEP(ptr*, 0, memberIndex)
		Value_GetPointer,					// equivalent to GEP(ptr*, index)
		Value_GetGEP2,						// equivalent to GEP(ptr*, ptrIndex, elmIndex) -- for arrays/pointers

		// string-specific things
		String_GetData,
		String_SetData,

		String_GetLength,
		String_SetLength,

		String_GetRefCount,
		String_SetRefCount,

		Branch_UnCond,
		Branch_Cond,
	};




	struct IRBuilder;
	struct Instruction : Value
	{
		friend struct Module;
		friend struct IRBuilder;

		Instruction(OpKind kind, bool sideEffects, IRBlock* parent, Type* out, std::deque<Value*> vals);
		std::string str();

		Value* getResult();

		void setValue(Value* v);
		void clearValue();
		bool hasSideEffects();

		// static Instruction* GetBinaryOpInstruction(Ast::ArithmeticOp ao, Value* lhs, Value* rhs);


		// protected:
		OpKind opKind;

		bool sideEffects;
		Value* realOutput;
		IRBlock* parentBlock;
		std::deque<Value*> operands;
	};
}











































