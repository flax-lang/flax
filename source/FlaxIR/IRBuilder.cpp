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

	static Instruction* getBinaryOpInstruction(IRBlock* parent, Ast::ArithmeticOp ao, Value* vlhs, Value* vrhs)
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

			out = lhs;
		}
		else if(ao == Ast::ArithmeticOp::ShiftRight)
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = useSigned ? OpKind::Bitwise_Arithmetic_Shr : OpKind::Bitwise_Logical_Shr;

			out = lhs;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseAnd)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_And;

			out = lhs;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseOr)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Or;

			out = lhs;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseXor)
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Xor;

			out = lhs;
		}
		else if(ao == Ast::ArithmeticOp::BitwiseNot)
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

	Value* IRBuilder::CreateBinaryOp(Ast::ArithmeticOp ao, Value* a, Value* b, std::string vname)
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
			error("creating add instruction with non-equal types (%s vs %s)", a->getType()->str().c_str(), b->getType()->str().c_str());

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
			error("creating sub instruction with non-equal types (%s vs %s)", a->getType()->str().c_str(), b->getType()->str().c_str());

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
			error("creating mul instruction with non-equal types (%s vs %s)", a->getType()->str().c_str(), b->getType()->str().c_str());

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
			error("creating div instruction with non-equal types (%s vs %s)", a->getType()->str().c_str(), b->getType()->str().c_str());


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
			error("creating mod instruction with non-equal types (%s vs %s)", a->getType()->str().c_str(), b->getType()->str().c_str());

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
			{ v, ConstantValue::getNullValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFExtend(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Extend, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });

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
		iceAssert(a->getType()->isIntegerType() && "creating icmp multi instruction with non-integer type");
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
			{ v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntSizeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		// make constant result for constant operand
		if(ConstantInt* ci = dynamic_cast<ConstantInt*>(v))
		{
			return ConstantInt::get(targetType, ci->getSignedValue());
		}

		Instruction* instr = new Instruction(OpKind::Cast_IntSize, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntSignednessCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		// make constant result for constant operand
		if(ConstantInt* ci = dynamic_cast<ConstantInt*>(v))
		{
			return ConstantInt::get(targetType, ci->getType()->isSignedIntType() ? ci->getSignedValue() : ci->getUnsignedValue());
		}

		Instruction* instr = new Instruction(OpKind::Cast_IntSignedness, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });

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
				warn("Truncating constant '%Lf' in constant cast to type '%s'", cfp->getValue(), targetType->str().c_str());

			return ConstantInt::get(targetType, (size_t) cfp->getValue());
		}

		Instruction* instr = new Instruction(OpKind::Cast_FloatToInt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntToFloatCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isFloatingPointType() && "target is not floating point type");

		// make constant result for constant operand
		if(ConstantInt* ci = dynamic_cast<ConstantInt*>(v))
		{
			if(targetType == fir::Type::getFloat32())
			{
				return ConstantFP::getFloat32((float) ci->getType()->isSignedIntType() ? ci->getSignedValue()
					: ci->getUnsignedValue());
			}
			else if(targetType == fir::Type::getFloat64())
			{
				return ConstantFP::getFloat64((double) ci->getType()->isSignedIntType() ? ci->getSignedValue()
					: ci->getUnsignedValue());
			}
			else
			{
				error("Unknown floating point type '%s'", targetType->str().c_str());
			}
		}


		Instruction* instr = new Instruction(OpKind::Cast_IntToFloat, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerTypeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerType, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerToIntCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerToInt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntToPointerCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToPointer, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateIntTruncate(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Integer_Truncate, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntZeroExt(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Integer_ZeroExt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}








	Value* IRBuilder::CreateLoad(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got %s)", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_Load, false, this->currentBlock, ptr->getType()->getPointerElementType(), { ptr });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateStore(Value* v, Value* ptr)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got %s)", ptr->getType()->str().c_str());

		if(v->getType()->getPointerTo() != ptr->getType())
			error("ptr is not a pointer to type of value (storing %s into %s)", v->getType()->str().c_str(), ptr->getType()->str().c_str());

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


	Value* IRBuilder::CreateCall(Function* fn, std::deque<Value*> args, std::string vname)
	{
		// in theory we should still check, but i'm lazy right now
		// TODO.
		if(!fn->isCStyleVarArg())
		{
			// check here, to stop llvm dying
			if(args.size() != fn->getArgumentCount())
				error("Calling function %s with the wrong number of arguments (needs %zu, have %zu)", fn->getName().str().c_str(),
					fn->getArgumentCount(), args.size());

			for(size_t i = 0; i < args.size(); i++)
			{
				auto target = fn->getArguments()[i]->getType();
				if(args[i]->getType() != target)
				{
					if(args[i]->getType()->isPrimitiveType() && args[i]->getType()->toPrimitiveType()->isLiteralType())
					{
						bool shouldcast = false;

						ConstantInt* ci = 0;
						ConstantFP* cf = 0;
						if((ci = dynamic_cast<ConstantInt*>(args[i])))
						{
							if(ci->getType()->isSignedIntType())
							{
								shouldcast = fir::checkSignedIntLiteralFitsIntoType(target->toPrimitiveType(),
									ci->getSignedValue());
							}
							else
							{
								shouldcast = fir::checkUnsignedIntLiteralFitsIntoType(target->toPrimitiveType(),
									ci->getUnsignedValue());
							}
						}
						else if((cf = dynamic_cast<ConstantFP*>(args[i])))
						{
							shouldcast = fir::checkFloatingPointLiteralFitsIntoType(target->toPrimitiveType(), cf->getValue());
						}




						if(shouldcast)
						{
							if(ci)
							{
								PrimitiveType* real = 0;
								if(ci->getType()->isSignedIntType())
									real = fir::PrimitiveType::getIntN(target->toPrimitiveType()->getIntegerBitWidth());

								else
									real = fir::PrimitiveType::getUintN(target->toPrimitiveType()->getIntegerBitWidth());

								args[i] = fir::ConstantInt::get(real, ci->getSignedValue());
							}
							else if(cf)
							{
								args[i] = fir::ConstantFP::get(target, cf->getValue());
							}
							else
							{
								args[i]->setType(target);
							}
						}


						if(args[i]->getType() == target)
							continue;
					}

					error("Mismatch in argument type (arg. %zu) in function %s (need %s, have %s)", i, fn->getName().str().c_str(),
						fn->getArguments()[i]->getType()->str().c_str(), args[i]->getType()->str().c_str());
				}
			}
		}

		args.push_front(fn);

		Instruction* instr = new Instruction(OpKind::Value_CallFunction, true, this->currentBlock, fn->getType()->getReturnType(), args);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::vector<Value*> args, std::string vname)
	{
		std::deque<Value*> dargs;

		for(auto a : args)
			dargs.push_back(a);

		return this->CreateCall(fn, dargs, vname);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::initializer_list<Value*> args, std::string vname)
	{
		std::deque<Value*> dargs;

		for(auto a : args)
			dargs.push_back(a);

		return this->CreateCall(fn, dargs, vname);
	}





	Value* IRBuilder::CreateCallToFunctionPointer(Value* fn, FunctionType* ft, std::deque<Value*> args, std::string vname)
	{
		// we can't really check anything.
		args.push_front(fn);

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

	Value* IRBuilder::CreateStackAlloc(Type* type, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_StackAlloc, false, this->currentBlock, type->getPointerTo(),
			{ ConstantValue::getNullValue(type) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateImmutStackAlloc(Type* type, Value* v, std::string vname)
	{
		Value* ret = this->CreateStackAlloc(type, vname);
		ret->immut = false;		// for now

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
			error("type %s is not a valid type to GEP into", ptr->getType()->getPointerElementType()->str().c_str());
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
			error("type '%s' is not a valid type to GEP into", structPtr->getType()->getPointerElementType()->str().c_str());
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
			error("type %s is not a valid type to GEP into", structPtr->getType()->getPointerElementType()->str().c_str());
		}
	}



	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::CreateConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		auto ptri = ConstantInt::getUint64(ptrIndex);
		auto elmi = ConstantInt::getUint64(elmIndex);

		return this->CreateGEP2(ptr, ptri, elmi);
	}

	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::CreateGEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

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
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		if(!ptrIndex->getType()->isIntegerType())
			error("ptrIndex is not an integer type (got %s)", ptrIndex->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_GetPointer, false, this->currentBlock, ptr->getType(), { ptr, ptrIndex });

		// disallow storing to members of immut arrays
		if(ptr->isImmutable())
			instr->realOutput->immut = true;


		return this->addInstruction(instr, vname);
	}















	Value* IRBuilder::CreatePointerAdd(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got %s)", num->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_PointerAddition, false, this->currentBlock, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerSub(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got %s)", num->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_PointerSubtraction, false, this->currentBlock, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}










	Value* IRBuilder::CreateGetStringData(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isStringType())
			error("ptr is not a pointer to string type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::String_GetData, false, this->currentBlock,
			fir::Type::getInt8()->getPointerTo(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetStringData(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isStringType())
			error("ptr is not a pointer to string type (got '%s')", ptr->getType()->str().c_str());

		if(val->getType() != fir::Type::getInt8Ptr())
			error("val is not an int8*");

		Instruction* instr = new Instruction(OpKind::String_SetData, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateGetStringLength(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isStringType())
			error("ptr is not a pointer to string type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::String_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetStringLength(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isStringType())
			error("ptr is not a pointer to string type (got '%s')", ptr->getType()->str().c_str());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetLength, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateGetStringRefCount(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isStringType())
			error("ptr is not a pointer to string type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::String_GetRefCount, false, this->currentBlock,
			fir::Type::getInt64(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetStringRefCount(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isStringType())
			error("ptr is not a pointer to string type (got '%s')", ptr->getType()->str().c_str());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetRefCount, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}














	Value* IRBuilder::CreateGetDynamicArrayData(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isDynamicArrayType())
			error("ptr is not a pointer to dynamic array type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetData, false, this->currentBlock,
			ptr->getType()->getPointerElementType()->toDynamicArrayType()->getElementType()->getPointerTo(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetDynamicArrayData(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isDynamicArrayType())
			error("ptr is not a pointer to dynamic array type (got '%s')", ptr->getType()->str().c_str());

		auto t = ptr->getType()->getPointerElementType()->toDynamicArrayType()->getElementType();
		if(val->getType() != t->getPointerTo())
		{
			error("val is not a pointer to elm type (need '%s', have '%s')",
				t->getPointerTo()->str().c_str(), val->getType()->str().c_str());
		}

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetData, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateGetDynamicArrayLength(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isDynamicArrayType())
			error("ptr is not a pointer to dynamic array type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetDynamicArrayLength(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isDynamicArrayType())
			error("ptr is not a pointer to dynamic array type (got '%s')", ptr->getType()->str().c_str());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetLength, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::CreateGetDynamicArrayCapacity(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isDynamicArrayType())
			error("ptr is not a pointer to dynamic array type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetCapacity, false, this->currentBlock,
			fir::Type::getInt64(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetDynamicArrayCapacity(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isDynamicArrayType())
			error("ptr is not a pointer to dynamic array type (got '%s')", ptr->getType()->str().c_str());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetCapacity, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}










	Value* IRBuilder::CreateGetParameterPackData(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isParameterPackType())
			error("ptr is not a pointer to a parameter pack type (got '%s')", ptr->getType()->str().c_str());

		auto t = ptr->getType()->getPointerElementType()->toParameterPackType()->getElementType();
		Instruction* instr = new Instruction(OpKind::ParamPack_GetData, false, this->currentBlock,
			t->getPointerTo(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetParameterPackData(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isParameterPackType())
			error("ptr is not a pointer to a parameter pack type (got '%s')", ptr->getType()->str().c_str());

		auto t = ptr->getType()->getPointerElementType()->toParameterPackType()->getElementType();
		if(val->getType() != t->getPointerTo())
		{
			error("val is not a pointer to element type (need '%s', have '%s')", t->getPointerTo()->str().c_str(),
				val->getType()->str().c_str());
		}

		Instruction* instr = new Instruction(OpKind::ParamPack_SetData, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateGetParameterPackLength(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isParameterPackType())
			error("ptr is not a pointer to a parameter pack type (got '%s')", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::ParamPack_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { ptr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateSetParameterPackLength(Value* ptr, Value* val, std::string vname)
	{
		if(!ptr->getType()->isPointerType() || !ptr->getType()->getPointerElementType()->isParameterPackType())
			error("ptr is not a pointer to a parameter pack type (got '%s')", ptr->getType()->str().c_str());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::ParamPack_SetLength, true, this->currentBlock,
			fir::Type::getVoid(), { ptr, val });

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
				this->currentFunction->blocks.insert(this->currentFunction->blocks.begin() + i + 1, nb);
				return nb;
			}
		}

		iceAssert(0 && "no such block to insert after");
		nb->setName(name);
		return nb;
	}
}




