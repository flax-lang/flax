// IRBuilder.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/block.h"
#include "../include/ir/irbuilder.h"

namespace fir
{
	IRBuilder::IRBuilder(FTContext* c)
	{
		this->context = c;

		this->currentBlock = 0;
		this->previousBlock = 0;
		this->currentFunction = 0;
	}

	void IRBuilder::setCurrentBlock(IRBlock* block)
	{
		this->previousBlock = this->currentBlock;
		this->currentBlock = block;

		if(this->currentBlock->parentFunction != 0)
			this->currentFunction = this->currentBlock->parentFunction;

		else
			this->currentFunction = 0;
	}

	void IRBuilder::restorePreviousBlock()
	{
		this->currentBlock = this->previousBlock;
	}

	Function* IRBuilder::getCurrentFunction()
	{
		return this->currentFunction;
	}

	IRBlock* IRBuilder::getCurrentBlock()
	{
		return this->currentBlock;
	}


	Value* IRBuilder::addInstruction(Instruction* instr)
	{
		iceAssert(this->currentBlock && "no current block");

		// add instruction to the end of the block
		this->currentBlock->instructions.push_back(instr);
		Value* v = new Value(instr->getType());

		v->addUser(this->currentBlock);
		return v;
	}




























	Value* IRBuilder::CreateAdd(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating add instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Signed_Add, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateSub(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating sub instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Signed_Sub, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateMul(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating mul instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Signed_Mul, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateDiv(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating div instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Signed_Div, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateMod(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating mod instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Signed_Mod, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateICmpEQ(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Equal, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateICmpNEQ(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_NotEqual, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateICmpGT(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Greater, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateICmpLT(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Less, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateICmpGEQ(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::IComapre_GreaterEqual, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateICmpLEQ(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_LessEqual, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpEQ_ORD(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_ord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_ORD, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpEQ_UNORD(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_uord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_UNORD, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpNEQ_ORD(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_ord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_ORD, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpNEQ_UNORD(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_uord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_UNORD, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpGT(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpLT(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpGEQ(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FComapre_GreaterEqual, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFCmpLEQ(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateLogicalAND(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating logical and instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Logical_And, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateLogicalOR(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating logical or instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Logical_Or, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseXOR(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise xor instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Xor, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseLogicalSHL(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise lshl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Logical_Shl, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseArithmeticSHL(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise ashl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Arithmetic_Shl, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseSHR(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise shr instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Shr, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseAND(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise and instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_And, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseOR(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise or instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Or, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitcast(Value* v, Type* targetType)
	{
		Instruction* instr = new Instruction(OpKind::Cast_Bitcast, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateIntSizeCast(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntSize, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateFloatToIntCast(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isFloatingPointType() && "value is not floating point type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_FloatToInt, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateIntToFloatCast(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isFloatingPointType() && "target is not floating point type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToFloat, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreatePointerTypeCast(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerType, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreatePointerToIntCast(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerToInt, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateIntToPointerCast(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToPointer, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr);
	}


	Value* IRBuilder::CreateLoad(Value* ptr)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not pointer type");

		Instruction* instr = new Instruction(OpKind::Value_Load, ptr->getType()->getPointerElementType(), { ptr });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateStore(Value* v, Value* ptr)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not pointer type");
		iceAssert(v->getType()->getPointerTo() == v->getType() && "ptr is not a pointer to type of value");

		Instruction* instr = new Instruction(OpKind::Value_Store, PrimitiveType::getVoid(), { v, ptr });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateCall0(Function* fn)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateCall1(Function* fn, Value* p1)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { p1 });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateCall2(Function* fn, Value* p1, Value* p2)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { p1, p2 });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateCall3(Function* fn, Value* p1, Value* p2, Value* p3)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { p1, p2, p3 });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::deque<Value*> args)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), args);
		return this->addInstruction(instr);
	}


	void IRBuilder::CreateReturn(Value* v)
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, v->getType(), { v });
		this->addInstruction(instr);
	}

	void IRBuilder::CreateReturnVoid()
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, PrimitiveType::getVoid(), { });
		this->addInstruction(instr);
	}


	Value* IRBuilder::CreateLogicalNot(Value* v)
	{
		Instruction* instr = new Instruction(OpKind::Logical_Not, PrimitiveType::getBool(), { v });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateStackAlloc(Type* type)
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, type->getPointerTo(), { ConstantValue::getNullValue(type) });
		return this->addInstruction(instr);
	}


	// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
	Value* IRBuilder::CreateGetPointerToStructMember(Value* ptr, Value* ptrIndex, Value* memberIndex)
	{
		error("enosup");
	}

	Value* IRBuilder::CreateGetPointerToConstStructMember(Value* ptr, Value* ptrIndex, size_t memberIndex)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not a pointer");
		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not an integer type");

		StructType* st = dynamic_cast<StructType*>(ptr->getType()->getPointerElementType());
		iceAssert(st && "ptr is not pointer to struct");
		iceAssert(st->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = new Instruction(OpKind::Value_GetPointerToStructMember,
			st->getElementN(memberIndex), { ptr, ptrIndex, ConstantInt::getConstantUIntValue(PrimitiveType::getUint64(), memberIndex) });

		return this->addInstruction(instr);
	}


	// equivalent to GEP(ptr*, 0, memberIndex)
	Value* IRBuilder::CreateGetStructMember(Value* structPtr, Value* memberIndex)
	{
		error("enosup");
	}

	Value* IRBuilder::CreateGetConstStructMember(Value* structPtr, size_t memberIndex)
	{
		iceAssert(structPtr->getType()->isPointerType() && "ptr is not a pointer");

		StructType* st = dynamic_cast<StructType*>(structPtr->getType()->getPointerElementType());
		iceAssert(st && "ptr is not pointer to struct");
		iceAssert(st->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = new Instruction(OpKind::Value_GetStructMember,
			st->getElementN(memberIndex), { structPtr, ConstantInt::getConstantUIntValue(PrimitiveType::getUint64(), memberIndex) });

		return this->addInstruction(instr);
	}


	// equivalent to GEP(ptr*, index)
	Value* IRBuilder::CreateGetPointer(Value* ptr, Value* ptrIndex)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not a pointer type");
		iceAssert(ptrIndex->getType()->isPointerType() && "ptrIndex is not an integer type");
		Instruction* instr = new Instruction(OpKind::Value_GetPointer, ptr->getType()->getPointerElementType(), { ptr, ptrIndex });
		return this->addInstruction(instr);
	}

	void IRBuilder::CreateCondBranch(IRBlock* target, Value* condition)
	{
		Instruction* instr = new Instruction(OpKind::Branch_Cond, PrimitiveType::getVoid(), { target, condition });
		this->addInstruction(instr);
	}

	void IRBuilder::CreateUnCondBranch(IRBlock* target)
	{
		Instruction* instr = new Instruction(OpKind::Branch_UnCond, PrimitiveType::getVoid(), { target });
		this->addInstruction(instr);
	}

	IRBlock* IRBuilder::addNewBlockInFunction(Function* func)
	{
		IRBlock* block = new IRBlock(func);
		if(func != this->currentFunction)
		{
			warn("changing current function in irbuilder");
			this->currentFunction = block->parentFunction;
		}

		this->currentFunction->blocks.push_back(block);
		return block;
	}

	IRBlock* IRBuilder::addNewBlockAfter(IRBlock* block)
	{
		IRBlock* nb = new IRBlock(block->parentFunction);
		if(nb->parentFunction != this->currentFunction)
		{
			warn("changing current function in irbuilder");
			this->currentFunction = nb->parentFunction;
		}

		for(size_t i = 0; i < this->currentFunction->blocks.size(); i++)
		{
			IRBlock* b = this->currentFunction->blocks[i];
			if(b == nb)
			{
				this->currentFunction->blocks.insert(this->currentFunction->blocks.begin() + i + 1, nb);
				return nb;
			}
		}

		iceAssert(0 && "no such block to insert after");
		return nb;
	}
}




