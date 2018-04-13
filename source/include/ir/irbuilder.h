// irbuilder.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "block.h"
#include "value.h"
#include "module.h"
#include "function.h"
#include "constant.h"

namespace fir
{
	struct IRBuilder
	{
		IRBuilder(FTContext* c);

		Value* Negate(Value* a, std::string vname = "");
		Value* Add(Value* a, Value* b, std::string vname = "");
		Value* Subtract(Value* a, Value* b, std::string vname = "");
		Value* Multiply(Value* a, Value* b, std::string vname = "");
		Value* Divide(Value* a, Value* b, std::string vname = "");
		Value* Modulo(Value* a, Value* b, std::string vname = "");
		Value* ICmpEQ(Value* a, Value* b, std::string vname = "");
		Value* ICmpNEQ(Value* a, Value* b, std::string vname = "");
		Value* ICmpGT(Value* a, Value* b, std::string vname = "");
		Value* ICmpLT(Value* a, Value* b, std::string vname = "");
		Value* ICmpGEQ(Value* a, Value* b, std::string vname = "");
		Value* ICmpLEQ(Value* a, Value* b, std::string vname = "");
		Value* FCmpEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpEQ_UNORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpNEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpNEQ_UNORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpGT_ORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpGT_UNORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpLT_ORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpLT_UNORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpGEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpGEQ_UNORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpLEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* FCmpLEQ_UNORD(Value* a, Value* b, std::string vname = "");

		Value* ICmpMulti(Value* a, Value* b, std::string vname = "");
		Value* FCmpMulti(Value* a, Value* b, std::string vname = "");

		Value* BitwiseXOR(Value* a, Value* b, std::string vname = "");
		Value* BitwiseLogicalSHR(Value* a, Value* b, std::string vname = "");
		Value* BitwiseArithmeticSHR(Value* a, Value* b, std::string vname = "");
		Value* BitwiseSHL(Value* a, Value* b, std::string vname = "");
		Value* BitwiseAND(Value* a, Value* b, std::string vname = "");
		Value* BitwiseOR(Value* a, Value* b, std::string vname = "");
		Value* BitwiseNOT(Value* a, std::string vname = "");

		Value* Bitcast(Value* v, Type* targetType, std::string vname = "");
		Value* IntSizeCast(Value* v, Type* targetType, std::string vname = "");
		Value* FloatToIntCast(Value* v, Type* targetType, std::string vname = "");
		Value* IntToFloatCast(Value* v, Type* targetType, std::string vname = "");
		Value* PointerTypeCast(Value* v, Type* targetType, std::string vname = "");
		Value* PointerToIntCast(Value* v, Type* targetType, std::string vname = "");
		Value* IntToPointerCast(Value* v, Type* targetType, std::string vname = "");
		Value* IntSignednessCast(Value* v, Type* targetType, std::string vname = "");

		Value* AppropriateCast(Value* v, Type* targetType, std::string vname = "");

		Value* IntTruncate(Value* v, Type* targetType, std::string vname = "");
		Value* IntZeroExt(Value* v, Type* targetType, std::string vname = "");

		Value* FTruncate(Value* v, Type* targetType, std::string vname = "");
		Value* FExtend(Value* v, Type* targetType, std::string vname = "");

		Value* Load(Value* ptr, std::string vname = "");
		Value* Store(Value* v, Value* ptr);

		Value* Call(Function* fn, std::string vname = "");
		Value* Call(Function* fn, Value* p1, std::string vname = "");
		Value* Call(Function* fn, Value* p1, Value* p2, std::string vname = "");
		Value* Call(Function* fn, Value* p1, Value* p2, Value* p3, std::string vname = "");

		Value* Call(Function* fn, std::vector<Value*> args, std::string vname = "");
		Value* Call(Function* fn, std::initializer_list<Value*> args, std::string vname = "");

		Value* CallToFunctionPointer(Value* fn, FunctionType* ft, std::vector<Value*> args, std::string vname = "");

		Value* CallVirtualMethod(ClassType* cls, FunctionType* ft, size_t index, std::vector<Value*> args, std::string vname = "");

		Value* Return(Value* v);
		Value* ReturnVoid();

		Value* LogicalNot(Value* v, std::string vname = "");

		PHINode* CreatePHINode(Type* type, std::string vname = "");
		Value* StackAlloc(Type* type, std::string vname = "");
		Value* ImmutStackAlloc(Type* type, Value* initialValue, std::string vname = "");

		Value* CreateSliceFromString(Value* str, bool mut, std::string vname = "");

		// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)

		// equivalent to GEP(ptr*, 0, memberIndex)
		Value* GetStructMember(Value* ptr, std::string memberName);
		Value* StructGEP(Value* structPtr, size_t memberIndex, std::string vname = "");

