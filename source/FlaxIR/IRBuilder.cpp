// IRBuilder.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/ir/block.h"
#include "../include/ir/irbuilder.h"
#include "../include/ir/instruction.h"

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

	Instruction* Instruction::GetBinaryOpInstruction(Ast::ArithmeticOp ao, Value* vlhs, Value* vrhs)
	{
		OpKind op = OpKind::Invalid;

		Type* lhs = vlhs->getType();
		Type* rhs = vrhs->getType();

		bool useFloating = (lhs->isFloatingPointType() || rhs->isFloatingPointType());
		bool useSigned = ((lhs->isIntegerType() && lhs->toPrimitiveType()->isSigned())
			|| (rhs->isIntegerType() && rhs->toPrimitiveType()->isSigned()));


		PrimitiveType* lpt = lhs->toPrimitiveType();
		PrimitiveType* rpt = rhs->toPrimitiveType();

		iceAssert(lpt && rpt && "not primitive types");

		Type* out = 0;
		if(ao == Ast::ArithmeticOp::Add)
		{
			op = useFloating ? OpKind::Floating_Add : useSigned ? OpKind::Signed_Add : OpKind::Unsigned_Add;

			// use the larger type.
			if(useFloating)
			{
				if(lpt->getFloatingPointBitWidth() > rpt->getFloatingPointBitWidth())
					out = lpt;

				else
					out = rpt;
			}
			else
			{
				// following c/c++ conventions, signed types are converted to unsigned types in mixed ops.
				if(lpt->getIntegerBitWidth() > rpt->getIntegerBitWidth())
				{
					if(lpt->isSigned() && rpt->isSigned()) out = lpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned()) out = rpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
			}
		}
		else if(ao == Ast::ArithmeticOp::Subtract)
		{
			op = useFloating ? OpKind::Floating_Sub : useSigned ? OpKind::Signed_Sub : OpKind::Unsigned_Sub;

			// use the larger type.
			if(useFloating)
			{
				if(lpt->getFloatingPointBitWidth() > rpt->getFloatingPointBitWidth())
					out = lpt;

				else
					out = rpt;
			}
			else
			{
				// following c/c++ conventions, signed types are converted to unsigned types in mixed ops.
				if(lpt->getIntegerBitWidth() > rpt->getIntegerBitWidth())
				{
					if(lpt->isSigned() && rpt->isSigned()) out = lpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned()) out = rpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
			}
		}
		else if(ao == Ast::ArithmeticOp::Multiply)
		{
			op = useFloating ? OpKind::Floating_Mul : useSigned ? OpKind::Signed_Mul : OpKind::Unsigned_Mul;

			// use the larger type.
			if(useFloating)
			{
				if(lpt->getFloatingPointBitWidth() > rpt->getFloatingPointBitWidth())
					out = lpt;

				else
					out = rpt;
			}
			else
			{
				// following c/c++ conventions, signed types are converted to unsigned types in mixed ops.
				if(lpt->getIntegerBitWidth() > rpt->getIntegerBitWidth())
				{
					if(lpt->isSigned() && rpt->isSigned()) out = lpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned()) out = rpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
			}
		}
		else if(ao == Ast::ArithmeticOp::Divide)
		{
			op = useFloating ? OpKind::Floating_Div : useSigned ? OpKind::Signed_Div : OpKind::Unsigned_Div;

			// use the larger type.
			if(useFloating)
			{
				if(lpt->getFloatingPointBitWidth() > rpt->getFloatingPointBitWidth())
					out = lpt;

				else
					out = rpt;
			}
			else
			{
				// following c/c++ conventions, signed types are converted to unsigned types in mixed ops.
				if(lpt->getIntegerBitWidth() > rpt->getIntegerBitWidth())
				{
					if(lpt->isSigned() && rpt->isSigned()) out = lpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned()) out = rpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
			}
		}
		else if(ao == Ast::ArithmeticOp::Modulo)
		{
			op = useFloating ? OpKind::Floating_Mod : useSigned ? OpKind::Signed_Mod : OpKind::Unsigned_Mod;

			// use the larger type.
			if(useFloating)
			{
				if(lpt->getFloatingPointBitWidth() > rpt->getFloatingPointBitWidth())
					out = lpt;

				else
					out = rpt;
			}
			else
			{
				// following c/c++ conventions, signed types are converted to unsigned types in mixed ops.
				if(lpt->getIntegerBitWidth() > rpt->getIntegerBitWidth())
				{
					if(lpt->isSigned() && rpt->isSigned()) out = lpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned()) out = rpt;
					out = (lpt->isSigned() ? rpt : lpt);
				}
			}
		}
		else if(ao == Ast::ArithmeticOp::ShiftLeft)
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = OpKind::Bitwise_Shl;
		}
		else if(ao == Ast::ArithmeticOp::ShiftRight)
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = useSigned ? OpKind::Bitwise_Arithmetic_Shr : OpKind::Bitwise_Logical_Shr;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseAnd)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_And;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseOr)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Or;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseXor)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Xor;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseNot)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Not;
		}
		else
		{
			return 0;
		}

		return new Instruction(op, out, { vlhs, vrhs });
	}

	Value* IRBuilder::CreateBinaryOp(Ast::ArithmeticOp ao, Value* a, Value* b)
	{
		Instruction* instr = Instruction::GetBinaryOpInstruction(ao, a, b);
		return this->addInstruction(instr);
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

	Value* IRBuilder::CreateFTruncate(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Truncate, targetType, { v });
		return instr;
	}

	Value* IRBuilder::CreateFExtend(Value* v, Type* targetType)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Extend, targetType, { v });
		return instr;
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

	Value* IRBuilder::CreateBitwiseLogicalSHR(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise lshl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Logical_Shr, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseArithmeticSHR(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise ashl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Arithmetic_Shr, a->getType(), { a, b });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateBitwiseSHL(Value* a, Value* b)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise shr instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Shl, a->getType(), { a, b });
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

	Value* IRBuilder::CreateBitwiseNOT(Value* a)
	{
		Instruction* instr = new Instruction(OpKind::Bitwise_Not, a->getType(), { a });
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


	Value* IRBuilder::CreateReturn(Value* v)
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, v->getType(), { v });
		return this->addInstruction(instr);
	}

	Value* IRBuilder::CreateReturnVoid()
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, PrimitiveType::getVoid(), { });
		return this->addInstruction(instr);
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
		error("enotsup");
	}

	Value* IRBuilder::CreateGetPointerToConstStructMember(Value* ptr, Value* ptrIndex, size_t memberIndex)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not a pointer");
		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not an integer type");

		StructType* st = dynamic_cast<StructType*>(ptr->getType()->getPointerElementType());
		iceAssert(st && "ptr is not pointer to struct");
		iceAssert(st->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = new Instruction(OpKind::Value_GetPointerToStructMember,
			st->getElementN(memberIndex), { ptr, ptrIndex, ConstantInt::getUnsigned(PrimitiveType::getUint64(), memberIndex) });

		return this->addInstruction(instr);
	}


	// equivalent to GEP(ptr*, 0, memberIndex)
	Value* IRBuilder::CreateGetStructMember(Value* structPtr, Value* memberIndex)
	{
		error("enotsup");
	}

	Value* IRBuilder::CreateGetConstStructMember(Value* structPtr, size_t memberIndex)
	{
		iceAssert(structPtr->getType()->isPointerType() && "ptr is not a pointer");

		StructType* st = dynamic_cast<StructType*>(structPtr->getType()->getPointerElementType());
		iceAssert(st && "ptr is not pointer to struct");
		iceAssert(st->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = new Instruction(OpKind::Value_GetStructMember,
			st->getElementN(memberIndex), { structPtr, ConstantInt::getUnsigned(PrimitiveType::getUint64(), memberIndex) });

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

	void IRBuilder::CreateCondBranch(Value* condition, IRBlock* trueB, IRBlock* falseB)
	{
		Instruction* instr = new Instruction(OpKind::Branch_Cond, PrimitiveType::getVoid(), { condition, trueB, falseB });
		this->addInstruction(instr);
	}

	void IRBuilder::CreateUnCondBranch(IRBlock* target)
	{
		Instruction* instr = new Instruction(OpKind::Branch_UnCond, PrimitiveType::getVoid(), { target });
		this->addInstruction(instr);
	}

	IRBlock* IRBuilder::addNewBlockInFunction(std::string name, Function* func)
	{
		IRBlock* block = new IRBlock(func);
		if(func != this->currentFunction)
		{
			warn("changing current function in irbuilder");
			this->currentFunction = block->parentFunction;
		}


		this->currentFunction->blocks.push_back(block);
		block->setName(name);
		return block;
	}

	IRBlock* IRBuilder::addNewBlockAfter(std::string name, IRBlock* block)
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
		nb->setName(name);
		return nb;
	}
}




