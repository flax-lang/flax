// IRBuilder.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

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

		v->addUser(this->currentBlock);
		v->setName(vname);
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
		if(ao == Ast::ArithmeticOp::Add || ao == Ast::ArithmeticOp::PlusEquals)
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
		else if(ao == Ast::ArithmeticOp::Subtract || ao == Ast::ArithmeticOp::MinusEquals)
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
		else if(ao == Ast::ArithmeticOp::Multiply || ao == Ast::ArithmeticOp::MultiplyEquals)
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
		else if(ao == Ast::ArithmeticOp::Divide || ao == Ast::ArithmeticOp::DivideEquals)
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
		else if(ao == Ast::ArithmeticOp::Modulo || ao == Ast::ArithmeticOp::ModEquals)
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
		else if(ao == Ast::ArithmeticOp::ShiftLeft || ao == Ast::ArithmeticOp::ShiftLeftEquals)
		{
			if(useFloating) iceAssert("shift operation can only be done with ints");
			op = OpKind::Bitwise_Shl;

			out = lhs;
		}
		else if(ao == Ast::ArithmeticOp::ShiftRight || ao == Ast::ArithmeticOp::ShiftRightEquals)
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

		return new Instruction(op, out, { vlhs, vrhs });
	}

	Value* IRBuilder::CreateBinaryOp(Ast::ArithmeticOp ao, Value* a, Value* b, std::string vname)
	{
		Instruction* instr = Instruction::GetBinaryOpInstruction(ao, a, b);
		if(instr == 0) return 0;

		return this->addInstruction(instr, vname);
	}































	Value* IRBuilder::CreateNeg(Value* a, std::string vname)
	{
		iceAssert(a->getType()->toPrimitiveType() && "cannot negate non-primitive type");
		iceAssert((a->getType()->isFloatingPointType() || a->getType()->toPrimitiveType()->isSigned()) && "cannot negate unsigned type");

		Instruction* instr = new Instruction(a->getType()->isFloatingPointType() ? OpKind::Floating_Neg : OpKind::Signed_Neg,
			a->getType(), { a });

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




		#if 0
		{
			ConstantInt* cia = dynamic_cast<ConstantInt*>(a);
			ConstantInt* cib = dynamic_cast<ConstantInt*>(b);

			ConstantFP* cfa = dynamic_cast<ConstantFP*>(a);
			ConstantFP* cfb = dynamic_cast<ConstantFP*>(b);

			Type* out = 0;
			if(cia && cib)
			{
				if(cia->getType()->toPrimitiveType()->getIntegerBitWidth()
					> cib->getType()->toPrimitiveType()->getIntegerBitWidth())
				{
					out = cia->getType();
				}
				else
				{
					out = cib->getType();
				}
			}
			else if(cfa && cfb)
			{
				if(cfa->getType()->toPrimitiveType()->getFloatingPointBitWidth()
					> cfb->getType()->toPrimitiveType()->getFloatingPointBitWidth())
				{
					out = cfa->getType();
				}
				else
				{
					out = cfb->getType();
				}
			}
			else
			{
				if(cfa) out = cfa->getType();
				else if(cfb) out = cfb->getType();
			}

			if(out)
			{
				if(cia && cib)
				{
					if(out->isSignedIntType())
					{
						return ConstantInt::getSigned(out, cia->getSignedValue() + cib->getSignedValue());
					}
					else
					{
						return ConstantInt::getUnsigned(out, cia->getUnsignedValue() + cib->getUnsignedValue());
					}
				}
				else if(cfa && cfb)
				{
				}
				else if(cia && cfb)
				{
				}
				else if(cfa && cib)
				{
				}
			}
		}
		#endif


		Instruction* instr = new Instruction(ok, a->getType(), { a, b });
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

		Instruction* instr = new Instruction(ok, a->getType(), { a, b });
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

		Instruction* instr = new Instruction(ok, a->getType(), { a, b });
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

		Instruction* instr = new Instruction(ok, a->getType(), { a, b });
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

		Instruction* instr = new Instruction(ok, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFTruncate(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Truncate, targetType, { v });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFExtend(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = new Instruction(OpKind::Floating_Extend, targetType, { v });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateICmpEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Equal, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpNEQ(Value* a, Value* b, std::string vname)
	{
		if(a->getType() != b->getType())
			error("creating cmp neq instruction with non-equal types (%s vs %s)", a->getType()->str().c_str(), b->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::ICompare_NotEqual, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpGT(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Greater, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpLT(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_Less, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpGEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_GreaterEqual, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateICmpLEQ(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::ICompare_LessEqual, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_ord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_ORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_uord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Equal_UNORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpNEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_ord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_ORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpNEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_uord instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_NotEqual_UNORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGT_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater_ORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGT_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Greater_UNORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLT_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less_ORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLT_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_Less_UNORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_GreaterEqual_ORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpGEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_GreaterEqual_UNORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLEQ_ORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual_ORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFCmpLEQ_UNORD(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::FCompare_LessEqual_UNORD, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateLogicalAND(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating logical and instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Logical_And, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateLogicalOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating logical or instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Logical_Or, fir::PrimitiveType::getBool(this->context), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseXOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise xor instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Xor, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseLogicalSHR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise lshl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Logical_Shr, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseArithmeticSHR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise ashl instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Arithmetic_Shr, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseSHL(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise shr instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Shl, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseAND(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise and instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_And, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseOR(Value* a, Value* b, std::string vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise or instruction with non-equal types");
		Instruction* instr = new Instruction(OpKind::Bitwise_Or, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitwiseNOT(Value* a, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Bitwise_Not, a->getType(), { a });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateBitcast(Value* v, Type* targetType, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Cast_Bitcast, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntSizeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntSize, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateFloatToIntCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && "value is not floating point type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_FloatToInt, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntToFloatCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isFloatingPointType() && "target is not floating point type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToFloat, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerTypeCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerType, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerToIntCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = new Instruction(OpKind::Cast_PointerToInt, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateIntToPointerCast(Value* v, Type* targetType, std::string vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = new Instruction(OpKind::Cast_IntToPointer, targetType, { v, ConstantValue::getNullValue(targetType) });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreateLoad(Value* ptr, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got %s)", ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_Load, ptr->getType()->getPointerElementType(), { ptr });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateStore(Value* v, Value* ptr)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not pointer type (got %s)", ptr->getType()->str().c_str());

		if(v->getType()->getPointerTo() != ptr->getType())
			error("ptr is not a pointer to type of value (storing %s into %s)", v->getType()->str().c_str(), ptr->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_Store, PrimitiveType::getVoid(), { v, ptr });
		return this->addInstruction(instr, "");
	}

	Value* IRBuilder::CreateCall0(Function* fn, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { fn });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall1(Function* fn, Value* p1, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { fn, p1 });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall2(Function* fn, Value* p1, Value* p2, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { fn, p1, p2 });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall3(Function* fn, Value* p1, Value* p2, Value* p3, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), { fn, p1, p2, p3 });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::deque<Value*> args, std::string vname)
	{
		auto v = args;
		args.push_front(fn);

		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), v);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::vector<Value*> args, std::string vname)
	{
		std::deque<Value*> dargs;
		dargs.push_back(fn);

		for(auto a : args)
			dargs.push_back(a);

		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), dargs);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateCall(Function* fn, std::initializer_list<Value*> args, std::string vname)
	{
		std::deque<Value*> dargs;
		dargs.push_back(fn);

		for(auto a : args)
			dargs.push_back(a);

		Instruction* instr = new Instruction(OpKind::Value_CallFunction, fn->getType()->getReturnType(), dargs);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateReturn(Value* v)
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, PrimitiveType::getVoid(), { v });
		return this->addInstruction(instr, "");
	}

	Value* IRBuilder::CreateReturnVoid()
	{
		Instruction* instr = new Instruction(OpKind::Value_Return, PrimitiveType::getVoid(), { });
		return this->addInstruction(instr, "");
	}


	Value* IRBuilder::CreateLogicalNot(Value* v, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Logical_Not, PrimitiveType::getBool(), { v });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateStackAlloc(Type* type, std::string vname)
	{
		Instruction* instr = new Instruction(OpKind::Value_StackAlloc, type->getPointerTo(), { ConstantValue::getNullValue(type) });
		return this->addInstruction(instr, vname);
	}

	void IRBuilder::CreateCondBranch(Value* condition, IRBlock* trueB, IRBlock* falseB)
	{
		Instruction* instr = new Instruction(OpKind::Branch_Cond, PrimitiveType::getVoid(), { condition, trueB, falseB });
		this->addInstruction(instr, "");
	}

	void IRBuilder::CreateUnCondBranch(IRBlock* target)
	{
		Instruction* instr = new Instruction(OpKind::Branch_UnCond, PrimitiveType::getVoid(), { target });
		this->addInstruction(instr, "");
	}




	// gep stuff


	// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
	Value* IRBuilder::CreateGetPointerToStructMember(Value* ptr, Value* ptrIndex, Value* memberIndex, std::string vname)
	{
		error("enotsup");
	}

	Value* IRBuilder::CreateGetPointerToConstStructMember(Value* ptr, Value* ptrIndex, size_t memberIndex, std::string vname)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not a pointer");
		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not an integer type");

		if(StructType* st = dynamic_cast<StructType*>(ptr->getType()->getPointerElementType()))
		{
			iceAssert(st->getElementCount() > memberIndex && "struct does not have so many members");

			Instruction* instr = new Instruction(OpKind::Value_GetPointerToStructMember, st->getElementN(memberIndex)->getPointerTo(),
				{ ptr, ptrIndex, ConstantInt::getUint64(memberIndex) });

			return this->addInstruction(instr, vname);
		}
		else
		{
			error("type %s is not a valid type to GEP into", ptr->getType()->getPointerElementType()->str().c_str());
		}
	}


	// equivalent to CreateStructGEP()
	Value* IRBuilder::CreateStructGEP(Value* structPtr, size_t memberIndex, std::string vname)
	{
		iceAssert(structPtr->getType()->isPointerType() && "ptr is not a pointer");

		if(StructType* st = dynamic_cast<StructType*>(structPtr->getType()->getPointerElementType()))
		{
			iceAssert(st->getElementCount() > memberIndex && "struct does not have so many members");

			Instruction* instr = new Instruction(OpKind::Value_GetStructMember, st->getElementN(memberIndex)->getPointerTo(),
				{ structPtr, ConstantInt::getUint64(memberIndex) });

			return this->addInstruction(instr, vname);
		}
		else if(LLVariableArrayType* llat = dynamic_cast<LLVariableArrayType*>(structPtr->getType()->getPointerElementType()))
		{
			iceAssert(memberIndex <= 1 && "LLVariableArrayType only has 2 members");

			Type* ty = (memberIndex == 0 ? llat->getElementType()->getPointerTo() : PrimitiveType::getInt64());

			Instruction* instr = new Instruction(OpKind::Value_GetStructMember, ty->getPointerTo(),
				{ structPtr, ConstantInt::getUint64(memberIndex) });

			return this->addInstruction(instr, vname);
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

		Instruction* instr = new Instruction(OpKind::Value_GetGEP2, retType, { ptr, ptrIndex, elmIndex });
		return this->addInstruction(instr, vname);
	}

	// equivalent to GEP(ptr*, index)
	Value* IRBuilder::CreateGetPointer(Value* ptr, Value* ptrIndex, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		if(!ptrIndex->getType()->isIntegerType())
			error("ptrIndex is not an integer type (got %s)", ptrIndex->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_GetPointer, ptr->getType(), { ptr, ptrIndex });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CreatePointerAdd(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got %s)", num->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_PointerAddition, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreatePointerSub(Value* ptr, Value* num, std::string vname)
	{
		if(!ptr->getType()->isPointerType())
			error("ptr is not a pointer type (got %s)", ptr->getType()->str().c_str());

		if(!num->getType()->isIntegerType())
			error("num is not an integer type (got %s)", num->getType()->str().c_str());

		Instruction* instr = new Instruction(OpKind::Value_PointerAddition, ptr->getType(), { ptr, num });
		return this->addInstruction(instr, vname);
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




