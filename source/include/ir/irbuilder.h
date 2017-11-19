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

		Value* CreateNeg(Value* a, std::string vname = "");
		Value* CreateAdd(Value* a, Value* b, std::string vname = "");
		Value* CreateSub(Value* a, Value* b, std::string vname = "");
		Value* CreateMul(Value* a, Value* b, std::string vname = "");
		Value* CreateDiv(Value* a, Value* b, std::string vname = "");
		Value* CreateMod(Value* a, Value* b, std::string vname = "");
		Value* CreateICmpEQ(Value* a, Value* b, std::string vname = "");
		Value* CreateICmpNEQ(Value* a, Value* b, std::string vname = "");
		Value* CreateICmpGT(Value* a, Value* b, std::string vname = "");
		Value* CreateICmpLT(Value* a, Value* b, std::string vname = "");
		Value* CreateICmpGEQ(Value* a, Value* b, std::string vname = "");
		Value* CreateICmpLEQ(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpEQ_UNORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpNEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpNEQ_UNORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpGT_ORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpGT_UNORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpLT_ORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpLT_UNORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpGEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpGEQ_UNORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpLEQ_ORD(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpLEQ_UNORD(Value* a, Value* b, std::string vname = "");

		Value* CreateICmpMulti(Value* a, Value* b, std::string vname = "");
		Value* CreateFCmpMulti(Value* a, Value* b, std::string vname = "");

		Value* CreateBitwiseXOR(Value* a, Value* b, std::string vname = "");
		Value* CreateBitwiseLogicalSHR(Value* a, Value* b, std::string vname = "");
		Value* CreateBitwiseArithmeticSHR(Value* a, Value* b, std::string vname = "");
		Value* CreateBitwiseSHL(Value* a, Value* b, std::string vname = "");
		Value* CreateBitwiseAND(Value* a, Value* b, std::string vname = "");
		Value* CreateBitwiseOR(Value* a, Value* b, std::string vname = "");
		Value* CreateBitwiseNOT(Value* a, std::string vname = "");

		Value* CreateBitcast(Value* v, Type* targetType, std::string vname = "");
		Value* CreateIntSizeCast(Value* v, Type* targetType, std::string vname = "");
		Value* CreateFloatToIntCast(Value* v, Type* targetType, std::string vname = "");
		Value* CreateIntToFloatCast(Value* v, Type* targetType, std::string vname = "");
		Value* CreatePointerTypeCast(Value* v, Type* targetType, std::string vname = "");
		Value* CreatePointerToIntCast(Value* v, Type* targetType, std::string vname = "");
		Value* CreateIntToPointerCast(Value* v, Type* targetType, std::string vname = "");
		Value* CreateIntSignednessCast(Value* v, Type* targetType, std::string vname = "");

		Value* CreateAppropriateCast(Value* v, Type* targetType, std::string vname = "");

		Value* CreateIntTruncate(Value* v, Type* targetType, std::string vname = "");
		Value* CreateIntZeroExt(Value* v, Type* targetType, std::string vname = "");

		Value* CreateFTruncate(Value* v, Type* targetType, std::string vname = "");
		Value* CreateFExtend(Value* v, Type* targetType, std::string vname = "");

		Value* CreateLoad(Value* ptr, std::string vname = "");
		Value* CreateStore(Value* v, Value* ptr);
		Value* CreateCall0(Function* fn, std::string vname = "");
		Value* CreateCall1(Function* fn, Value* p1, std::string vname = "");
		Value* CreateCall2(Function* fn, Value* p1, Value* p2, std::string vname = "");
		Value* CreateCall3(Function* fn, Value* p1, Value* p2, Value* p3, std::string vname = "");
		Value* CreateCall(Function* fn, std::vector<Value*> args, std::string vname = "");
		Value* CreateCall(Function* fn, std::initializer_list<Value*> args, std::string vname = "");

		Value* CreateCallToFunctionPointer(Value* fn, FunctionType* ft, std::vector<Value*> args, std::string vname = "");

		Value* CreateReturn(Value* v);
		Value* CreateReturnVoid();

		Value* CreateLogicalNot(Value* v, std::string vname = "");

		PHINode* CreatePHINode(Type* type, std::string vname = "");
		Value* CreateStackAlloc(Type* type, std::string vname = "");
		Value* CreateImmutStackAlloc(Type* type, Value* initialValue, std::string vname = "");

		// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
		Value* CreateGetPointerToConstStructMember(Value* ptr, Value* ptrIndex, size_t memberIndex, std::string vname = "");

		// equivalent to GEP(ptr*, 0, memberIndex)
		Value* CreateGetStructMember(Value* ptr, std::string memberName);
		Value* CreateStructGEP(Value* structPtr, size_t memberIndex, std::string vname = "");

		// equivalent to GEP(ptr*, index)
		Value* CreateGetPointer(Value* ptr, Value* ptrIndex, std::string vname = "");

		// equivalent to GEP(ptr*, ptrIndex, elmIndex)
		Value* CreateGEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, std::string vname = "");
		Value* CreateConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, std::string vname = "");

		void CreateCondBranch(Value* condition, IRBlock* trueBlock, IRBlock* falseBlock);
		void CreateUnCondBranch(IRBlock* target);


		Value* CreateSizeof(Type* t, std::string vname = "");


		Value* CreateBinaryOp(Operator ao, Value* a, Value* b, std::string vname = "");


		Value* CreatePointerAdd(Value* ptr, Value* num, std::string vname = "");
		Value* CreatePointerSub(Value* ptr, Value* num, std::string vname = "");


		Value* CreateAggregateValue(Type* t, std::vector<Value*> values, std::string vname = "");
		Value* CreateValue(Type* t, std::string vname = "");

		Value* CreateInsertValue(Value* val, std::vector<size_t> inds, Value* elm, std::string vname = "");
		Value* CreateExtractValue(Value* val, std::vector<size_t> inds, std::string vname = "");

		Value* CreateInsertValueByName(Value* val, std::string mem, Value* elm, std::string vname = "");
		Value* CreateExtractValueByName(Value* val, std::string mem, std::string vname = "");


		Value* CreateGetStringData(Value* str, std::string vname = "");
		Value* CreateSetStringData(Value* str, Value* val, std::string vname = "");
		Value* CreateGetStringLength(Value* str, std::string vname = "");
		Value* CreateSetStringLength(Value* str, Value* val, std::string vname = "");
		Value* CreateGetStringRefCount(Value* str, std::string vname = "");
		void CreateSetStringRefCount(Value* str, Value* val, std::string vname = "");


		Value* CreateGetDynamicArrayData(Value* arr, std::string vname = "");
		Value* CreateSetDynamicArrayData(Value* arr, Value* val, std::string vname = "");
		Value* CreateGetDynamicArrayLength(Value* arr, std::string vname = "");
		Value* CreateSetDynamicArrayLength(Value* arr, Value* val, std::string vname = "");
		Value* CreateGetDynamicArrayCapacity(Value* arr, std::string vname = "");
		Value* CreateSetDynamicArrayCapacity(Value* arr, Value* val, std::string vname = "");
		Value* CreateGetDynamicArrayRefCount(Value* arr, std::string vname = "");
		void CreateSetDynamicArrayRefCount(Value* arr, Value* val, std::string vname = "");


		Value* CreateGetArraySliceData(Value* arr, std::string vname = "");
		Value* CreateSetArraySliceData(Value* arr, Value* val, std::string vname = "");
		Value* CreateGetArraySliceLength(Value* arr, std::string vname = "");
		Value* CreateSetArraySliceLength(Value* arr, Value* val, std::string vname = "");

		Value* CreateGetAnyTypeID(Value* any, std::string vname = "");
		Value* CreateSetAnyTypeID(Value* any, Value* val, std::string vname = "");
		Value* CreateGetAnyFlag(Value* any, std::string vname = "");
		Value* CreateSetAnyFlag(Value* any, Value* val, std::string vname = "");
		Value* CreateGetAnyData(Value* any, std::string vname = "");
		Value* CreateSetAnyData(Value* any, Value* val, std::string vname = "");

		Value* CreateGetRangeLower(Value* range, std::string vname = "");
		Value* CreateSetRangeLower(Value* range, Value* val, std::string vname = "");
		Value* CreateGetRangeUpper(Value* range, std::string vname = "");
		Value* CreateSetRangeUpper(Value* range, Value* val, std::string vname = "");

		Value* CreateGetEnumCaseIndex(Value* ecs, std::string vname = "");
		Value* CreateSetEnumCaseIndex(Value* ecs, Value* idx, std::string vname = "");
		Value* CreateGetEnumCaseValue(Value* ecs, std::string vname = "");
		Value* CreateSetEnumCaseValue(Value* ecs, Value* val, std::string vname = "");


		void CreateUnreachable();


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




















































