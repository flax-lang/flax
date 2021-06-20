// irbuilder.h
// Copyright (c) 2014 - 2016, zhiayang
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
		IRBuilder(Module* mod);

		Value* Negate(Value* a, const std::string& vname = "");
		Value* Add(Value* a, Value* b, const std::string& vname = "");
		Value* Subtract(Value* a, Value* b, const std::string& vname = "");
		Value* Multiply(Value* a, Value* b, const std::string& vname = "");
		Value* Divide(Value* a, Value* b, const std::string& vname = "");
		Value* Modulo(Value* a, Value* b, const std::string& vname = "");
		Value* ICmpEQ(Value* a, Value* b, const std::string& vname = "");
		Value* ICmpNEQ(Value* a, Value* b, const std::string& vname = "");
		Value* ICmpGT(Value* a, Value* b, const std::string& vname = "");
		Value* ICmpLT(Value* a, Value* b, const std::string& vname = "");
		Value* ICmpGEQ(Value* a, Value* b, const std::string& vname = "");
		Value* ICmpLEQ(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpEQ_ORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpEQ_UNORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpNEQ_ORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpNEQ_UNORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpGT_ORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpGT_UNORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpLT_ORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpLT_UNORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpGEQ_ORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpGEQ_UNORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpLEQ_ORD(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpLEQ_UNORD(Value* a, Value* b, const std::string& vname = "");

		Value* ICmpMulti(Value* a, Value* b, const std::string& vname = "");
		Value* FCmpMulti(Value* a, Value* b, const std::string& vname = "");

		Value* BitwiseXOR(Value* a, Value* b, const std::string& vname = "");
		Value* BitwiseLogicalSHR(Value* a, Value* b, const std::string& vname = "");
		Value* BitwiseArithmeticSHR(Value* a, Value* b, const std::string& vname = "");
		Value* BitwiseSHL(Value* a, Value* b, const std::string& vname = "");
		Value* BitwiseAND(Value* a, Value* b, const std::string& vname = "");
		Value* BitwiseOR(Value* a, Value* b, const std::string& vname = "");
		Value* BitwiseNOT(Value* a, const std::string& vname = "");

		Value* Bitcast(Value* v, Type* targetType, const std::string& vname = "");
		Value* IntSizeCast(Value* v, Type* targetType, const std::string& vname = "");
		Value* FloatToIntCast(Value* v, Type* targetType, const std::string& vname = "");
		Value* IntToFloatCast(Value* v, Type* targetType, const std::string& vname = "");
		Value* PointerTypeCast(Value* v, Type* targetType, const std::string& vname = "");
		Value* PointerToIntCast(Value* v, Type* targetType, const std::string& vname = "");
		Value* IntToPointerCast(Value* v, Type* targetType, const std::string& vname = "");
		Value* IntSignednessCast(Value* v, Type* targetType, const std::string& vname = "");

		Value* AppropriateCast(Value* v, Type* targetType, const std::string& vname = "");

		Value* IntTruncate(Value* v, Type* targetType, const std::string& vname = "");
		Value* IntZeroExt(Value* v, Type* targetType, const std::string& vname = "");

		Value* FTruncate(Value* v, Type* targetType, const std::string& vname = "");
		Value* FExtend(Value* v, Type* targetType, const std::string& vname = "");

		Value* Call(Function* fn, const std::string& vname = "");
		Value* Call(Function* fn, Value* p1, const std::string& vname = "");
		Value* Call(Function* fn, Value* p1, Value* p2, const std::string& vname = "");
		Value* Call(Function* fn, Value* p1, Value* p2, Value* p3, const std::string& vname = "");
		Value* Call(Function* fn, Value* p1, Value* p2, Value* p3, Value* p4, const std::string& vname = "");

		Value* Call(Function* fn, const std::vector<Value*>& args, const std::string& vname = "");
		Value* Call(Function* fn, const std::initializer_list<Value*>& args, const std::string& vname = "");

		Value* CallToFunctionPointer(Value* fn, FunctionType* ft, const std::vector<Value*>& args, const std::string& vname = "");

		Value* CallVirtualMethod(ClassType* cls, FunctionType* ft, size_t index, const std::vector<Value*>& args, const std::string& vname = "");

		Value* Return(Value* v);
		Value* ReturnVoid();

		Value* LogicalNot(Value* v, const std::string& vname = "");

		PHINode* CreatePHINode(Type* type, const std::string& vname = "");
		Value* StackAlloc(Type* type, const std::string& vname = "");
		Value* ImmutStackAlloc(Type* type, Value* initialValue, const std::string& vname = "");

		// given an l or cl value of raw_union type, return an l or cl value of the correct type of the field
		Value* GetRawUnionField(Value* lval, const std::string& field, const std::string& vname = "");

		// same as the above, but give it a type instead -- this is hacky cos it's not checked.
		// the backend will just do some pointer magic regardless, so it doesn't really matter.
		Value* GetRawUnionFieldByType(Value* lval, Type* type, const std::string& vname = "");

		// equivalent to GEP(ptr*, 0, memberIndex)
		Value* GetStructMember(Value* ptr, const std::string& memberName);
		Value* StructGEP(Value* structPtr, size_t memberIndex, const std::string& vname = "");

		// equivalent to GEP(ptr*, index)
		Value* GetPointer(Value* ptr, Value* ptrIndex, const std::string& vname = "");

		// equivalent to GEP(ptr*, ptrIndex, elmIndex)
		Value* GEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, const std::string& vname = "");
		Value* ConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, const std::string& vname = "");

		void SetVtable(Value* ptr, Value* table);

		void CondBranch(Value* condition, IRBlock* trueBlock, IRBlock* falseBlock);
		void UnCondBranch(IRBlock* target);


		Value* Sizeof(Type* t, const std::string& vname = "");

		Value* Select(Value* cond, Value* one, Value* two, const std::string& vname = "");

		Value* BinaryOp(const std::string& ao, Value* a, Value* b, const std::string& vname = "");

		Value* CreateValue(Type* t, const std::string& vname = "");

		Value* ExtractValue(Value* val, const std::vector<size_t>& inds, const std::string& vname = "");
		Value* ExtractValueByName(Value* val, const std::string& mem, const std::string& vname = "");

		[[nodiscard]] Value* InsertValueByName(Value* val, const std::string& mem, Value* elm, const std::string& vname = "");
		[[nodiscard]] Value* InsertValue(Value* val, const std::vector<size_t>& inds, Value* elm, const std::string& vname = "");


		Value* GetArraySliceData(Value* arr, const std::string& vname = "");
		Value* GetArraySliceLength(Value* arr, const std::string& vname = "");

		[[nodiscard]] Value* SetArraySliceData(Value* arr, Value* val, const std::string& vname = "");
		[[nodiscard]] Value* SetArraySliceLength(Value* arr, Value* val, const std::string& vname = "");

		Value* GetRangeLower(Value* range, const std::string& vname = "");
		Value* GetRangeUpper(Value* range, const std::string& vname = "");
		Value* GetRangeStep(Value* range, const std::string& vname = "");

		[[nodiscard]] Value* SetRangeLower(Value* range, Value* val, const std::string& vname = "");
		[[nodiscard]] Value* SetRangeUpper(Value* range, Value* val, const std::string& vname = "");
		[[nodiscard]] Value* SetRangeStep(Value* range, Value* step, const std::string& vname = "");

		Value* GetEnumCaseIndex(Value* ecs, const std::string& vname = "");
		Value* GetEnumCaseValue(Value* ecs, const std::string& vname = "");

		[[nodiscard]] Value* SetEnumCaseIndex(Value* ecs, Value* idx, const std::string& vname = "");
		[[nodiscard]] Value* SetEnumCaseValue(Value* ecs, Value* val, const std::string& vname = "");



		[[nodiscard]] Value* SetUnionVariantData(Value* unn, size_t id, Value* data, const std::string& vname = "");
		[[nodiscard]] Value* GetUnionVariantData(Value* unn, size_t id, const std::string& vname = "");

		[[nodiscard]] Value* GetUnionVariantID(Value* unn, const std::string& vname = "");
		[[nodiscard]] Value* SetUnionVariantID(Value* unn, size_t id, const std::string& vname = "");



		//* V2 api

		Value* ReadPtr(Value* ptr, const std::string& vname = "");
		void WritePtr(Value* v, Value* ptr);

		void Store(Value* val, Value* lval);
		Value* CreateLValue(Type* t, const std::string& vname = "");

		Value* Dereference(Value* val, const std::string& vname = "");
		Value* AddressOf(Value* lval, bool mut, const std::string& vname = "");






		void Unreachable();


		IRBlock* addNewBlockInFunction(const std::string& name, Function* func);
		IRBlock* addNewBlockAfter(const std::string& name, IRBlock* block);


		void setCurrentBlock(IRBlock* block);
		void restorePreviousBlock();

		Function* getCurrentFunction();
		IRBlock* getCurrentBlock();


		private:
		Value* addInstruction(Instruction* instr, const std::string& vname);

		Module* module = 0;
		Function* currentFunction = 0;
		IRBlock* currentBlock = 0;
		IRBlock* previousBlock = 0;
	};
}




















































