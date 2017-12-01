// IRBuilder.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cmath>

#include "ast.h"
#include "ir/block.h"
#include "ir/irbuilder.h"
#include "ir/instruction.h"

#define DO_IN_SITU_CONSTANT_FOLDING		0


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

		if(this->currentBlock != 0)
		{
			if(this->currentBlock->parentFunction != 0)
				this->currentFunction = this->currentBlock->parentFunction;
			else
				this->currentFunction = 0;
		}
		else
		{
			this->currentFunction = 0;
		}
	}

	void IRBuilder::restorePreviousBlock()
	{
		this->currentBlock = this->previousBlock;
	}

	Function* IRBuilder::getCurrentFunction()
	{
		return this->currentFunction;
		// return this->currentBlock->getParentFunction();
	}

	IRBlock* IRBuilder::getCurrentBlock()
	{
		return this->currentBlock;
	}


	Value* IRBuilder::addInstruction(Instruction* instr, std::string vname)
	{
		iceAssert(this->currentBlock && "no current block");

		// add instruction to the end of the block
		this->currentBlock->instructions.push_back(instr);
		Value* v = instr->realOutput;

		// v->addUser(this->currentBlock);
		v->setName(vname);
		return v;
	}

	static Instruction* getBinaryOpInstruction(IRBlock* parent, Operator ao, Value* vlhs, Value* vrhs)
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
		if(ao == Operator::Add)
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
		else if(ao == Operator::Subtract)
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
		else if(ao == Operator::Multiply)
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
		else if(ao == Operator::Divide)
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
		else if(ao == Operator::Modulo)
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
		else if(ao == Operator::ShiftLeft)
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = OpKind::Bitwise_Shl;

			out = lhs;
		}
		else if(ao == Operator::ShiftRight)
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = useSigned ? OpKind::Bitwise_Arithmetic_Shr : OpKind::Bitwise_Logical_Shr;

			out = lhs;
		}
		else if(ao == Operator::BitwiseAnd)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_And;

			out = lhs;
		}
		else if(ao == Operator::BitwiseOr)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Or;

			out = lhs;
		}
		else if(ao == Operator::BitwiseXor)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Xor;

			out = lhs;
		}
		else if(ao == Operator::BitwiseNot)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Not;

			out = lhs;
		}
		else
		{
			return 0;
		}

		return new Instruction(op, false, parent, out, { vlhs, vrhs });
	}

	Value* IRBuilder::CreateBinaryOp(Operator ao, Value* a, Value* b, std::string vname)
	{
		Instruction* instr = getBinaryOpInstruction(this->currentBlock, ao, a, b);
		if(instr == 0) return 0;

		return this->addInstruction(instr, vname);
	}































	Value* IRBuilder::CreateNeg(Value* a, std::string vname)
	{
		iceAssert(a->getType()->toPrimitiveType() && "cannot negate non-primitive type");
		iceAssert((a->getType()->isFloatingPointType() || a->getType()->toPrimitiveType()->isSigned()) && "cannot negate unsigned type");

		Instruction* instr = new Instruction(a->getType()->isFloatingPointType() ? OpKind::Floating_Neg : OpKind::Signed_Neg,
			false, this->currentBlock, a->getType(), { a });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateAdd(Value* a, Value* b, std::string vname)
	{
		if(a->getType() != b->getType())
			error("creating add instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Add;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Add;
		else ok = OpKind::Floating_Add;


		Instruction* instr = new Instruction(ok, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSub(Value* a, Value* b, std::string vname)
	{
		if(a->getType() != b->getType())
			error("creating sub instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Sub;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Sub;
		else ok = OpKind::Floating_Sub;

		Instruction* instr = new Instruction(ok, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateMul(Value* a, Value* b, std::string vname)
	{
		if(a->getType() != b->getType())
			error("creating mul instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Mul;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Mul;
		else ok = OpKind::Floating_Mul;

		Instruction* instr = new Instruction(ok, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateDiv(Value* a, Value* b, std::string vname)
	{
		if(a->getType() != b->getType())
			error("creating div instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());


		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Div;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Div;
		else ok = OpKind::Floating_Div;

		Instruction* instr = new Instruction(ok, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateMod(Value* a, Value* b, std::string vname)
	{
		if(a->getType() != b->getType())
			error("creating mod instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Mod;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Mod;
		else ok = OpKind::Floating_Mod;

		Instruction* instr = new Instruction(ok, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFTruncate(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Truncate, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFExtend(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Extend, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::CreateICmpEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp eq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Equal, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpNEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp neq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_NotEqual, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpGT(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp gt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Greater, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpLT(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp lt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Less, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpGEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp geq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_GreaterEqual, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpLEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp leq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_LessEqual, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::CreateFCmpEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_ord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_ORD, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_uord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpNEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_ord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpNEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_uord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGT_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGT_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLT_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLT_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_GreaterEqual_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_GreaterEqual_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}


	// returns -1 for a < b, 0 for a == b, 1 for a > b
	Value* IRBuilder::CreateICmpMulti(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp multi instruction with non-equal types");
		// iceAssert(a->getType()->isIntegerType() && "creating icmp multi instruction with non-integer type");
		Instruction* instr = new Instruction(OpKind::ICompare_Multi, false, this->currentBlock,
			fir::Type::getInt64(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpMulti(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Multi, false, this->currentBlock,
			fir::Type::getInt64(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}










	Value* IRBuilder::CreateBitwiseXOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise xor instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Xor, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseLogicalSHR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise lshl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Logical_Shr, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseArithmeticSHR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise ashl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Arithmetic_Shr, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseSHL(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise shr instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Shl, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseAND(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise and instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_And, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise or instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Or, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseNOT(Value* a, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Bitwise_Not, false, this->currentBlock, a->getType(), { a });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitcast(Value* v, Type* targetType, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Cast_Bitcast, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntSizeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert((v->getType()->isIntegerType() || v->getType()->isBoolType()) && "value is not integer type");
		iceAssert((targetType->isIntegerType() || targetType->isBoolType()) && "target is not integer type");

		// make constant result for constant operand
		if(ConstantInt* ci = dynamic_cast<ConstantInt*>(v))
		{
			return ConstantInt::get(targetType, ci->getSignedValue());
		}

		Instruction* instr = new Instruction(OpKind::Cast_IntSize, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntSignednessCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		// make constant result for constant operand
		if(ConstantInt* ci = dynamic_cast<ConstantInt*>(v))
		{
			if(ci->getType()->isSignedIntType())
				return ConstantInt::get(targetType, ci->getSignedValue());

			else
				return ConstantInt::get(targetType, ci->getUnsignedValue());
		}

		Instruction* instr = new Instruction(OpKind::Cast_IntSignedness, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFloatToIntCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && "value is not floating point type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		// make constant result for constant operand
		if(ConstantFP* cfp = dynamic_cast<ConstantFP*>(v))
		{
			double _ = 0;

			if(std::modf(cfp->getValue(), &_) != 0.0)
				warn("Truncating constant '%Lf' in constant cast to type '%s'", cfp->getValue(), targetType);

			return ConstantInt::get(targetType, (size_t) cfp->getValue());
		}

		Instruction* instr = new Instruction(OpKind::Cast_FloatToInt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntToFloatCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isFloatingPointType() && "target is not floating point type");

		// make constant result for constant operand
		if(ConstantInt* ci = dynamic_cast<ConstantInt*>(v))
		{
			ConstantFP* ret = 0;
			bool sgn = ci->getType()->isSignedIntType();
			if(targetType == fir::Type::getFloat32())
			{
				if(sgn)	ret = ConstantFP::getFloat32((float) ci->getSignedValue());
				else	ret = ConstantFP::getFloat32((float) ci->getUnsignedValue());
			}
			else if(targetType == fir::Type::getFloat64())
			{
				if(sgn)	ret = ConstantFP::getFloat64((double) ci->getSignedValue());
				else	ret = ConstantFP::getFloat64((double) ci->getUnsignedValue());
			}
			else if(targetType == fir::Type::getFloat80())
			{
				if(sgn)	ret = ConstantFP::getFloat80((long double) ci->getSignedValue());
				else	ret = ConstantFP::getFloat80((long double) ci->getUnsignedValue());
			}
			else
			{
				error("Unknown floating point type '%s'", targetType);
			}

			return ret;
		}

		Instruction* instr = new Instruction(OpKind::Cast_IntToFloat, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerTypeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerType, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerToIntCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerToInt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntToPointerCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToPointer, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateIntTruncate(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Integer_Truncate, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntZeroExt(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Integer_ZeroExt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::CreateAppropriateCast(Value* v, Type* r, std::string vname)
	{
		auto l = v->getType();

		if(l->isIntegerType() && r->isIntegerType())
			return this->CreateIntSizeCast(v, r);

		else if(l->isFloatingPointType() && r->isFloatingPointType())
			return (l->getBitWidth() > r->getBitWidth() ? this->CreateFTruncate(v, r) : this->CreateFExtend(v, r));

		else if(l->isIntegerType() && r->isFloatingPointType())
			return this->CreateIntToFloatCast(v, r);

		else if(l->isFloatingPointType() && r->isIntegerType())
			return this->CreateFloatToIntCast(v, r);

		else if(l->isIntegerType() && r->isPointerType())
			return this->CreateIntToPointerCast(v, r);

		else if(l->isPointerType() && r->isIntegerType())
			return this->CreatePointerToIntCast(v, r);

		else if(l->isPointerType() && r->isPointerType())
			return this->CreatePointerTypeCast(v, r);

		// nope.
		return 0;
	}










	Value* IRBuilder::CreateLoad(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got '%s')", ptr->getType());

		Instruction* instr = new Instruction(OpKind::Value_Load, false, this->currentBlock, ptr->getType()->getPointerElementType(), { ptr });
		auto ret = this->addInstruction(instr, vname);
		if(ptr->isImmutable()) ret->makeImmutable();

		return ret;
	}

	Value* IRBuilder::CreateStore(Value* v, Value* ptr)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got '%s')", ptr->getType());

		if(v->getType()->getPointerTo() != ptr->getType())
			error("ptr is not a pointer to type of value (storing '%s' into '%s')", v->getType(), ptr->getType());

		if(ptr->isImmutable())
			error("Cannot store value to immutable alloc (id: %zu)", ptr->id);

		Instruction* instr = new Instruction(OpKind::Value_Store, true, this->currentBlock, Type::getVoid(), { v, ptr });
		return this->addInstruction(instr, "");
	}


	Value* IRBuilder::CreateCall0(Function* fn, std::string vname)
	{
		return this->CreateCall(fn, { }, vname);
	}

	Value* IRBuilder::CreateCall1(Function* fn, Value* p1, std::string vname)
	{
		return this->CreateCall(fn, { p1 }, vname);
	}

	Value* IRBuilder::CreateCall2(Function* fn, Value* p1, Value* p2, std::string vname)
	{
		return this->CreateCall(fn, { p1, p2 }, vname);
	}

	Value* IRBuilder::CreateCall3(Function* fn, Value* p1, Value* p2, Value* p3, std::string vname)
	{
		return this->CreateCall(fn, { p1, p2, p3 }, vname);
	}

	#if 0
	fir::ConstantInt* ci = 0;
	if(dist == -1 || from->getType()->toPrimitiveType()->isLiteralType())
	{
		if((ci = dynamic_cast<fir::ConstantInt*>(from)))
		{
			if(ci->getType()->isSignedIntType())
			{
				shouldCast = fir::checkSignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getSignedValue());
			}
			else
			{
				shouldCast = fir::checkUnsignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getUnsignedValue());
			}
		}
	}

	if(shouldCast)
	{
		// if it is a literal, we need to create a new constant with a proper type
		if(from->getType()->toPrimitiveType()->isLiteralType())
		{
			if(ci)
			{
				fir::PrimitiveType* real = 0;
				if(ci->getType()->isSignedIntType())
					real = fir::PrimitiveType::getIntN(target->toPrimitiveType()->getIntegerBitWidth());

				else
					real = fir::PrimitiveType::getUintN(target->toPrimitiveType()->getIntegerBitWidth());

				from = fir::ConstantInt::get(real, ci->getSignedValue());
			}
			else
			{
				// nothing?
				from->setType(from->getType()->toPrimitiveType()->getUnliteralType());
			}

			retval = from;
		}

		// check signed to unsigned first
		if(target->toPrimitiveType()->isSigned() != from->getType()->toPrimitiveType()->isSigned())
		{
			from = this->irb.CreateIntSignednessCast(from, from->getType()->toPrimitiveType()->getOppositeSignedType());
		}

		retval = this->irb.CreateIntSizeCast(from, target);
	}
	#endif


	Value* IRBuilder::CreateCall(Function* fn, std::vector<Value*> args, std::string vname)
	{
		// in theory we should still check, but i'm lazy right now
		// TODO.
		if(!fn->isCStyleVarArg())
		{
			// check here, to stop llvm dying
			if(args.size() != fn->getArgumentCount())
				error("Calling function '%s' with the wrong number of arguments (needs %zu, have %zu)", fn->getName().str(),
					fn->getArgumentCount(), args.size());

			for(size_t i = 0; i < args.size(); i++)
			{
				auto target = fn->getArguments()[i]->getType();

				// special case
				if(args[i]->getType()->isDynamicArrayType() && target->isDynamicArrayType() &&
					target->toDynamicArrayType()->isFunctionVariadic() != args[i]->getType()->toDynamicArrayType()->isFunctionVariadic())
				{
					// silently cast, because they're the same thing
					// the distinction is solely for the type system's benefit
					args[i] = this->CreateBitcast(args[i], target);
				}

				if(args[i]->getType() != target)
				{
					error("Mismatch in argument type (arg. %zu) in function '%s' (need '%s', have '%s')", i, fn->getName().str(),
						fn->getArguments()[i]->getType(), args[i]->getType());
				}
			}
		}

		args.insert(args.begin(), fn);

		Instruction* instr = new Instruction(OpKind::Value_CallFunction, true, this->currentBlock, fn->getType()->getReturnType(), args);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::initializer_list<Value*> args, std::string vname)
	{
		return this->CreateCall(fn, std::vector<Value*>(args.begin(), args.end()), vname);
	}





	Value* IRBuilder::CreateCallToFunctionPointer(Value* fn, FunctionType* ft, std::vector<Value*> args, std::string vname)
	{
		// we can't really check anything.
		args.insert(args.begin(), fn);

		Instruction* instr = new Instruction(OpKind::Value_CallFunctionPointer, true, this->currentBlock, ft->getReturnType(), args);
		return this->addInstruction(instr, vname);
	}






















	Value* IRBuilder::CreateReturn(Value* v)
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, true, this->currentBlock, Type::getVoid(), { v });
		return this->addInstruction(instr, "");
	}

	Value* IRBuilder::CreateReturnVoid()
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, true, this->currentBlock, Type::getVoid(), { });
		return this->addInstruction(instr, "");
	}


	Value* IRBuilder::CreateLogicalNot(Value* v, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Logical_Not, false, this->currentBlock, Type::getBool(), { v });
		return this->addInstruction(instr, vname);
	}


	PHINode* IRBuilder::CreatePHINode(Type* type, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_CreatePHI, false, this->currentBlock, type->getPointerTo(),
			{ ConstantValue::getZeroValue(type) });

		// we need to 'lift' the allocation up to make it the first in the block
		// this is an llvm requirement.

		delete instr->realOutput;

		instr->realOutput = new PHINode(type);
		fir::Value* ret = instr->realOutput;

		ret->setName(vname);

		// insert at the front (back = no guarantees)
		this->currentBlock->instructions.insert(this->currentBlock->instructions.begin(), instr);
		return (PHINode*) instr->realOutput;
	}

	Value* IRBuilder::CreateStackAlloc(Type* type, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_StackAlloc, false, this->currentBlock, type->getPointerTo(),
			{ ConstantValue::getZeroValue(type) });

		// we need to 'lift' the allocation up to the entry block of the function
		// this prevents allocation inside loops eating stack memory forever

		fir::Value* ret = instr->realOutput;
		ret->setName(vname);

		// get the parent function
		auto parent = this->currentBlock->getParentFunction();
		iceAssert(parent);

		// get the entry block
		auto entry = parent->getBlockList().front();
		iceAssert(entry);

		// insert at the front (back = no guarantees)
		entry->instructions.insert(entry->instructions.begin(), instr);

		return ret;
	}

	Value* IRBuilder::CreateImmutStackAlloc(Type* type, Value* v, std::string vname)
	{
		Value* ret = this->CreateStackAlloc(type, vname);
		ret->immut = false;		// for now

		// same lifting shit as above

		this->CreateStore(v, ret);
		ret->immut = true;

		return ret;
	}



	void IRBuilder::CreateCondBranch(Value* condition, IRBlock* trueB, IRBlock* falseB)
	{
		Instruction* instr = new Instruction(OpKind::Branch_Cond, true, this->currentBlock, Type::getVoid(),
			{ condition, trueB, falseB });
		this->addInstruction(instr, "");
	}

	void IRBuilder::CreateUnCondBranch(IRBlock* target)
	{
		Instruction* instr = new Instruction(OpKind::Branch_UnCond, true, this->currentBlock, Type::getVoid(),
			{ target });
		this->addInstruction(instr, "");
	}




	// gep stuff


	// structs and tuples have the same member names.
	template <typename T>
	static Instruction* doGEPOnCompoundType(IRBlock* parent, T* type, Value* ptr, Value* ptrIndex, size_t memberIndex)
	{
		iceAssert(type->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = new Instruction(OpKind::Value_GetPointerToStructMember, false, parent,
			type->getElementN(memberIndex)->getPointerTo(), { ptr, ptrIndex, ConstantInt::getUint64(memberIndex) });

		// disallow storing to members of immut structs
		if(ptr->isImmutable())
			instr->realOutput->makeImmutable();

		return instr;
	}

	template <typename T>
	static Instruction* doGEPOnCompoundType(IRBlock* parent, T* type, Value* structPtr, size_t memberIndex)
	{
		iceAssert(type->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, parent,
			type->getElementN(memberIndex)->getPointerTo(), { structPtr, ConstantInt::getUint64(memberIndex) });

		// disallow storing to members of immut structs
		if(structPtr->isImmutable())
			instr->realOutput->makeImmutable();

		return instr;
	}


	Value* IRBuilder::CreateGetPointerToConstStructMember(Value* ptr, Value* ptrIndex, size_t memberIndex, std::string vname)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not a pointer");
		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not an integer type");

		// todo: consolidate
		if(StructType* st = dynamic_cast<StructType*>(ptr->getType()->getPointerElementType()))
		{
			return this->addInstruction(doGEPOnCompoundType(this->currentBlock, st, ptr, ptrIndex, memberIndex), vname);
		}
		else if(ClassType* st = dynamic_cast<ClassType*>(ptr->getType()->getPointerElementType()))
		{
			return this->addInstruction(doGEPOnCompoundType(this->currentBlock, st, ptr, ptrIndex, memberIndex), vname);
		}
		else if(TupleType* tt = dynamic_cast<TupleType*>(ptr->getType()->getPointerElementType()))
		{
			return this->addInstruction(doGEPOnCompoundType(this->currentBlock, tt, ptr, ptrIndex, memberIndex), vname);
		}
		else
		{
			error("type '%s' is not a valid type to GEP into", ptr->getType()->getPointerElementType());
		}
	}





	Value* IRBuilder::CreateStructGEP(Value* structPtr, size_t memberIndex, std::string vname)
	{
		iceAssert(structPtr->getType()->isPointerType() && "ptr is not a pointer");

		if(StructType* st = dynamic_cast<StructType*>(structPtr->getType()->getPointerElementType()))
		{
			return this->addInstruction(doGEPOnCompoundType(this->currentBlock, st, structPtr, memberIndex), vname);
		}
		if(ClassType* st = dynamic_cast<ClassType*>(structPtr->getType()->getPointerElementType()))
		{
			return this->addInstruction(doGEPOnCompoundType(this->currentBlock, st, structPtr, memberIndex), vname);
		}
		else if(TupleType* tt = dynamic_cast<TupleType*>(structPtr->getType()->getPointerElementType()))
		{
			return this->addInstruction(doGEPOnCompoundType(this->currentBlock, tt, structPtr, memberIndex), vname);
		}
		else
		{
			error("type '%s' is not a valid type to GEP into", structPtr->getType()->getPointerElementType());
		}
	}

	Value* IRBuilder::CreateGetStructMember(Value* structPtr, std::string memberName)
	{
		iceAssert(structPtr->getType()->isPointerType() && "ptr is not pointer");
		if(StructType* st = dynamic_cast<StructType*>(structPtr->getType()->getPointerElementType()))
		{
			iceAssert(st->hasElementWithName(memberName) && "no element with such name");

			Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, this->currentBlock,
				st->getElement(memberName)->getPointerTo(), { structPtr, ConstantInt::getUint64(st->getElementIndex(memberName)) });

			if(structPtr->isImmutable())
				instr->realOutput->immut = true;

			return this->addInstruction(instr, memberName);
		}
		else if(ClassType* ct = dynamic_cast<ClassType*>(structPtr->getType()->getPointerElementType()))
		{
			iceAssert(ct->hasElementWithName(memberName) && "no element with such name");

			Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, this->currentBlock,
				ct->getElement(memberName)->getPointerTo(), { structPtr, ConstantInt::getUint64(ct->getElementIndex(memberName)) });

			if(structPtr->isImmutable())
				instr->realOutput->immut = true;

			return this->addInstruction(instr, memberName);
		}
		else
		{
			error("type '%s' is not a valid type to GEP into", structPtr->getType()->getPointerElementType());
		}
	}



	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::CreateConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		auto ptri = ConstantInt::getUint64(ptrIndex);
		auto elmi = ConstantInt::getUint64(elmIndex);

		return this->CreateGEP2(ptr, ptri, elmi);
	}

	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::CreateGEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not integer type");
		iceAssert(elmIndex->getType()->isIntegerType() && "elmIndex is not integer type");

		Type* retType = ptr->getType()->getPointerElementType();
		if(retType->isArrayType())
			retType = retType->toArrayType()->getElementType()->getPointerTo();


		Instruction* instr = new Instruction(OpKind::Value_GetGEP2, false, this->currentBlock, retType, { ptr, ptrIndex, elmIndex });

		// disallow storing to members of immut arrays
		if(ptr->isImmutable())
			instr->realOutput->immut = true;

		return this->addInstruction(instr, vname);
	}

	// equivalent to GEP(ptr*, index)
	Value* IRBuilder::CreateGetPointer(Value* ptr, Value* ptrIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		if(!ptrIndex->getType()->isIntegerType())
			error("ptrIndex is not an integer type (got '%s')", ptrIndex->getType());

		Instruction* instr = new Instruction(OpKind::Value_GetPointer, false, this->currentBlock, ptr->getType(), { ptr, ptrIndex });

		// disallow storing to members of immut arrays
		if(ptr->isImmutable())
			instr->realOutput->immut = true;


		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateSizeof(Type* t, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Misc_Sizeof, false, this->currentBlock, Type::getInt64(),
			{ ConstantValue::getZeroValue(t) });

		instr->realOutput->makeImmutable();
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateValue(Type* t, std::string vname)
	{
		return fir::ConstantValue::getZeroValue(t);
	}










	Value* IRBuilder::CreatePointerAdd(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got '%s')", num->getType());

		Instruction* instr = new Instruction(OpKind::Value_PointerAddition, false, this->currentBlock, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerSub(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got '%s')", num->getType());

		Instruction* instr = new Instruction(OpKind::Value_PointerSubtraction, false, this->currentBlock, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateInsertValue(Value* val, std::vector<size_t> inds, Value* elm, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("val is not an aggregate type (have '%s')", t);

		Type* et = 0;
		if(t->isStructType()) { et = t->toStructType()->getElementN(inds[0]); }
		else if(t->isClassType()) { et = t->toClassType()->getElementN(inds[0]); }
		else if(t->isTupleType()) { et = t->toTupleType()->getElementN(inds[0]); }
		else if(t->isArrayType()) { et = t->toArrayType()->getElementType(); }

		iceAssert(et);

		if(elm->getType() != et)
		{
			error("Mismatched types for value and element -- trying to insert '%s' into '%s'",
				elm->getType(), et);
		}

		std::vector<Value*> args = { val, elm };
		for(auto id : inds)
			args.push_back(fir::ConstantInt::getInt64(id));

		// note: no sideeffects, since we return a new aggregate
		Instruction* instr = new Instruction(OpKind::Value_InsertValue, false, this->currentBlock, t, args);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateExtractValue(Value* val, std::vector<size_t> inds, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("val is not an aggregate type (have '%s')", t);

		Type* et = 0;
		if(t->isStructType()) { et = t->toStructType()->getElementN(inds[0]); }
		else if(t->isClassType()) { et = t->toClassType()->getElementN(inds[0]); }
		else if(t->isTupleType()) { et = t->toTupleType()->getElementN(inds[0]); }
		else if(t->isArrayType()) { et = t->toArrayType()->getElementType(); }

		iceAssert(et);

		std::vector<Value*> args = { val };
		for(auto id : inds)
			args.push_back(fir::ConstantInt::getInt64(id));


		// note: no sideeffects, since we return a new aggregate
		Instruction* instr = new Instruction(OpKind::Value_ExtractValue, false, this->currentBlock, et, args);
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateInsertValueByName(Value* val, std::string n, Value* elm, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType())
			error("val is not an aggregate type with named members (class or struct) (have '%s')", t);

		size_t ind = 0;
		if(t->isStructType()) { ind = t->toStructType()->getElementIndex(n); }
		else if(t->isClassType()) { ind = t->toClassType()->getElementIndex(n); }
		iceAssert(ind);

		return this->CreateInsertValue(val, { ind }, elm, vname);
	}

	Value* IRBuilder::CreateExtractValueByName(Value* val, std::string n, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType())
			error("val is not an aggregate type with named members (class or struct) (have '%s')", t);


		size_t ind = 0;
		if(t->isStructType()) { ind = t->toStructType()->getElementIndex(n); }
		else if(t->isClassType()) { ind = t->toClassType()->getElementIndex(n); }
		iceAssert(ind);

		return this->CreateExtractValue(val, { ind }, vname);
	}








	Value* IRBuilder::CreateGetStringData(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetData, false, this->currentBlock,
			fir::Type::getInt8()->getPointerTo(), { str });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetStringData(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt8Ptr())
			error("val is not an int8*");

		Instruction* instr = new Instruction(OpKind::String_SetData, true, this->currentBlock, fir::Type::getString(), { str, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateGetStringLength(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { str });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetStringLength(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetLength, true, this->currentBlock, fir::Type::getString(), { str, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateGetStringRefCount(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetRefCount, false, this->currentBlock,
			fir::Type::getInt64(), { str });

		return this->addInstruction(instr, vname);
	}

	void IRBuilder::CreateSetStringRefCount(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetRefCount, true, this->currentBlock,
			fir::Type::getVoid(), { str, val });

		this->addInstruction(instr, vname);
	}














	Value* IRBuilder::CreateGetDynamicArrayData(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetData, false, this->currentBlock,
			arr->getType()->toDynamicArrayType()->getElementType()->getPointerTo(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetDynamicArrayData(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		auto t = arr->getType()->toDynamicArrayType()->getElementType();
		if(val->getType() != t->getPointerTo())
		{
			error("val is not a pointer to elm type (need '%s', have '%s')",
				t->getPointerTo(), val->getType());
		}

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetData, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateGetDynamicArrayLength(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetDynamicArrayLength(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetLength, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateGetDynamicArrayCapacity(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetCapacity, false, this->currentBlock,
			fir::Type::getInt64(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetDynamicArrayCapacity(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetCapacity, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateGetDynamicArrayRefCount(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetRefCount, false, this->currentBlock,
			fir::Type::getInt64(), { arr });

		return this->addInstruction(instr, vname);
	}

	void IRBuilder::CreateSetDynamicArrayRefCount(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetRefCount, true, this->currentBlock,
			fir::Type::getVoid(), { arr, val });

		this->addInstruction(instr, vname);
	}













	Value* IRBuilder::CreateGetArraySliceData(Value* slc, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		Instruction* instr = new Instruction(OpKind::ArraySlice_GetData, false, this->currentBlock,
			slc->getType()->toArraySliceType()->getElementType()->getPointerTo(), { slc });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetArraySliceData(Value* slc, Value* val, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		auto et = slc->getType()->toArraySliceType()->getElementType();
		if(val->getType() != et->getPointerTo())
			error("val is not a pointer to elm type (need '%s', have '%s')", et->getPointerTo(), val->getType());

		Instruction* instr = new Instruction(OpKind::ArraySlice_SetData, true, this->currentBlock,
			slc->getType(), { slc, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateGetArraySliceLength(Value* slc, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		Instruction* instr = new Instruction(OpKind::ArraySlice_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { slc });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetArraySliceLength(Value* slc, Value* val, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::ArraySlice_SetLength, true, this->currentBlock,
			slc->getType(), { slc, val });

		return this->addInstruction(instr, vname);
	}








	Value* IRBuilder::CreateGetAnyTypeID(Value* any, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		Instruction* instr = new Instruction(OpKind::Any_GetTypeID, false, this->currentBlock, fir::Type::getInt64(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetAnyTypeID(Value* any, Value* val, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::Any_SetTypeID, true, this->currentBlock, fir::Type::getVoid(), { any, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateGetAnyFlag(Value* any, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		Instruction* instr = new Instruction(OpKind::Any_GetFlag, false, this->currentBlock, fir::Type::getInt64(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetAnyFlag(Value* any, Value* val, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::Any_SetFlag, true, this->currentBlock, fir::Type::getVoid(), { any, val });

		return this->addInstruction(instr, vname);
	}


	// note: getData() returns i8*, to facilitate pointer tomfoolery.
	// setData() just takes any type, as long as its size is <= 24 bytes, so we can let the LLVM translator
	// handle the nitty-gritty
	Value* IRBuilder::CreateGetAnyData(Value* any, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		Instruction* instr = new Instruction(OpKind::Any_GetData, false, this->currentBlock, fir::Type::getInt8()->getPointerTo(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetAnyData(Value* any, Value* val, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		iceAssert(this->context);
		iceAssert(this->context->module);

		size_t sz = this->context->module->getExecutionTarget()->getTypeSizeInBytes(val->getType());
		if(sz > 24 || sz == -1)
		{
			error("Type '%s' cannot be stored directly in 'any', size is too large (max 24 bytes, have %zd bytes)",
				val->getType(), (int64_t) sz);
		}

		Instruction* instr = new Instruction(OpKind::Any_SetData, true, this->currentBlock, fir::Type::getVoid(), { any, val });

		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::CreateGetRangeLower(Value* range, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (have '%s')", range->getType());

		Instruction* instr = new Instruction(OpKind::Range_GetLower, false, this->currentBlock,
			fir::Type::getInt64(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetRangeLower(Value* range, Value* val, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("val is not an integer type (got '%s')", val->getType());

		Instruction* instr = new Instruction(OpKind::Range_SetLower, true, this->currentBlock,
			fir::Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateGetRangeUpper(Value* range, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (have '%s')", range->getType());

		Instruction* instr = new Instruction(OpKind::Range_GetUpper, false, this->currentBlock,
			fir::Type::getInt64(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetRangeUpper(Value* range, Value* val, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("val is not an integer type (got '%s')", val->getType());

		Instruction* instr = new Instruction(OpKind::Range_SetUpper, true, this->currentBlock,
			fir::Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}




	Value* IRBuilder::CreateGetEnumCaseIndex(Value* ecs, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		Instruction* instr = new Instruction(OpKind::Enum_GetIndex, true, this->currentBlock,
			fir::Type::getInt64(), { ecs });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetEnumCaseIndex(Value* ecs, Value* idx, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		if(!idx->getType()->isIntegerType())
			error("index is not an integer type (got '%s')", idx->getType());

		Instruction* instr = new Instruction(OpKind::Enum_SetIndex, true, this->currentBlock,
			ecs->getType(), { ecs, idx });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateGetEnumCaseValue(Value* ecs, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		Instruction* instr = new Instruction(OpKind::Enum_GetValue, true, this->currentBlock,
			ecs->getType()->toEnumType()->getCaseType(), { ecs });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetEnumCaseValue(Value* ecs, Value* val, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		if(ecs->getType()->toEnumType()->getCaseType() != val->getType())
		{
			error("value type mismatch (enum case type is '%s', value type is '%s'",
				ecs->getType()->toEnumType()->getCaseType(), val->getType());
		}

		Instruction* instr = new Instruction(OpKind::Enum_SetValue, true, this->currentBlock,
			ecs->getType(), { ecs, val });

		return this->addInstruction(instr, vname);
	}














	void IRBuilder::CreateUnreachable()
	{
		this->addInstruction(new Instruction(OpKind::Unreachable, true, this->currentBlock, fir::Type::getVoid(), { }), "");
	}
























	IRBlock* IRBuilder::addNewBlockInFunction(std::string name, Function* func)
	{
		IRBlock* block = new IRBlock(func);
		if(func != this->currentFunction)
		{
			// warn("changing current function in irbuilder");
			this->currentFunction = block->parentFunction;
		}

		this->currentFunction->blocks.push_back(block);

		size_t cnt = 0;
		for(auto b : this->currentFunction->blocks)
			if(b->getName().str() == name) cnt++;

		if(cnt > 0)
			name += "." + std::to_string(cnt);

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
			if(b == block)
			{
				size_t cnt = 0;
				for(auto bk : this->currentFunction->blocks)
					if(bk->getName().str() == name) cnt++;

				if(cnt > 0)
					name += "." + std::to_string(cnt);

				nb->setName(name);
				this->currentFunction->blocks.insert(this->currentFunction->blocks.begin() + i + 1, nb);
				return nb;
			}
		}

		iceAssert(0 && "no such block to insert after");
		nb->setName(name);
		return nb;
	}
}




