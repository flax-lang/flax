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

	static Instruction* getBinaryOpInstruction(IRBlock* parent, std::string ao, Value* vlhs, Value* vrhs)
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
		if(ao == "+")
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
		else if(ao == "-")
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
		else if(ao == "*")
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
		else if(ao == "/")
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
		else if(ao == "%")
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
		else if(ao == "<<")
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = OpKind::Bitwise_Shl;

			out = lhs;
		}
		else if(ao == ">>")
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = useSigned ? OpKind::Bitwise_Arithmetic_Shr : OpKind::Bitwise_Logical_Shr;

			out = lhs;
		}
		else if(ao == "&")
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_And;

			out = lhs;
		}
		else if(ao == "|")
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Or;

			out = lhs;
		}
		else if(ao == "^")
		{
			if(useFloating) iceAssert("bitwise ops only defined for int types (cast if needed)");
			op = OpKind::Bitwise_Xor;

			out = lhs;
		}
		else if(ao == "~")
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

	Value* IRBuilder::BinaryOp(std::string ao, Value* a, Value* b, std::string vname)
	{
		Instruction* instr = getBinaryOpInstruction(this->currentBlock, ao, a, b);
		if(instr == 0) return 0;

		return this->addInstruction(instr, vname);
	}































	Value* IRBuilder::Negate(Value* a, std::string vname)
	{
		iceAssert(a->getType()->toPrimitiveType() && "cannot negate non-primitive type");
		iceAssert((a->getType()->isFloatingPointType() || a->getType()->toPrimitiveType()->isSigned()) && "cannot negate unsigned type");

		Instruction* instr = new Instruction(a->getType()->isFloatingPointType() ? OpKind::Floating_Neg : OpKind::Signed_Neg,
			false, this->currentBlock, a->getType(), { a });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Add(Value* a, Value* b, std::string vname)
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

	Value* IRBuilder::Subtract(Value* a, Value* b, std::string vname)
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

	Value* IRBuilder::Multiply(Value* a, Value* b, std::string vname)
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

	Value* IRBuilder::Divide(Value* a, Value* b, std::string vname)
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

	Value* IRBuilder::Modulo(Value* a, Value* b, std::string vname)
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

	Value* IRBuilder::FTruncate(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Truncate, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FExtend(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Extend, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::ICmpEQ(Value* a, Value* b, std::string vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("creating icmp eq instruction with non-equal types");
		}

		Instruction* instr = new Instruction(OpKind::ICompare_Equal, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpNEQ(Value* a, Value* b, std::string vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("creating icmp neq instruction with non-equal types");
		}

		Instruction* instr = new Instruction(OpKind::ICompare_NotEqual, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpGT(Value* a, Value* b, std::string vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("creating icmp gt instruction with non-equal types");
		}

		Instruction* instr = new Instruction(OpKind::ICompare_Greater, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpLT(Value* a, Value* b, std::string vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("creating icmp lt instruction with non-equal types");
		}

		Instruction* instr = new Instruction(OpKind::ICompare_Less, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpGEQ(Value* a, Value* b, std::string vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("creating icmp geq instruction with non-equal types");
		}

		Instruction* instr = new Instruction(OpKind::ICompare_GreaterEqual, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpLEQ(Value* a, Value* b, std::string vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("creating icmp leq instruction with non-equal types");
		}

		Instruction* instr = new Instruction(OpKind::ICompare_LessEqual, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::FCmpEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_ord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_ORD, false, this->currentBlock, fir::Type::getBool(this->context),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_uord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpNEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_ord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpNEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_uord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGT_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGT_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLT_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLT_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_GreaterEqual_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_GreaterEqual_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual_ORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual_UNORD, false, this->currentBlock,
			fir::Type::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}


	// returns -1 for a < b, 0 for a == b, 1 for a > b
	Value* IRBuilder::ICmpMulti(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp multi instruction with non-equal types");
		// iceAssert(a->getType()->isIntegerType() && "creating icmp multi instruction with non-integer type");
		Instruction* instr = new Instruction(OpKind::ICompare_Multi, false, this->currentBlock,
			fir::Type::getInt64(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpMulti(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = new Instruction(OpKind::FCompare_Multi, false, this->currentBlock,
			fir::Type::getInt64(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}










	Value* IRBuilder::BitwiseXOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise xor instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Xor, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseLogicalSHR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise lshl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Logical_Shr, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseArithmeticSHR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise ashl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Arithmetic_Shr, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseSHL(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise shr instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Shl, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseAND(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise and instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_And, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise or instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Or, false, this->currentBlock, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseNOT(Value* a, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Bitwise_Not, false, this->currentBlock, a->getType(), { a });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Bitcast(Value* v, Type* targetType, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Cast_Bitcast, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntSizeCast(Value* v, Type* targetType, std::string vname)
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

	Value* IRBuilder::IntSignednessCast(Value* v, Type* targetType, std::string vname)
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

	Value* IRBuilder::FloatToIntCast(Value* v, Type* targetType, std::string vname)
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

	Value* IRBuilder::IntToFloatCast(Value* v, Type* targetType, std::string vname)
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

	Value* IRBuilder::PointerTypeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert((v->getType()->isPointerType() || v->getType()->isNullType()) && "value is not pointer type");
		iceAssert((targetType->isPointerType() || targetType->isNullType()) && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerType, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::PointerToIntCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerToInt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntToPointerCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToPointer, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::IntTruncate(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Integer_Truncate, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntZeroExt(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Integer_ZeroExt, false, this->currentBlock, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::AppropriateCast(Value* v, Type* r, std::string vname)
	{
		auto l = v->getType();

		if(l->isIntegerType() && r->isIntegerType())
			return this->IntSizeCast(v, r);

		else if(l->isFloatingPointType() && r->isFloatingPointType())
			return (l->getBitWidth() > r->getBitWidth() ? this->FTruncate(v, r) : this->FExtend(v, r));

		else if(l->isIntegerType() && r->isFloatingPointType())
			return this->IntToFloatCast(v, r);

		else if(l->isFloatingPointType() && r->isIntegerType())
			return this->FloatToIntCast(v, r);

		else if(l->isIntegerType() && r->isPointerType())
			return this->IntToPointerCast(v, r);

		else if(l->isPointerType() && r->isIntegerType())
			return this->PointerToIntCast(v, r);

		else if(l->isPointerType() && r->isPointerType())
			return this->PointerTypeCast(v, r);

		// nope.
		return 0;
	}










	Value* IRBuilder::Load(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got '%s')", ptr->getType());

		Instruction* instr = new Instruction(OpKind::Value_Load, false, this->currentBlock, ptr->getType()->getPointerElementType(), { ptr });
		auto ret = this->addInstruction(instr, vname);

		return ret;
	}

	Value* IRBuilder::Store(Value* v, Value* ptr)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got '%s')", ptr->getType());

		if(ptr->getType()->isImmutablePointer())
			error("Cannot store value to immutable pointer type '%s'", ptr->getType());

		auto vt = v->getType();
		auto pt = ptr->getType();

		if(vt != pt->getPointerElementType())
		{
			//* here, we know that the storage pointer is mutable. there's a special edge-case we need to catch:
			//* if we're storing a value of type &T to a & &mut T, or a &mut T to a & &T.
			//* in those cases, the mutability of the base type doesn't matter at all. At the LLVM level, we don't even make a distinction,
			//* so we can safely pass this onto the translation layer without worrying about it.

			// if((vt->isPointerType() && pt->isPointerType() && vt->getPointerElementType() == pt->getPointerElementType()) == false)
			error("ptr is not a pointer to type of value (base types '%s' -> '%s' differ)", vt, pt->getPointerElementType());
		}


		Instruction* instr = new Instruction(OpKind::Value_Store, true, this->currentBlock, Type::getVoid(), { v, ptr });
		return this->addInstruction(instr, "");
	}


	Value* IRBuilder::Call(Function* fn, std::string vname)
	{
		return this->Call(fn, { }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, std::string vname)
	{
		return this->Call(fn, { p1 }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, Value* p2, std::string vname)
	{
		return this->Call(fn, { p1, p2 }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, Value* p2, Value* p3, std::string vname)
	{
		return this->Call(fn, { p1, p2, p3 }, vname);
	}


	// static void checkCallArguments(Function* fn


	Value* IRBuilder::Call(Function* fn, std::vector<Value*> args, std::string vname)
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
				auto at = args[i]->getType();
				auto target = fn->getArguments()[i]->getType();

				// special case
				if(at->isDynamicArrayType() && target->isDynamicArrayType() &&
					target->toDynamicArrayType()->isFunctionVariadic() != at->toDynamicArrayType()->isFunctionVariadic())
				{
					// silently cast, because they're the same thing
					// the distinction is solely for the type system's benefit
					args[i] = this->Bitcast(args[i], target);
				}
				else if(at->isPointerType() && target->isPointerType() && at->getPointerElementType() == target->getPointerElementType() &&
					at->isMutablePointer() && target->isImmutablePointer())
				{
					// this is ok. at the llvm level the cast should reduce to a no-op.
					args[i] = this->PointerTypeCast(args[i], target);
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

	Value* IRBuilder::Call(Function* fn, std::initializer_list<Value*> args, std::string vname)
	{
		return this->Call(fn, std::vector<Value*>(args.begin(), args.end()), vname);
	}





	Value* IRBuilder::CallToFunctionPointer(Value* fn, FunctionType* ft, std::vector<Value*> args, std::string vname)
	{
		// we can't really check anything.
		args.insert(args.begin(), fn);

		Instruction* instr = new Instruction(OpKind::Value_CallFunctionPointer, true, this->currentBlock, ft->getReturnType(), args);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CallVirtualMethod(ClassType* cls, FunctionType* ft, size_t index, std::vector<Value*> args, std::string vname)
	{
		// args[0] must be the self, for obvious reasons.
		auto ty = args[0]->getType();
		iceAssert(ty->isPointerType() && ty->getPointerElementType()->isClassType());

		auto self = ty->getPointerElementType()->toClassType();
		iceAssert(self && self == cls);

		Instruction* instr = new Instruction(OpKind::Value_CallVirtualMethod, true, this->currentBlock, ft->getReturnType(),
			(Value*) ConstantValue::getZeroValue(cls) + ((Value*) ConstantInt::getInt64(index) + ((Value*) ConstantValue::getZeroValue(ft) + args)));

		return this->addInstruction(instr, vname);
	}




















	Value* IRBuilder::Return(Value* v)
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, true, this->currentBlock, Type::getVoid(), { v });
		return this->addInstruction(instr, "");
	}

	Value* IRBuilder::ReturnVoid()
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, true, this->currentBlock, Type::getVoid(), { });
		return this->addInstruction(instr, "");
	}


	Value* IRBuilder::LogicalNot(Value* v, std::string vname)
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

	Value* IRBuilder::StackAlloc(Type* type, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_StackAlloc, false, this->currentBlock, type->getMutablePointerTo(),
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

	Value* IRBuilder::ImmutStackAlloc(Type* type, Value* v, std::string vname)
	{
		Value* ret = this->StackAlloc(type, vname);
		this->Store(v, ret);

		// now make it immutable.
		ret->setType(type->getPointerTo());
		return ret;
	}


	Value* IRBuilder::CreateSliceFromString(Value* str, bool mut, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("expected string type, found '%s' instead", str->getType());

		// this is one of those compound thingies.
		auto slc = this->CreateValue(fir::ArraySliceType::get(fir::Type::getChar(), mut));
		slc = this->SetArraySliceData(slc, this->GetStringData(str));
		slc = this->SetArraySliceLength(slc, this->GetStringLength(str));

		return slc;
	}

	Value* IRBuilder::CreateSliceFromDynamicArray(Value* str, bool mut, std::string vname)
	{
		if(!str->getType()->isDynamicArrayType())
			error("expected dynamic array type, found '%s' instead", str->getType());

		// this is one of those compound thingies.
		auto slc = this->CreateValue(fir::ArraySliceType::get(str->getType()->getArrayElementType(), mut));
		slc = this->SetArraySliceData(slc, this->GetDynamicArrayData(str));
		slc = this->SetArraySliceLength(slc, this->GetDynamicArrayLength(str));

		return slc;
	}


	void IRBuilder::CondBranch(Value* condition, IRBlock* trueB, IRBlock* falseB)
	{
		Instruction* instr = new Instruction(OpKind::Branch_Cond, true, this->currentBlock, Type::getVoid(),
			{ condition, trueB, falseB });
		this->addInstruction(instr, "");
	}

	void IRBuilder::UnCondBranch(IRBlock* target)
	{
		Instruction* instr = new Instruction(OpKind::Branch_UnCond, true, this->currentBlock, Type::getVoid(),
			{ target });
		this->addInstruction(instr, "");
	}




	// gep stuff


	// structs and tuples have the same member names.
	// template <typename T>
	// static Instruction* doGEPOnCompoundType(IRBlock* parent, T* type, Value* ptr, Value* ptrIndex, size_t memberIndex)
	// {
	// 	iceAssert(type->getElementCount() > memberIndex && "struct does not have so many members");

	// 	Instruction* instr = new Instruction(OpKind::Value_GetPointerToStructMember, false, parent,
	// 		type->getElementN(memberIndex)->getPointerTo(), { ptr, ptrIndex, ConstantInt::getUint64(memberIndex) });

	// 	// disallow storing to members of immut structs
	// 	if(ptr->isImmutable())
	// 		instr->realOutput->makeImmutable();

	// 	return instr;
	// }

	template <typename T>
	static Instruction* doGEPOnCompoundType(IRBlock* parent, T* type, Value* structPtr, size_t memberIndex)
	{
		iceAssert(type->getElementCount() > memberIndex && "struct does not have so many members");

		bool mut = structPtr->getType()->isMutablePointer();

		Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, parent,
			mut ? type->getElementN(memberIndex)->getMutablePointerTo() : type->getElementN(memberIndex)->getPointerTo(),
			{ structPtr, ConstantInt::getUint64(memberIndex) });

		return instr;
	}




	Value* IRBuilder::StructGEP(Value* structPtr, size_t memberIndex, std::string vname)
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

	Value* IRBuilder::GetStructMember(Value* structPtr, std::string memberName)
	{
		iceAssert(structPtr->getType()->isPointerType() && "ptr is not pointer");
		if(StructType* st = dynamic_cast<StructType*>(structPtr->getType()->getPointerElementType()))
		{
			auto spt = structPtr->getType();
			auto memt = st->getElement(memberName);

			iceAssert(st->hasElementWithName(memberName) && "no element with such name");

			Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, this->currentBlock,
				spt->isMutablePointer() ? memt->getMutablePointerTo() : memt->getPointerTo(),
				{ structPtr, ConstantInt::getUint64(st->getElementIndex(memberName)) });

			return this->addInstruction(instr, memberName);
		}
		else if(ClassType* ct = dynamic_cast<ClassType*>(structPtr->getType()->getPointerElementType()))
		{
			iceAssert(ct->hasElementWithName(memberName) && "no element with such name");

			auto cpt = structPtr->getType();
			auto memt = ct->getElement(memberName);

			//! '+1' is for vtable.
			Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, this->currentBlock,
				cpt->isMutablePointer() ? memt->getMutablePointerTo() : memt->getPointerTo(),
				{ structPtr, ConstantInt::getUint64(ct->getElementIndex(memberName) + 1) });


			return this->addInstruction(instr, memberName);
		}
		else
		{
			error("type '%s' is not a valid type to GEP into", structPtr->getType()->getPointerElementType());
		}
	}



	void IRBuilder::SetVtable(Value* ptr, Value* table, std::string vname)
	{
		auto ty = ptr->getType();
		if(!ty->isPointerType())                        error("'%s' is not a pointer type", ty);
		if(!ty->getPointerElementType()->isClassType()) error("'%s' is not a pointer-to-class type", ty);
		if(table->getType() != fir::Type::getInt8Ptr()) error("expected i8* for vtable, got '%s'", table->getType());

		Instruction* instr = new Instruction(OpKind::Value_GetStructMember, false, this->currentBlock,
			fir::Type::getInt8Ptr()->getMutablePointerTo(), { ptr, ConstantInt::getUint64(0) });

		auto gep = this->addInstruction(instr, vname);

		this->Store(table, gep);
	}




	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::ConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		auto ptri = ConstantInt::getUint64(ptrIndex);
		auto elmi = ConstantInt::getUint64(elmIndex);

		return this->GEP2(ptr, ptri, elmi);
	}

	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::GEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		else if(ptr->getType()->getPointerElementType()->isClassType() || ptr->getType()->getPointerElementType()->isStructType())
			error("use the other function for struct types");

		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not integer type");
		iceAssert(elmIndex->getType()->isIntegerType() && "elmIndex is not integer type");

		Type* retType = ptr->getType()->getPointerElementType();
		if(retType->isArrayType())
			retType = retType->toArrayType()->getElementType()->getPointerTo();


		if(ptr->getType()->isMutablePointer())
			retType = retType->getMutablePointerVersion();

		Instruction* instr = new Instruction(OpKind::Value_GetGEP2, false, this->currentBlock, retType, { ptr, ptrIndex, elmIndex });

		return this->addInstruction(instr, vname);
	}

	// equivalent to GEP(ptr*, index)
	Value* IRBuilder::GetPointer(Value* ptr, Value* ptrIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		if(!ptrIndex->getType()->isIntegerType())
			error("ptrIndex is not an integer type (got '%s')", ptrIndex->getType());

		if(ptr->getType()->getPointerElementType()->isClassType() || ptr->getType()->getPointerElementType()->isStructType())
			error("use the other function for struct types");

		Instruction* instr = new Instruction(OpKind::Value_GetPointer, false, this->currentBlock, ptr->getType(), { ptr, ptrIndex });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::Select(Value* cond, Value* one, Value* two, std::string vname)
	{
		if(!cond->getType()->isBoolType())
			error("cond is not a boolean type (got '%s')", cond->getType());

		if(one->getType() != two->getType())
			error("Non-identical types for operands (got '%s' and '%s')", one->getType(), two->getType());

		Instruction* instr = new Instruction(OpKind::Value_Select, false, this->currentBlock, one->getType(), { cond, one, two });
		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::Sizeof(Type* t, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Misc_Sizeof, false, this->currentBlock, Type::getInt64(),
			{ ConstantValue::getZeroValue(t) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateValue(Type* t, std::string vname)
	{
		return fir::ConstantValue::getZeroValue(t);
	}










	Value* IRBuilder::PointerAdd(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got '%s')", num->getType());

		Instruction* instr = new Instruction(OpKind::Value_PointerAddition, false, this->currentBlock, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::PointerSub(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got '%s')", ptr->getType());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got '%s')", num->getType());

		Instruction* instr = new Instruction(OpKind::Value_PointerSubtraction, false, this->currentBlock, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::InsertValue(Value* val, std::vector<size_t> inds, Value* elm, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("val is not an aggregate type (have '%s')", t);

		Type* et = 0;
		if(t->isStructType())       et = t->toStructType()->getElementN(inds[0]);
		else if(t->isClassType())   et = t->toClassType()->getElementN(inds[0]);
		else if(t->isTupleType())   et = t->toTupleType()->getElementN(inds[0]);
		else if(t->isArrayType())   et = t->toArrayType()->getElementType();

		iceAssert(et);

		if(elm->getType() != et)
		{
			error("Mismatched types for value and element -- trying to insert '%s' into '%s'",
				elm->getType(), et);
		}

		int ofs = 0;
		if(t->isClassType()) ofs = 1;   //! to account for vtable

		std::vector<Value*> args = { val, elm };
		for(auto id : inds)
			args.push_back(fir::ConstantInt::getInt64(id + ofs));

		// note: no sideeffects, since we return a new aggregate
		Instruction* instr = new Instruction(OpKind::Value_InsertValue, false, this->currentBlock, t, args);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ExtractValue(Value* val, std::vector<size_t> inds, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("val is not an aggregate type (have '%s')", t);

		Type* et = 0;
		if(t->isStructType())       et = t->toStructType()->getElementN(inds[0]);
		else if(t->isClassType())   et = t->toClassType()->getElementN(inds[0]);
		else if(t->isTupleType())   et = t->toTupleType()->getElementN(inds[0]);
		else if(t->isArrayType())   et = t->toArrayType()->getElementType();

		iceAssert(et);

		int ofs = 0;
		if(t->isClassType()) ofs = 1;   //! to account for vtable

		std::vector<Value*> args = { val };
		for(auto id : inds)
			args.push_back(fir::ConstantInt::getInt64(id + ofs));


		// note: no sideeffects, since we return a new aggregate
		Instruction* instr = new Instruction(OpKind::Value_ExtractValue, false, this->currentBlock, et, args);
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::InsertValueByName(Value* val, std::string n, Value* elm, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType())
			error("val is not an aggregate type with named members (class or struct) (have '%s')", t);

		size_t ind = 0;
		if(t->isStructType())       ind = t->toStructType()->getElementIndex(n);
		else if(t->isClassType())   ind = t->toClassType()->getElementIndex(n);
		else                        iceAssert(0);

		return this->InsertValue(val, { ind }, elm, vname);
	}

	Value* IRBuilder::ExtractValueByName(Value* val, std::string n, std::string vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType())
			error("val is not an aggregate type with named members (class or struct) (have '%s')", t);


		size_t ind = 0;
		if(t->isStructType())       ind = t->toStructType()->getElementIndex(n);
		else if(t->isClassType())   ind = t->toClassType()->getElementIndex(n);
		else                        iceAssert(0);

		return this->ExtractValue(val, { ind }, vname);
	}




	Value* IRBuilder::GetStringData(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetData, false, this->currentBlock,
			fir::Type::getChar()->getMutablePointerTo(), { str });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetStringData(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		else if(!val->getType()->isPointerType())
			error("val is not a pointer type");

		if(val->getType()->getPointerElementType() != fir::Type::getChar())
			error("val is not a char*");

		Instruction* instr = new Instruction(OpKind::String_SetData, true, this->currentBlock, fir::Type::getString(), { str, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::GetStringLength(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { str });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetStringLength(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetLength, true, this->currentBlock, fir::Type::getString(), { str, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::GetStringCapacity(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetCapacity, false, this->currentBlock,
			fir::Type::getInt64(), { str });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetStringCapacity(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetCapacity, true, this->currentBlock, fir::Type::getString(), { str, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetStringRefCountPointer(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetRefCountPtr, false, this->currentBlock,
			fir::Type::getInt64Ptr(), { str });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetStringRefCountPointer(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt64Ptr())
			error("val is not an int64*");

		Instruction* instr = new Instruction(OpKind::String_SetRefCountPtr, true, this->currentBlock,
			fir::Type::getString(), { str, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetStringRefCount(Value* str, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		Instruction* instr = new Instruction(OpKind::String_GetRefCount, false, this->currentBlock,
			fir::Type::getInt64(), { str });

		return this->addInstruction(instr, vname);
	}

	void IRBuilder::SetStringRefCount(Value* str, Value* val, std::string vname)
	{
		if(!str->getType()->isStringType())
			error("str is not a string type (got '%s')", str->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::String_SetRefCount, true, this->currentBlock,
			fir::Type::getVoid(), { str, val });

		this->addInstruction(instr, vname);
	}














	Value* IRBuilder::GetDynamicArrayData(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetData, false, this->currentBlock,
			arr->getType()->toDynamicArrayType()->getElementType()->getMutablePointerTo(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetDynamicArrayData(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		auto t = arr->getType()->toDynamicArrayType()->getElementType();
		if(val->getType() != t->getMutablePointerTo())
		{
			error("val is not a pointer to elm type (need '%s', have '%s')",
				t->getMutablePointerTo(), val->getType());
		}

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetData, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetDynamicArrayLength(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetDynamicArrayLength(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetLength, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetDynamicArrayCapacity(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetCapacity, false, this->currentBlock,
			fir::Type::getInt64(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetDynamicArrayCapacity(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetCapacity, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetDynamicArrayRefCountPointer(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetRefCountPtr, false, this->currentBlock,
			fir::Type::getInt64Ptr(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetDynamicArrayRefCountPointer(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64()->getPointerTo())
			error("val is not an int64 pointer");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetRefCountPtr, true, this->currentBlock,
			arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetDynamicArrayRefCount(Value* arr, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		Instruction* instr = new Instruction(OpKind::DynamicArray_GetRefCount, false, this->currentBlock,
			fir::Type::getInt64(), { arr });

		return this->addInstruction(instr, vname);
	}

	void IRBuilder::SetDynamicArrayRefCount(Value* arr, Value* val, std::string vname)
	{
		if(!arr->getType()->isDynamicArrayType())
			error("arr is not a dynamic array type (got '%s')", arr->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::DynamicArray_SetRefCount, true, this->currentBlock,
			fir::Type::getVoid(), { arr, val });

		this->addInstruction(instr, vname);
	}




	Value* IRBuilder::GetSAAData(Value* saa, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->GetStringData(saa, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->GetDynamicArrayData(saa, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::GetSAALength(Value* saa, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->GetStringLength(saa, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->GetDynamicArrayLength(saa, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::GetSAACapacity(Value* saa, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->GetStringCapacity(saa, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->GetDynamicArrayCapacity(saa, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::GetSAARefCountPointer(Value* saa, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->GetStringRefCountPointer(saa, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->GetDynamicArrayRefCountPointer(saa, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::GetSAARefCount(Value* saa, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->GetStringRefCount(saa, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->GetDynamicArrayRefCount(saa, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}




	Value* IRBuilder::SetSAAData(Value* saa, Value* val, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->SetStringData(saa, val, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->SetDynamicArrayData(saa, val, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::SetSAALength(Value* saa, Value* val, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->SetStringLength(saa, val, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->SetDynamicArrayLength(saa, val, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::SetSAACapacity(Value* saa, Value* val, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->SetStringCapacity(saa, val, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->SetDynamicArrayCapacity(saa, val, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	Value* IRBuilder::SetSAARefCountPointer(Value* saa, Value* val, std::string vname)
	{
		if(saa->getType()->isStringType())              return this->SetStringRefCountPointer(saa, val, vname);
		else if(saa->getType()->isDynamicArrayType())   return this->SetDynamicArrayRefCountPointer(saa, val, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());
	}

	void IRBuilder::SetSAARefCount(Value* saa, Value* val, std::string vname)
	{
		if(saa->getType()->isStringType())              this->SetStringRefCount(saa, val, vname);
		else if(saa->getType()->isDynamicArrayType())   this->SetDynamicArrayRefCount(saa, val, vname);
		else                                            error("'%s' is not an SAA type", saa->getType());

		return;
	}






















	Value* IRBuilder::GetArraySliceData(Value* slc, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		auto st = slc->getType()->toArraySliceType();
		auto et = st->getElementType();

		Instruction* instr = new Instruction(OpKind::ArraySlice_GetData, false, this->currentBlock,
			st->isMutable() ? et->getMutablePointerTo() : et->getPointerTo(), { slc });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetArraySliceData(Value* slc, Value* val, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		auto st = slc->getType()->toArraySliceType();
		auto et = st->getElementType();
		auto pt = (st->isMutable() ? et->getMutablePointerTo() : et->getPointerTo());

		if(val->getType() != pt)
			error("val is not a pointer to elm type (need '%s', have '%s')", pt, val->getType());

		Instruction* instr = new Instruction(OpKind::ArraySlice_SetData, true, this->currentBlock,
			slc->getType(), { slc, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::GetArraySliceLength(Value* slc, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		Instruction* instr = new Instruction(OpKind::ArraySlice_GetLength, false, this->currentBlock,
			fir::Type::getInt64(), { slc });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetArraySliceLength(Value* slc, Value* val, std::string vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("slc is not an array slice type (got '%s')", slc->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::ArraySlice_SetLength, true, this->currentBlock,
			slc->getType(), { slc, val });

		return this->addInstruction(instr, vname);
	}








	Value* IRBuilder::GetAnyTypeID(Value* any, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		Instruction* instr = new Instruction(OpKind::Any_GetTypeID, false, this->currentBlock, fir::Type::getInt64(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetAnyTypeID(Value* any, Value* val, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		if(val->getType() != fir::Type::getInt64())
			error("val is not an int64");

		Instruction* instr = new Instruction(OpKind::Any_SetTypeID, true, this->currentBlock, fir::Type::getVoid(), { any, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetAnyFlag(Value* any, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		Instruction* instr = new Instruction(OpKind::Any_GetFlag, false, this->currentBlock, fir::Type::getInt64(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetAnyFlag(Value* any, Value* val, std::string vname)
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
	Value* IRBuilder::GetAnyData(Value* any, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		Instruction* instr = new Instruction(OpKind::Any_GetData, false, this->currentBlock, fir::Type::getInt8()->getPointerTo(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetAnyData(Value* any, Value* val, std::string vname)
	{
		if(!any->getType()->isPointerType() || !any->getType()->getPointerElementType()->isAnyType())
			error("any is not a pointer to an any type (got '%s')", any->getType());

		iceAssert(this->context);
		iceAssert(this->context->module);

		size_t sz = this->context->module->getExecutionTarget()->getTypeSizeInBytes(val->getType());
		if(sz > 24 || sz == (size_t) -1)
		{
			error("Type '%s' cannot be stored directly in 'any', size is too large (max 24 bytes, have %zd bytes)",
				val->getType(), (int64_t) sz);
		}

		Instruction* instr = new Instruction(OpKind::Any_SetData, true, this->currentBlock, fir::Type::getVoid(), { any, val });

		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::GetRangeLower(Value* range, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (have '%s')", range->getType());

		Instruction* instr = new Instruction(OpKind::Range_GetLower, false, this->currentBlock,
			fir::Type::getInt64(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetRangeLower(Value* range, Value* val, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("val is not an integer type (got '%s')", val->getType());

		Instruction* instr = new Instruction(OpKind::Range_SetLower, true, this->currentBlock,
			fir::Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetRangeUpper(Value* range, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (have '%s')", range->getType());

		Instruction* instr = new Instruction(OpKind::Range_GetUpper, false, this->currentBlock,
			fir::Type::getInt64(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetRangeUpper(Value* range, Value* val, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("val is not an integer type (got '%s')", val->getType());

		Instruction* instr = new Instruction(OpKind::Range_SetUpper, true, this->currentBlock,
			fir::Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetRangeStep(Value* range, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (have '%s')", range->getType());

		Instruction* instr = new Instruction(OpKind::Range_GetStep, false, this->currentBlock,
			fir::Type::getInt64(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetRangeStep(Value* range, Value* val, std::string vname)
	{
		if(!range->getType()->isRangeType())
			error("range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("val is not an integer type (got '%s')", val->getType());

		Instruction* instr = new Instruction(OpKind::Range_SetStep, true, this->currentBlock,
			fir::Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}




	Value* IRBuilder::GetEnumCaseIndex(Value* ecs, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		Instruction* instr = new Instruction(OpKind::Enum_GetIndex, true, this->currentBlock,
			fir::Type::getInt64(), { ecs });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetEnumCaseIndex(Value* ecs, Value* idx, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		if(!idx->getType()->isIntegerType())
			error("index is not an integer type (got '%s')", idx->getType());

		Instruction* instr = new Instruction(OpKind::Enum_SetIndex, true, this->currentBlock,
			ecs->getType(), { ecs, idx });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetEnumCaseValue(Value* ecs, std::string vname)
	{
		if(!ecs->getType()->isEnumType())
			error("enum is not an enum type (got '%s')", ecs->getType());

		Instruction* instr = new Instruction(OpKind::Enum_GetValue, true, this->currentBlock,
			ecs->getType()->toEnumType()->getCaseType(), { ecs });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetEnumCaseValue(Value* ecs, Value* val, std::string vname)
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














	void IRBuilder::Unreachable()
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




