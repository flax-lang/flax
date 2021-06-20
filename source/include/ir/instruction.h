// instruction.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "value.h"


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

		ICompare_Multi,
		FCompare_Multi,

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
		Value_ReadPtr,
		Value_WritePtr,
		Value_StackAlloc,
		Value_CallFunction,
		Value_CallFunctionPointer,
		Value_CallVirtualMethod,
		Value_Return,
		Value_GetPointerToStructMember,		// equivalent to GEP(ptr*, ptrIndex, memberIndex) -- for structs.
		Value_GetStructMember,				// equivalent to GEP(ptr*, 0, memberIndex)
		Value_GetPointer,					// equivalent to GEP(ptr*, index)
		Value_GetGEP2,						// equivalent to GEP(ptr*, ptrIndex, elmIndex) -- for arrays/pointers

		Value_InsertValue,					// corresponds to llvm.
		Value_ExtractValue,					// same

		Value_Select,						// same as llvm: takes 3 operands, the first being a bool and the others being a condition.
											// if the bool == true, returns the first value, else returns the second.
		Value_CreatePHI,

		Misc_Sizeof,						// portable sizeof using GEP

		Value_Dereference,
		Value_AddressOf,
		Value_Store,
		Value_CreateLVal,

		ArraySlice_GetData,
		ArraySlice_SetData,
		ArraySlice_GetLength,
		ArraySlice_SetLength,

		Range_GetLower,
		Range_SetLower,
		Range_GetUpper,
		Range_SetUpper,
		Range_GetStep,
		Range_SetStep,

		Enum_GetIndex,
		Enum_SetIndex,
		Enum_GetValue,
		Enum_SetValue,

		Union_SetValue,
		Union_GetValue,
		Union_GetVariantID,
		Union_SetVariantID,

		RawUnion_GEP,

		Branch_UnCond,
		Branch_Cond,


		Unreachable,
	};




	struct IRBuilder;
	struct Instruction : Value
	{
		friend struct Module;
		friend struct IRBuilder;

		Instruction(OpKind kind, bool sideEffects, Type* out, const std::vector<Value*>& vals);
		Instruction(OpKind kind, bool sideEffects, Type* out, const std::vector<Value*>& vals, Value::Kind k);
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
		std::vector<Value*> operands;
	};
}











