		// equivalent to GEP(ptr*, index)
		Value* GetPointer(Value* ptr, Value* ptrIndex, std::string vname = "");

		// equivalent to GEP(ptr*, ptrIndex, elmIndex)
		Value* GEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, std::string vname = "");
		Value* ConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, std::string vname = "");

		void SetVtable(Value* ptr, Value* table, std::string vname = "");

		void CondBranch(Value* condition, IRBlock* trueBlock, IRBlock* falseBlock);
		void UnCondBranch(IRBlock* target);


		Value* Sizeof(Type* t, std::string vname = "");

		Value* Select(Value* cond, Value* one, Value* two, std::string vname = "");

		Value* BinaryOp(std::string ao, Value* a, Value* b, std::string vname = "");


		Value* PointerAdd(Value* ptr, Value* num, std::string vname = "");
		Value* PointerSub(Value* ptr, Value* num, std::string vname = "");


		// Value* AggregateValue(Type* t, std::vector<Value*> values, std::string vname = "");
		Value* CreateValue(Type* t, std::string vname = "");

		Value* ExtractValue(Value* val, std::vector<size_t> inds, std::string vname = "");
		Value* ExtractValueByName(Value* val, std::string mem, std::string vname = "");

		[[nodiscard]] Value* InsertValueByName(Value* val, std::string mem, Value* elm, std::string vname = "");
		[[nodiscard]] Value* InsertValue(Value* val, std::vector<size_t> inds, Value* elm, std::string vname = "");


		Value* GetStringData(Value* str, std::string vname = "");
		Value* GetStringLength(Value* str, std::string vname = "");
		Value* GetStringRefCount(Value* str, std::string vname = "");

		[[nodiscard]] Value* SetStringLength(Value* str, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetStringData(Value* str, Value* val, std::string vname = "");
		void SetStringRefCount(Value* str, Value* val, std::string vname = "");

		Value* GetDynamicArrayData(Value* arr, std::string vname = "");
		Value* GetDynamicArrayLength(Value* arr, std::string vname = "");
		Value* GetDynamicArrayCapacity(Value* arr, std::string vname = "");
		Value* GetDynamicArrayRefCount(Value* arr, std::string vname = "");
		Value* GetDynamicArrayRefCountPointer(Value* arr, std::string vname = "");

		[[nodiscard]] Value* SetDynamicArrayData(Value* arr, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetDynamicArrayLength(Value* arr, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetDynamicArrayCapacity(Value* arr, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetDynamicArrayRefCountPointer(Value* arr, Value* ptr, std::string vname = "");

		void SetDynamicArrayRefCount(Value* arr, Value* val, std::string vname = "");

		Value* GetArraySliceData(Value* arr, std::string vname = "");
		Value* GetArraySliceLength(Value* arr, std::string vname = "");

		[[nodiscard]] Value* SetArraySliceData(Value* arr, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetArraySliceLength(Value* arr, Value* val, std::string vname = "");

		Value* GetAnyTypeID(Value* any, std::string vname = "");
		Value* GetAnyFlag(Value* any, std::string vname = "");
		Value* GetAnyData(Value* any, std::string vname = "");

		[[nodiscard]] Value* SetAnyFlag(Value* any, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetAnyData(Value* any, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetAnyTypeID(Value* any, Value* val, std::string vname = "");

		Value* GetRangeLower(Value* range, std::string vname = "");
		Value* GetRangeUpper(Value* range, std::string vname = "");
		Value* GetRangeStep(Value* range, std::string vname = "");

		[[nodiscard]] Value* SetRangeLower(Value* range, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetRangeUpper(Value* range, Value* val, std::string vname = "");
		[[nodiscard]] Value* SetRangeStep(Value* range, Value* step, std::string vname = "");

		Value* GetEnumCaseIndex(Value* ecs, std::string vname = "");
		Value* GetEnumCaseValue(Value* ecs, std::string vname = "");

		[[nodiscard]] Value* SetEnumCaseIndex(Value* ecs, Value* idx, std::string vname = "");
		[[nodiscard]] Value* SetEnumCaseValue(Value* ecs, Value* val, std::string vname = "");


		void Unreachable();


		IRBlock* addNewBlockInFunction(std::string name, Function* func);
		IRBlock* addNewBlockAfter(std::string name, IRBlock* block);


		void setCurrentBlock(IRBlock* block);
		void restorePreviousBlock();

		Function* getCurrentFunction();
		IRBlock* getCurrentBlock();


		private:
		Value* addInstruction(Instruction* instr, std::string vname);

		FTContext* context;

		Function* currentFunction = 0;
		IRBlock* currentBlock = 0;
		IRBlock* previousBlock = 0;
	};
}




















































