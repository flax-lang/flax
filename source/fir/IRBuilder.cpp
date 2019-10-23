// IRBuilder.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include <cmath>

#include "ast.h"
#include "gluecode.h"

#include "ir/block.h"
#include "ir/irbuilder.h"
#include "ir/instruction.h"

#include "memorypool.h"


static bool isSAAType(fir::Type* t)
{
	return t->isStringType() || t->isDynamicArrayType();
}

static fir::Type* getSAAElmType(fir::Type* t)
{
	iceAssert(isSAAType(t));

	if(t->isStringType())   return fir::Type::getInt8();
	else                    return t->getArrayElementType();
}


namespace fir
{
	IRBuilder::IRBuilder(Module* mod)
	{
		this->module = mod;
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

	static util::MemoryPool<Instruction> instr_pool(65536);
	static Instruction* make_instr(OpKind kind, bool sideEffects, Type* out, const std::vector<Value*>& vals,
		Value::Kind k = Value::Kind::prvalue)
	{
		return instr_pool.construct(kind, sideEffects, out, vals, k);
	}

	Value* IRBuilder::addInstruction(Instruction* instr, const std::string& vname)
	{
		iceAssert(this->currentBlock && "no current block");

		// add instruction to the end of the block
		this->currentBlock->instructions.push_back(instr);
		Value* v = instr->realOutput;

		// v->addUser(this->currentBlock);
		v->setName(vname);
		return v;
	}

	static Instruction* getBinaryOpInstruction(const std::string& ao, Value* vlhs, Value* vrhs)
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
					if(lpt->isSigned() && rpt->isSigned())  out = lpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned())  out = rpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
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
					if(lpt->isSigned() && rpt->isSigned())  out = lpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned())  out = rpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
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
					if(lpt->isSigned() && rpt->isSigned())  out = lpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned())  out = rpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
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
					if(lpt->isSigned() && rpt->isSigned())  out = lpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned())  out = rpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
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
					if(lpt->isSigned() && rpt->isSigned())  out = lpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
				}
				else
				{
					if(lpt->isSigned() && rpt->isSigned())  out = rpt;
					else                                    out = (lpt->isSigned() ? rpt : lpt);
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

		return make_instr(op, false, out, { vlhs, vrhs });
	}

	Value* IRBuilder::BinaryOp(const std::string& ao, Value* a, Value* b, const std::string& vname)
	{
		Instruction* instr = getBinaryOpInstruction(ao, a, b);
		if(instr == 0) return 0;

		return this->addInstruction(instr, vname);
	}































	Value* IRBuilder::Negate(Value* a, const std::string& vname)
	{
		iceAssert(a->getType()->toPrimitiveType() && "cannot negate non-primitive type");
		iceAssert((a->getType()->isFloatingPointType() || a->getType()->toPrimitiveType()->isSigned()) && "cannot negate unsigned type");

		Instruction* instr = make_instr(a->getType()->isFloatingPointType() ? OpKind::Floating_Neg : OpKind::Signed_Neg,
			false, a->getType(), { a });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Add(Value* a, Value* b, const std::string& vname)
	{
		if(a->getType() != b->getType())
			error("irbuilder: creating add instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Add;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Add;
		else ok = OpKind::Floating_Add;


		Instruction* instr = make_instr(ok, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Subtract(Value* a, Value* b, const std::string& vname)
	{
		if(a->getType() != b->getType())
			error("irbuilder: creating sub instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Sub;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Sub;
		else ok = OpKind::Floating_Sub;

		Instruction* instr = make_instr(ok, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Multiply(Value* a, Value* b, const std::string& vname)
	{
		if(a->getType() != b->getType())
			error("irbuilder: creating mul instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Mul;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Mul;
		else ok = OpKind::Floating_Mul;

		Instruction* instr = make_instr(ok, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Divide(Value* a, Value* b, const std::string& vname)
	{
		if(a->getType() != b->getType())
			error("irbuilder: creating div instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());


		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Div;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Div;
		else ok = OpKind::Floating_Div;

		Instruction* instr = make_instr(ok, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Modulo(Value* a, Value* b, const std::string& vname)
	{
		if(a->getType() != b->getType())
			error("irbuilder: creating mod instruction with non-equal types ('%s' vs '%s')", a->getType(), b->getType());

		OpKind ok = OpKind::Invalid;
		if(a->getType()->isSignedIntType()) ok = OpKind::Signed_Mod;
		else if(a->getType()->isIntegerType()) ok = OpKind::Unsigned_Mod;
		else ok = OpKind::Floating_Mod;

		Instruction* instr = make_instr(ok, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FTruncate(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = make_instr(OpKind::Floating_Truncate, false, targetType,
			util::vectorOf<Value*>(v, ConstantValue::getZeroValue(targetType))
		);

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FExtend(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && targetType->isFloatingPointType() && "not floating point type");
		Instruction* instr = make_instr(OpKind::Floating_Extend, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::ICmpEQ(Value* a, Value* b, const std::string& vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("irbuilder: creating icmp eq instruction with non-equal types");
		}

		Instruction* instr = make_instr(OpKind::ICompare_Equal, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpNEQ(Value* a, Value* b, const std::string& vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("irbuilder: creating icmp neq instruction with non-equal types");
		}

		Instruction* instr = make_instr(OpKind::ICompare_NotEqual, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpGT(Value* a, Value* b, const std::string& vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("irbuilder: creating icmp gt instruction with non-equal types");
		}

		Instruction* instr = make_instr(OpKind::ICompare_Greater, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpLT(Value* a, Value* b, const std::string& vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("irbuilder: creating icmp lt instruction with non-equal types");
		}

		Instruction* instr = make_instr(OpKind::ICompare_Less, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpGEQ(Value* a, Value* b, const std::string& vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("irbuilder: creating icmp geq instruction with non-equal types");
		}

		Instruction* instr = make_instr(OpKind::ICompare_GreaterEqual, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::ICmpLEQ(Value* a, Value* b, const std::string& vname)
	{
		//* note: allows comparing mutable and immutable pointers.
		if(a->getType() != b->getType() && !(a->getType()->isPointerType() && b->getType()->isPointerType()
			&& a->getType()->getPointerElementType() == b->getType()->getPointerElementType()))
		{
			error("irbuilder: creating icmp leq instruction with non-equal types");
		}

		Instruction* instr = make_instr(OpKind::ICompare_LessEqual, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::FCmpEQ_ORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_ord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Equal_ORD, false, Type::getBool(),
			{ a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpEQ_UNORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp eq_uord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Equal_UNORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpNEQ_ORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_ord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_NotEqual_ORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpNEQ_UNORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp neq_uord instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_NotEqual_UNORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGT_ORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Greater_ORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGT_UNORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp gt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Greater_UNORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLT_ORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Less_ORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLT_UNORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp lt instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Less_UNORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGEQ_ORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_GreaterEqual_ORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpGEQ_UNORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp geq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_GreaterEqual_UNORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLEQ_ORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_LessEqual_ORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpLEQ_UNORD(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_LessEqual_UNORD, false, Type::getBool(), { a, b });
		return this->addInstruction(instr, vname);
	}


	// returns -1 for a < b, 0 for a == b, 1 for a > b
	Value* IRBuilder::ICmpMulti(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating icmp multi instruction with non-equal types");
		// iceAssert(a->getType()->isIntegerType() && "creating icmp multi instruction with non-integer type");
		Instruction* instr = make_instr(OpKind::ICompare_Multi, false, Type::getNativeWord(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FCmpMulti(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating cmp leq instruction with non-equal types");
		iceAssert(a->getType()->isFloatingPointType() && "creating fcmp instruction with non floating-point types");
		Instruction* instr = make_instr(OpKind::FCompare_Multi, false, Type::getNativeWord(), { a, b });
		return this->addInstruction(instr, vname);
	}










	Value* IRBuilder::BitwiseXOR(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise xor instruction with non-equal types");
		Instruction* instr = make_instr(OpKind::Bitwise_Xor, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseLogicalSHR(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise lshl instruction with non-equal types");
		Instruction* instr = make_instr(OpKind::Bitwise_Logical_Shr, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseArithmeticSHR(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise ashl instruction with non-equal types");
		Instruction* instr = make_instr(OpKind::Bitwise_Arithmetic_Shr, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseSHL(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise shr instruction with non-equal types");
		Instruction* instr = make_instr(OpKind::Bitwise_Shl, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseAND(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise and instruction with non-equal types");
		Instruction* instr = make_instr(OpKind::Bitwise_And, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseOR(Value* a, Value* b, const std::string& vname)
	{
		iceAssert(a->getType() == b->getType() && "creating bitwise or instruction with non-equal types");
		Instruction* instr = make_instr(OpKind::Bitwise_Or, false, a->getType(), { a, b });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::BitwiseNOT(Value* a, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Bitwise_Not, false, a->getType(), { a });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Bitcast(Value* v, Type* targetType, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Cast_Bitcast, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntSizeCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert((v->getType()->isIntegerType() || v->getType()->isBoolType()) && "value is not integer type");
		iceAssert((targetType->isIntegerType() || targetType->isBoolType()) && "target is not integer type");

		// make constant result for constant operand
		if(ConstantInt* ci = dcast(ConstantInt, v))
		{
			return ConstantInt::get(targetType, ci->getSignedValue());
		}

		Instruction* instr = make_instr(OpKind::Cast_IntSize, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntSignednessCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		// make constant result for constant operand
		if(ConstantInt* ci = dcast(ConstantInt, v))
		{
			if(ci->getType()->isSignedIntType())
				return ConstantInt::get(targetType, ci->getSignedValue());

			else
				return ConstantInt::get(targetType, ci->getUnsignedValue());
		}

		Instruction* instr = make_instr(OpKind::Cast_IntSignedness, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::FloatToIntCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isFloatingPointType() && "value is not floating point type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		// make constant result for constant operand
		if(ConstantFP* cfp = dcast(ConstantFP, v))
		{
			double _ = 0;

			if(std::modf(cfp->getValue(), &_) != 0.0)
				warn("truncating constant '%f' in constant cast to type '%s'", cfp->getValue(), targetType);

			return ConstantInt::get(targetType, static_cast<size_t>(cfp->getValue()));
		}

		Instruction* instr = make_instr(OpKind::Cast_FloatToInt, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntToFloatCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isFloatingPointType() && "target is not floating point type");

		// make constant result for constant operand
		if(ConstantInt* ci = dcast(ConstantInt, v))
		{
			ConstantFP* ret = 0;
			bool sgn = ci->getType()->isSignedIntType();
			if(targetType == Type::getFloat32())
			{
				if(sgn)	ret = ConstantFP::getFloat32(static_cast<float>(ci->getSignedValue()));
				else	ret = ConstantFP::getFloat32(static_cast<float>(ci->getUnsignedValue()));
			}
			else if(targetType == Type::getFloat64())
			{
				if(sgn)	ret = ConstantFP::getFloat64(static_cast<double>(ci->getSignedValue()));
				else	ret = ConstantFP::getFloat64(static_cast<double>(ci->getUnsignedValue()));
			}
			else
			{
				error("irbuilder: unknown floating point type '%s'", targetType);
			}

			return ret;
		}

		Instruction* instr = make_instr(OpKind::Cast_IntToFloat, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::PointerTypeCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert((v->getType()->isPointerType() || v->getType()->isNullType()) && "value is not pointer type");
		iceAssert((targetType->isPointerType() || targetType->isNullType()) && "target is not pointer type");

		Instruction* instr = make_instr(OpKind::Cast_PointerType, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::PointerToIntCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isPointerType() && "value is not pointer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = make_instr(OpKind::Cast_PointerToInt, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntToPointerCast(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isPointerType() && "target is not pointer type");

		Instruction* instr = make_instr(OpKind::Cast_IntToPointer, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::IntTruncate(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert(v->getType()->isIntegerType() && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = make_instr(OpKind::Integer_Truncate, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::IntZeroExt(Value* v, Type* targetType, const std::string& vname)
	{
		iceAssert((v->getType()->isIntegerType() || v->getType()->isBoolType()) && "value is not integer type");
		iceAssert(targetType->isIntegerType() && "target is not integer type");

		Instruction* instr = make_instr(OpKind::Integer_ZeroExt, false, targetType,
			{ v, ConstantValue::getZeroValue(targetType) });
		return this->addInstruction(instr, vname);
	}





	Value* IRBuilder::AppropriateCast(Value* v, Type* r, const std::string& vname)
	{
		auto l = v->getType();

		if(l->isIntegerType() && r->isIntegerType())
			return this->IntSizeCast(v, r, vname);

		else if(l->isFloatingPointType() && r->isFloatingPointType())
			return (l->getBitWidth() > r->getBitWidth() ? this->FTruncate(v, r, vname) : this->FExtend(v, r, vname));

		else if(l->isIntegerType() && r->isFloatingPointType())
			return this->IntToFloatCast(v, r, vname);

		else if(l->isFloatingPointType() && r->isIntegerType())
			return this->FloatToIntCast(v, r, vname);

		else if(l->isIntegerType() && r->isPointerType())
			return this->IntToPointerCast(v, r, vname);

		else if(l->isPointerType() && r->isIntegerType())
			return this->PointerToIntCast(v, r, vname);

		else if(l->isPointerType() && r->isPointerType())
			return this->PointerTypeCast(v, r, vname);

		// nope.
		return 0;
	}











	Value* IRBuilder::Call(Function* fn, const std::string& vname)
	{
		return this->Call(fn, { }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, const std::string& vname)
	{
		return this->Call(fn, { p1 }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, Value* p2, const std::string& vname)
	{
		return this->Call(fn, { p1, p2 }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, Value* p2, Value* p3, const std::string& vname)
	{
		return this->Call(fn, { p1, p2, p3 }, vname);
	}

	Value* IRBuilder::Call(Function* fn, Value* p1, Value* p2, Value* p3, Value* p4, const std::string& vname)
	{
		return this->Call(fn, { p1, p2, p3, p4 }, vname);
	}


	Value* IRBuilder::Call(Function* fn, const std::vector<Value*>& args, const std::string& vname)
	{
		if(args.size() != fn->getArgumentCount() && !fn->isVariadic() && !fn->isCStyleVarArg())
		{
			error("irbuilder: calling function '%s' with the wrong number of arguments (needs %d, have %d)", fn->getName().str(),
				fn->getArgumentCount(), args.size());
		}

		auto autocastStuff = [this](Value* arg, Type* target) -> Value* {

			auto isSlice = [](Type* ty) -> bool {
				return ty->isArraySliceType();
			};

			auto isVariadicSlice = [&isSlice](Type* ty) -> bool {
				return isSlice(ty) && ty->toArraySliceType()->isVariadicType();
			};

			auto getSliceElm = [](Type* ty) -> Type* {
				return ty->getArrayElementType();
			};

			auto isSliceMut = [&isSlice](Type* ty) -> bool {
				return isSlice(ty) && ty->toArraySliceType()->isMutable();
			};

			auto at = arg->getType();
			if((isSlice(at) && isSlice(target) && (isVariadicSlice(at) != isVariadicSlice(target)))
				|| (isSlice(at) && isSlice(target)
					&& getSliceElm(at) == getSliceElm(target)
					&& isSliceMut(at) && !isSliceMut(target)
				))
			{
				// silently cast, because they're the same thing
				// the distinction is solely for the type system's benefit
				return this->Bitcast(arg, target);
			}
			else if(at->isPointerType() && target->isPointerType() && at->getPointerElementType() == target->getPointerElementType() &&
				at->isMutablePointer() && target->isImmutablePointer())
			{
				// this is ok. at the llvm level the cast should reduce to a no-op.
				return this->PointerTypeCast(arg, target);
			}
			else
			{
				return arg;
			}
		};

		std::vector<Value*> out;
		out.reserve(args.size());

		bool forwarded = false;
		std::vector<Value*> variadicArgs;

		auto numArgs = fn->getArgumentCount();
		for(size_t i = 0; i < args.size(); i++)
		{
			auto at = args[i]->getType();

			if(i < (fn->isVariadic() ? numArgs - 1 : numArgs))
			{
				auto target = fn->getArguments()[i]->getType();
				out.push_back(autocastStuff(args[i], target));

				if(out[i]->getType() != target)
				{
					error("irbuilder: mismatch in argument type (arg. %d) in function '%s' (need '%s', have '%s')", i, fn->getName().str(),
						fn->getArguments()[i]->getType(), out[i]->getType());
				}
			}
			else if(fn->isVariadic())
			{
				iceAssert(fn->getArguments().back()->getType()->isVariadicArrayType());
				auto elm = fn->getArguments().back()->getType()->getArrayElementType();

				if(at->isArraySliceType() && at->getArrayElementType() == elm)
				{
					forwarded = true;
					out.push_back(args[i]);
				}
				else if(args[i]->getType() != elm)
				{
					error("irbuilder: mismatch in argument type (in variadic portion) (arg. %d) in function '%s' (need '%s', have '%s')",
						i, fn->getName().str(), elm, args[i]->getType());
				}
				else
				{
					// handle it later, lol.
					variadicArgs.push_back(autocastStuff(args[i], elm));
				}
			}
			else if(fn->isCStyleVarArg())
			{
				// auto-convert strings and char slices into char* when passing to va_args
				if(at->isStringType())
					out.push_back(this->GetSAAData(args[i]));

				else if(at->isCharSliceType())
					out.push_back(this->GetArraySliceData(args[i]));

				else
					out.push_back(args[i]);
			}
			else
			{
				// shouldn't happen -- we should've errored out earlier.
				iceAssert(0);
			}
		}

		if(variadicArgs.size() > 0 && !forwarded)
		{
			iceAssert(fn->isVariadic());
			iceAssert(fn->getArguments().back()->getType()->isVariadicArrayType());
			auto elm = fn->getArguments().back()->getType()->getArrayElementType();

			//? so the strat here is to stack-allocate an array, so we get a pointer to the array,
			//? with which we can use GEP instructions to store things inside.

			auto arrty = ArrayType::get(elm, variadicArgs.size());
			auto arrptr = this->StackAlloc(arrty);

			for(size_t i = 0; i < variadicArgs.size(); i++)
				this->WritePtr(variadicArgs[i], this->ConstGEP2(arrptr, 0, i));

			// then we make a slice out of it
			auto slcty = ArraySliceType::getVariadic(elm);
			auto slc = this->CreateValue(slcty);

			// ugh, fix mutability cast.
			slc = this->SetArraySliceData(slc, this->PointerTypeCast(this->ConstGEP2(arrptr, 0, 0), elm->getPointerTo()));
			slc = this->SetArraySliceLength(slc, ConstantInt::getNative(variadicArgs.size()));

			// ok, this is the last argument.
			out.push_back(slc);
		}
		else if(fn->isVariadic() && variadicArgs.empty() && !forwarded)
		{
			// ok, insert the empty slice here.
			auto elm = fn->getArguments().back()->getType()->getArrayElementType();
			out.push_back(ConstantArraySlice::get(ArraySliceType::getVariadic(elm),
				ConstantValue::getZeroValue(elm->getPointerTo()), ConstantInt::getNative(0)));
		}

		out.insert(out.begin(), fn);

		Instruction* instr = make_instr(OpKind::Value_CallFunction, true, fn->getType()->getReturnType(), out);
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::Call(Function* fn, const std::initializer_list<Value*>& args, const std::string& vname)
	{
		return this->Call(fn, std::vector<Value*>(args.begin(), args.end()), vname);
	}





	Value* IRBuilder::CallToFunctionPointer(Value* fn, FunctionType* ft, const std::vector<Value*>& args, const std::string& vname)
	{
		//* note: we're using our operator overload here for T + VEC<T>
		auto out = fn + args;

		Instruction* instr = make_instr(OpKind::Value_CallFunctionPointer, true, ft->getReturnType(), out);
		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::CallVirtualMethod(ClassType* cls, FunctionType* ft, size_t index, const std::vector<Value*>& args, const std::string& vname)
	{
		// args[0] must be the self, for obvious reasons.
		auto ty = args[0]->getType();
		iceAssert(ty->isPointerType() && ty->getPointerElementType()->isClassType());

		auto self = ty->getPointerElementType()->toClassType();
		iceAssert(self && self == cls);

		Instruction* instr = make_instr(OpKind::Value_CallVirtualMethod, true, ft->getReturnType(),
			util::vectorOf<Value*>(
				ConstantValue::getZeroValue(cls),
				ConstantInt::getNative(index),
				ConstantValue::getZeroValue(ft)
			) + args
		);

		return this->addInstruction(instr, vname);
	}




















	Value* IRBuilder::Return(Value* v)
	{
		Instruction* instr = make_instr(OpKind::Value_Return, true, Type::getVoid(), { v });
		return this->addInstruction(instr, "");
	}

	Value* IRBuilder::ReturnVoid()
	{
		Instruction* instr = make_instr(OpKind::Value_Return, true, Type::getVoid(), { });
		return this->addInstruction(instr, "");
	}


	Value* IRBuilder::LogicalNot(Value* v, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Logical_Not, false, Type::getBool(), { v });
		return this->addInstruction(instr, vname);
	}


	PHINode* IRBuilder::CreatePHINode(Type* type, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Value_CreatePHI, false, type->getPointerTo(),
			{ ConstantValue::getZeroValue(type) });

		// we need to 'lift' (hoist) the allocation up to make it the first in the block
		// this is an llvm requirement.

		// MEMORY LEAK
		instr->realOutput = new PHINode(type);
		Value* ret = instr->realOutput;

		ret->setName(vname);

		// insert at the front (back = no guarantees)
		this->currentBlock->instructions.insert(this->currentBlock->instructions.begin(), instr);
		return dcast(PHINode, instr->realOutput);
	}

	Value* IRBuilder::StackAlloc(Type* type, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Value_StackAlloc, false, type->getMutablePointerTo(),
			{ ConstantValue::getZeroValue(type) });

		// we need to 'lift' (hoist) the allocation up to the entry block of the function
		// this prevents allocation inside loops eating stack memory forever

		Value* ret = instr->realOutput;
		ret->setName(vname);

		// get the parent function
		auto parent = this->currentBlock->getParentFunction();
		iceAssert(parent);

		parent->addStackAllocation(type);


		// get the entry block
		auto entry = parent->getBlockList().front();
		iceAssert(entry);

		// insert at the front (back = no guarantees)
		entry->instructions.insert(entry->instructions.begin(), instr);

		return ret;
	}

	Value* IRBuilder::ImmutStackAlloc(Type* type, Value* v, const std::string& vname)
	{
		Value* ret = this->StackAlloc(type, vname);
		this->WritePtr(v, ret);

		// now make it immutable.
		ret->setType(type->getPointerTo());
		return ret;
	}


	Value* IRBuilder::CreateSliceFromSAA(Value* saa, bool mut, const std::string& vname)
	{
		if(!isSAAType(saa->getType()))
			error("irbuilder: expected string or dynamic array type, found '%s' instead", saa->getType());

		auto slc = this->CreateValue(saa->getType()->isStringType() ? Type::getCharSlice(mut)
			: ArraySliceType::get(saa->getType()->getArrayElementType(), mut));

		auto slcelmty = slc->getType()->getArrayElementType();

		slc = this->SetArraySliceData(slc, this->PointerTypeCast(this->GetSAAData(saa), mut ? slcelmty->getMutablePointerTo()
			: slcelmty->getPointerTo()));

		slc = this->SetArraySliceLength(slc, this->GetSAALength(saa));
		slc->setName(vname);

		return slc;
	}


	void IRBuilder::CondBranch(Value* condition, IRBlock* trueB, IRBlock* falseB)
	{
		Instruction* instr = make_instr(OpKind::Branch_Cond, true, Type::getVoid(),
			{ condition, trueB, falseB });
		this->addInstruction(instr, "");
	}

	void IRBuilder::UnCondBranch(IRBlock* target)
	{
		Instruction* instr = make_instr(OpKind::Branch_UnCond, true, Type::getVoid(),
			{ target });
		this->addInstruction(instr, "");
	}


	Value* IRBuilder::GetRawUnionFieldByType(Value* lval, Type* type, const std::string& vname)
	{
		if(!lval->islvalue())
			error("irbuilder: cannot do raw union ops on non-lvalue");

		if(!lval->getType()->isRawUnionType())
			error("irbuilder: '%s' is not a raw union type!", lval->getType());

		Instruction* instr = make_instr(OpKind::RawUnion_GEP, false, type, { lval, ConstantValue::getZeroValue(type) });

		auto ret = this->addInstruction(instr, "");
		ret->setKind(lval->kind);
		ret->setName(vname);

		return ret;
	}

	Value* IRBuilder::GetRawUnionField(Value* lval, const std::string& field, const std::string& vname)
	{
		if(!lval->islvalue())
			error("irbuilder: cannot do raw union ops on non-lvalue");

		if(!lval->getType()->isRawUnionType())
			error("irbuilder: '%s' is not a raw union type!", lval->getType());

		auto rut = lval->getType()->toRawUnionType();
		if(!rut->hasVariant(field))
			error("irbuilder: union '%s' does not have a field '%s'", rut->getTypeName(), field);

		auto ty = rut->getVariant(field);
		return this->GetRawUnionFieldByType(lval, ty, vname);
	}



	template <typename T>
	static Instruction* doGEPOnCompoundType(T* type, Value* structPtr, size_t memberIndex)
	{
		if(!structPtr->islvalue())
			error("irbuilder: cannot do GEP on non-lvalue");

		iceAssert(type->getElementCount() > memberIndex && "struct does not have so many members");

		Instruction* instr = make_instr(OpKind::Value_GetStructMember, false, type->getElementN(memberIndex),
			{ structPtr, ConstantInt::getUNative(memberIndex) }, Value::Kind::lvalue);

		return instr;
	}




	Value* IRBuilder::StructGEP(Value* structPtr, size_t memberIndex, const std::string& vname)
	{
		if(!structPtr->islvalue())
			error("irbuilder: cannot do GEP on non-lvalue");

		if(structPtr->getType()->isStructType())
		{
			auto st = structPtr->getType()->toStructType();
			return this->addInstruction(doGEPOnCompoundType(st,	structPtr, memberIndex), vname);
		}
		else if(structPtr->getType()->isTupleType())
		{
			auto tt = structPtr->getType()->toTupleType();
			return this->addInstruction(doGEPOnCompoundType(tt, structPtr, memberIndex), vname);
		}
		else if(structPtr->getType()->isClassType())
		{
			error("irbuilder: classes do not support element access by index");

			// auto ct = structPtr->getType()->toClassType();
			// // to compensate for the vtable, we add one to the index if there is a vtable!
			// if(ct->getVirtualMethodCount() > 0)
			// 	memberIndex += 1;

			// return this->addInstruction(doGEPOnCompoundType(ct,
			// 	structPtr, memberIndex), vname);
		}
		else
		{
			error("irbuilder: type '%s' is not a valid type to GEP into", structPtr->getType());
		}
	}

	Value* IRBuilder::GetStructMember(Value* ptr, const std::string& memberName)
	{
		if(!ptr->islvalue())
			error("irbuilder: cannot do GEP on non-lvalue");

		if(ptr->getType()->isStructType())
		{
			auto st = ptr->getType()->toStructType();
			auto memt = st->getElement(memberName);

			iceAssert(st->hasElementWithName(memberName) && "no element with such name");

			Instruction* instr = make_instr(OpKind::Value_GetStructMember, false, memt,
				{ ptr, ConstantInt::getUNative(st->getElementIndex(memberName)) }, Value::Kind::lvalue);

			return this->addInstruction(instr, memberName);
		}
		else if(ptr->getType()->isClassType())
		{
			auto ct = ptr->getType()->toClassType();

			iceAssert(ct->hasElementWithName(memberName) && "no element with such name");
			auto memt = ct->getElement(memberName);

			//! VTABLE HANDLING
			size_t vTableOfs = 0;
			if(ct->getVirtualMethodCount() > 0)
				vTableOfs = 1;

			Instruction* instr = make_instr(OpKind::Value_GetStructMember, false, memt,
				{ ptr, ConstantInt::getUNative(ct->getAbsoluteElementIndex(memberName) + vTableOfs) }, Value::Kind::lvalue);

			return this->addInstruction(instr, memberName);
		}
		else
		{
			error("irbuilder: type '%s' is not a valid type to GEP into", ptr->getType());
		}
	}



	void IRBuilder::SetVtable(Value* ptr, Value* table)
	{
		if(!ptr->islvalue())
			error("irbuilder: cannot do set vtable on non-lvalue");

		auto ty = ptr->getType();
		if(!ty->isClassType()) error("irbuilder: '%s' is not a class type", ty);
		if(table->getType() != Type::getInt8Ptr()) error("irbuilder: expected i8* for vtable, got '%s'", table->getType());

		Instruction* instr = make_instr(OpKind::Value_GetStructMember, false, Type::getInt8Ptr(), { ptr, ConstantInt::getUNative(0) }, Value::Kind::lvalue);

		auto gep = this->addInstruction(instr, "__vtable");
		this->Store(table, gep);
	}




	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::ConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex, const std::string& vname)
	{
		if(!ptr->getType()->isPointerType())
			error("irbuilder: ptr is not a pointer type (got '%s')", ptr->getType());

		auto ptri = ConstantInt::getUNative(ptrIndex);
		auto elmi = ConstantInt::getUNative(elmIndex);

		return this->GEP2(ptr, ptri, elmi, vname);
	}

	// equivalent to GEP(ptr*, ptrIndex, elmIndex)
	Value* IRBuilder::GEP2(Value* ptr, Value* ptrIndex, Value* elmIndex, const std::string& vname)
	{
		if(!ptr->getType()->isPointerType())
			error("irbuilder: ptr is not a pointer type (got '%s')", ptr->getType());

		else if(ptr->getType()->getPointerElementType()->isClassType() || ptr->getType()->getPointerElementType()->isStructType())
			error("irbuilder: use the other function for struct types");

		iceAssert(ptrIndex->getType()->isIntegerType() && "ptrIndex is not integer type");
		iceAssert(elmIndex->getType()->isIntegerType() && "elmIndex is not integer type");

		Type* retType = ptr->getType()->getPointerElementType();
		if(retType->isArrayType())
			retType = retType->toArrayType()->getElementType()->getPointerTo();

		if(ptr->getType()->isMutablePointer())
			retType = retType->getMutablePointerVersion();

		Instruction* instr = make_instr(OpKind::Value_GetGEP2, false, retType, { ptr, ptrIndex, elmIndex });

		return this->addInstruction(instr, vname);
	}

	// equivalent to GEP(ptr*, index)
	Value* IRBuilder::GetPointer(Value* ptr, Value* ptrIndex, const std::string& vname)
	{
		if(!ptr->getType()->isPointerType())
			error("irbuilder: ptr is not a pointer type (got '%s')", ptr->getType());

		if(!ptrIndex->getType()->isIntegerType())
			error("irbuilder: ptrIndex is not an integer type (got '%s')", ptrIndex->getType());

		if(ptr->getType()->getPointerElementType()->isClassType() || ptr->getType()->getPointerElementType()->isStructType())
			error("irbuilder: use the other function for struct types");

		Instruction* instr = make_instr(OpKind::Value_GetPointer, false, ptr->getType(), { ptr, ptrIndex });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::Select(Value* cond, Value* one, Value* two, const std::string& vname)
	{
		if(!cond->getType()->isBoolType())
			error("irbuilder: cond is not a boolean type (got '%s')", cond->getType());

		if(one->getType() != two->getType())
			error("irbuilder: non-identical types for operands (got '%s' and '%s')", one->getType(), two->getType());

		Instruction* instr = make_instr(OpKind::Value_Select, false, one->getType(), { cond, one, two });
		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::Sizeof(Type* t, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Misc_Sizeof, false, Type::getNativeWord(),
			{ ConstantValue::getZeroValue(t) });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::CreateValue(Type* t, const std::string& vname)
	{
		auto ret = ConstantValue::getZeroValue(t);
		ret->setName(vname);

		return ret;
	}








	static Instruction* _insertValue(Value* val, size_t idx, Type* et, Value* elm)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("irbuilder: val is not an aggregate type (have '%s')", t);

		if(elm->getType() != et)
		{
			error("irbuilder: mismatched types for value and element -- trying to insert '%s' into '%s'",
				elm->getType(), et);
		}

		int ofs = 0;
		//! VTABLE HANDLING
		if(t->isClassType() && t->toClassType()->getVirtualMethodCount() > 0)
			ofs = 1;

		std::vector<Value*> args = { val, elm, ConstantInt::getNative(idx + ofs) };

		// note: no sideeffects, since we return a new aggregate
		return make_instr(OpKind::Value_InsertValue, false, t, args);
	}

	static Instruction* _extractValue(Value* val, size_t idx, Type* et)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("irbuilder: val is not an aggregate type (have '%s')", t);

		int ofs = 0;
		//! VTABLE HANDLING
		if(t->isClassType() && t->toClassType()->getVirtualMethodCount() > 0)
			ofs = 1;

		std::vector<Value*> args = { val, ConstantInt::getNative(idx + ofs) };

		// note: no sideeffects, since we return a new aggregate
		return make_instr(OpKind::Value_ExtractValue, false, et, args);
	}






	Value* IRBuilder::InsertValue(Value* val, const std::vector<size_t>& inds, Value* elm, const std::string& vname)
	{
		Type* t = val->getType();
		if(t->isClassType())
			error("irbuilder: classes do not support element access by index");

		if(!t->isStructType() && !t->isTupleType() && !t->isArrayType())
			error("irbuilder: val is not a supported aggregate type (have '%s')", t);

		if(inds.size() != 1)
			error("irbuilder: must have exactly one index!");


		Type* et = 0;
		if(t->isStructType())       et = t->toStructType()->getElementN(inds[0]);
		else if(t->isTupleType())   et = t->toTupleType()->getElementN(inds[0]);
		else if(t->isArrayType())   et = t->toArrayType()->getElementType();

		iceAssert(et);
		return this->addInstruction(_insertValue(val, inds[0], et, elm), vname);
	}

	Value* IRBuilder::ExtractValue(Value* val, const std::vector<size_t>& inds, const std::string& vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType() && !t->isTupleType() && !t->isArrayType())
			error("irbuilder: val is not an aggregate type (have '%s')", t);

		if(inds.size() != 1)
			error("irbuilder: must have exactly one index!");

		if(t->isClassType())
			error("irbuilder: classes do not support element access by index");

		Type* et = 0;
		if(t->isStructType())       et = t->toStructType()->getElementN(inds[0]);
		else if(t->isTupleType())   et = t->toTupleType()->getElementN(inds[0]);
		else if(t->isArrayType())   et = t->toArrayType()->getElementType();

		iceAssert(et);
		return this->addInstruction(_extractValue(val, inds[0], et), vname);
	}


	Value* IRBuilder::InsertValueByName(Value* val, const std::string& n, Value* elm, const std::string& vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType())
			error("irbuilder: val is not an aggregate type with named members (class or struct) (have '%s')", t);

		size_t ind = 0;
		Type* et = 0;
		if(t->isStructType())       ind = t->toStructType()->getElementIndex(n), et = t->toStructType()->getElement(n);
		else if(t->isClassType())   ind = t->toClassType()->getAbsoluteElementIndex(n), et = t->toClassType()->getElement(n);
		else                        iceAssert(0);

		return this->addInstruction(_insertValue(val, ind, et, elm), vname);
	}

	Value* IRBuilder::ExtractValueByName(Value* val, const std::string& n, const std::string& vname)
	{
		Type* t = val->getType();
		if(!t->isStructType() && !t->isClassType())
			error("irbuilder: val is not an aggregate type with named members (class or struct) (have '%s')", t);

		size_t ind = 0;
		Type* et = 0;
		if(t->isStructType())       ind = t->toStructType()->getElementIndex(n), et = t->toStructType()->getElement(n);
		else if(t->isClassType())   ind = t->toClassType()->getAbsoluteElementIndex(n), et = t->toClassType()->getElement(n);
		else                        iceAssert(0);

		return this->addInstruction(_extractValue(val, ind, et), vname);
	}












	Value* IRBuilder::GetSAAData(Value* arr, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		Instruction* instr = make_instr(OpKind::SAA_GetData, false, getSAAElmType(arr->getType())->getMutablePointerTo(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetSAAData(Value* arr, Value* val, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		auto t = getSAAElmType(arr->getType());
		if(val->getType() != t->getMutablePointerTo())
		{
			error("irbuilder: val is not a pointer to elm type (need '%s', have '%s')",
				t->getMutablePointerTo(), val->getType());
		}

		Instruction* instr = make_instr(OpKind::SAA_SetData, true, arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetSAALength(Value* arr, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		Instruction* instr = make_instr(OpKind::SAA_GetLength, false, Type::getNativeWord(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetSAALength(Value* arr, Value* val, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		if(val->getType() != Type::getNativeWord())
			error("irbuilder: val is not an int64");

		Instruction* instr = make_instr(OpKind::SAA_SetLength, true, arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetSAACapacity(Value* arr, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		Instruction* instr = make_instr(OpKind::SAA_GetCapacity, false, Type::getNativeWord(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetSAACapacity(Value* arr, Value* val, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		if(val->getType() != Type::getNativeWord())
			error("irbuilder: val is not an int64");

		Instruction* instr = make_instr(OpKind::SAA_SetCapacity, true, arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetSAARefCountPointer(Value* arr, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		Instruction* instr = make_instr(OpKind::SAA_GetRefCountPtr, false, Type::getNativeWordPtr(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetSAARefCountPointer(Value* arr, Value* val, const std::string& vname)
	{
		if(!isSAAType(arr->getType()))
			error("irbuilder: thing is not an SAA type (got '%s')", arr->getType());

		if(val->getType() != Type::getNativeWord()->getPointerTo())
			error("irbuilder: val is not an int64 pointer");

		Instruction* instr = make_instr(OpKind::SAA_SetRefCountPtr, true, arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetSAARefCount(Value* arr, const std::string& vname)
	{
		return this->ReadPtr(this->GetSAARefCountPointer(arr), vname);
	}

	void IRBuilder::SetSAARefCount(Value* arr, Value* val)
	{
		if(val->getType() != Type::getNativeWord())
			error("irbuilder: val is not an int64");

		this->WritePtr(val, this->PointerTypeCast(this->GetSAARefCountPointer(arr), Type::getNativeWordPtr()->getMutablePointerVersion()));
	}






















	Value* IRBuilder::GetArraySliceData(Value* slc, const std::string& vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("irbuilder: slc is not an array slice type (got '%s')", slc->getType());

		auto st = slc->getType()->toArraySliceType();
		auto et = st->getElementType();

		Instruction* instr = make_instr(OpKind::ArraySlice_GetData, false, st->isMutable() ? et->getMutablePointerTo() : et->getPointerTo(), { slc });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetArraySliceData(Value* slc, Value* val, const std::string& vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("irbuilder: slc is not an array slice type (got '%s')", slc->getType());

		auto st = slc->getType()->toArraySliceType();
		auto et = st->getElementType();
		auto pt = (st->isMutable() ? et->getMutablePointerTo() : et->getPointerTo());

		if(val->getType() != pt)
		{
			if(pt->getPointerElementType() != val->getType()->getPointerElementType() || (pt->isMutablePointer() && val->getType()->isImmutablePointer()))
				error("irbuilder: val is not a pointer to elm type (need '%s', have '%s')", pt, val->getType());
		}

		Instruction* instr = make_instr(OpKind::ArraySlice_SetData, true, slc->getType(), { slc, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::GetArraySliceLength(Value* slc, const std::string& vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("irbuilder: slc is not an array slice type (got '%s')", slc->getType());

		Instruction* instr = make_instr(OpKind::ArraySlice_GetLength, false, Type::getNativeWord(), { slc });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetArraySliceLength(Value* slc, Value* val, const std::string& vname)
	{
		if(!slc->getType()->isArraySliceType())
			error("irbuilder: slc is not an array slice type (got '%s')", slc->getType());

		if(val->getType() != Type::getNativeWord())
			error("irbuilder: val is not an int64");

		Instruction* instr = make_instr(OpKind::ArraySlice_SetLength, true, slc->getType(), { slc, val });

		return this->addInstruction(instr, vname);
	}








	Value* IRBuilder::GetAnyTypeID(Value* any, const std::string& vname)
	{
		if(!any->getType()->isAnyType())
			error("irbuilder: not any type (got '%s')", any->getType());

		Instruction* instr = make_instr(OpKind::Any_GetTypeID, false, Type::getNativeUWord(), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetAnyTypeID(Value* any, Value* val, const std::string& vname)
	{
		if(!any->getType()->isAnyType())
			error("irbuilder: not any type (got '%s')", any->getType());

		else if(val->getType() != Type::getNativeUWord())
			error("irbuilder: val is not a uint64");

		Instruction* instr = make_instr(OpKind::Any_SetTypeID, true, Type::getAny(), { any, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::GetAnyData(Value* any, const std::string& vname)
	{
		if(!any->getType()->isAnyType())
			error("irbuilder: not any type (got '%s')", any->getType());

		Instruction* instr = make_instr(OpKind::Any_GetData, false, ArrayType::get(Type::getInt8(),
			BUILTIN_ANY_DATA_BYTECOUNT), { any });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetAnyData(Value* any, Value* val, const std::string& vname)
	{
		if(!any->getType()->isAnyType())
			error("irbuilder: not any type (got '%s')", any->getType());

		else if(val->getType() != ArrayType::get(Type::getInt8(), BUILTIN_ANY_DATA_BYTECOUNT))
			error("irbuilder: val is not array type (got '%s')", val->getType());

		Instruction* instr = make_instr(OpKind::Any_SetData, true, Type::getAny(), { any, val });

		return this->addInstruction(instr, vname);
	}


	Value* IRBuilder::GetAnyRefCountPointer(Value* arr, const std::string& vname)
	{
		if(!arr->getType()->isAnyType())
			error("irbuilder: arr is not an any type (got '%s')", arr->getType());

		Instruction* instr = make_instr(OpKind::Any_GetRefCountPtr, false, Type::getNativeWordPtr(), { arr });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetAnyRefCountPointer(Value* arr, Value* val, const std::string& vname)
	{
		if(!arr->getType()->isAnyType())
			error("irbuilder: arr is not an any type (got '%s')", arr->getType());

		if(val->getType() != Type::getNativeWord()->getPointerTo())
			error("irbuilder: val is not an int64 pointer");

		Instruction* instr = make_instr(OpKind::Any_SetRefCountPtr, true, arr->getType(), { arr, val });

		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::GetAnyRefCount(Value* arr, const std::string& vname)
	{
		return this->ReadPtr(this->GetAnyRefCountPointer(arr), vname);
	}

	void IRBuilder::SetAnyRefCount(Value* arr, Value* val)
	{
		if(val->getType() != Type::getNativeWord())
			error("irbuilder: val is not an int64");

		this->WritePtr(val, this->PointerTypeCast(this->GetAnyRefCountPointer(arr),
			Type::getNativeWordPtr()->getMutablePointerVersion()));
	}
















	Value* IRBuilder::GetRangeLower(Value* range, const std::string& vname)
	{
		if(!range->getType()->isRangeType())
			error("irbuilder: range is not a range type (have '%s')", range->getType());

		Instruction* instr = make_instr(OpKind::Range_GetLower, false, Type::getNativeWord(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetRangeLower(Value* range, Value* val, const std::string& vname)
	{
		if(!range->getType()->isRangeType())
			error("irbuilder: range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("irbuilder: val is not an integer type (got '%s')", val->getType());

		Instruction* instr = make_instr(OpKind::Range_SetLower, true, Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetRangeUpper(Value* range, const std::string& vname)
	{
		if(!range->getType()->isRangeType())
			error("irbuilder: range is not a range type (have '%s')", range->getType());

		Instruction* instr = make_instr(OpKind::Range_GetUpper, false, Type::getNativeWord(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetRangeUpper(Value* range, Value* val, const std::string& vname)
	{
		if(!range->getType()->isRangeType())
			error("irbuilder: range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("irbuilder: val is not an integer type (got '%s')", val->getType());

		Instruction* instr = make_instr(OpKind::Range_SetUpper, true, Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetRangeStep(Value* range, const std::string& vname)
	{
		if(!range->getType()->isRangeType())
			error("irbuilder: range is not a range type (have '%s')", range->getType());

		Instruction* instr = make_instr(OpKind::Range_GetStep, false, Type::getNativeWord(), { range });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetRangeStep(Value* range, Value* val, const std::string& vname)
	{
		if(!range->getType()->isRangeType())
			error("irbuilder: range is not a range type (got '%s')", range->getType());

		if(!val->getType()->isIntegerType())
			error("irbuilder: val is not an integer type (got '%s')", val->getType());

		Instruction* instr = make_instr(OpKind::Range_SetStep, true, Type::getRange(), { range, val });

		return this->addInstruction(instr, vname);
	}




	Value* IRBuilder::GetEnumCaseIndex(Value* ecs, const std::string& vname)
	{
		if(!ecs->getType()->isEnumType())
			error("irbuilder: enum is not an enum type (got '%s')", ecs->getType());

		Instruction* instr = make_instr(OpKind::Enum_GetIndex, true, Type::getNativeWord(), { ecs });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetEnumCaseIndex(Value* ecs, Value* idx, const std::string& vname)
	{
		if(!ecs->getType()->isEnumType())
			error("irbuilder: enum is not an enum type (got '%s')", ecs->getType());

		if(!idx->getType()->isIntegerType())
			error("irbuilder: index is not an integer type (got '%s')", idx->getType());

		Instruction* instr = make_instr(OpKind::Enum_SetIndex, true, ecs->getType(), { ecs, idx });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetEnumCaseValue(Value* ecs, const std::string& vname)
	{
		if(!ecs->getType()->isEnumType())
			error("irbuilder: enum is not an enum type (got '%s')", ecs->getType());

		Instruction* instr = make_instr(OpKind::Enum_GetValue, true, ecs->getType()->toEnumType()->getCaseType(), { ecs });

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetEnumCaseValue(Value* ecs, Value* val, const std::string& vname)
	{
		if(!ecs->getType()->isEnumType())
			error("irbuilder: enum is not an enum type (got '%s')", ecs->getType());

		if(ecs->getType()->toEnumType()->getCaseType() != val->getType())
		{
			error("irbuilder: value type mismatch (enum case type is '%s', value type is '%s'",
				ecs->getType()->toEnumType()->getCaseType(), val->getType());
		}

		Instruction* instr = make_instr(OpKind::Enum_SetValue, true, ecs->getType(), { ecs, val });

		return this->addInstruction(instr, vname);
	}







	Value* IRBuilder::ReadPtr(Value* ptr, const std::string& vname)
	{
		if(!ptr->getType()->isPointerType())
			error("irbuilder: ptr is not pointer type (got '%s')", ptr->getType());

		Instruction* instr = make_instr(OpKind::Value_ReadPtr, false, ptr->getType()->getPointerElementType(), { ptr });
		return this->addInstruction(instr, vname);
	}

	void IRBuilder::WritePtr(Value* v, Value* ptr)
	{
		if(!ptr->getType()->isPointerType())
			error("irbuilder: ptr is not pointer type (got '%s')", ptr->getType());

		if(ptr->getType()->isImmutablePointer())
			error("irbuilder: cannot store value to immutable pointer type '%s'", ptr->getType());

		auto vt = v->getType();
		auto pt = ptr->getType();

		if(vt != pt->getPointerElementType())
		{
			//* here, we know that the storage pointer is mutable. there's a special edge-case we need to catch:
			//* if we're storing a value of type &T to a & &mut T, or a &mut T to a & &T.
			//* in those cases, the mutability of the base type doesn't matter at all. At the LLVM level, we don't even make a distinction,
			//* so we can safely pass this onto the translation layer without worrying about it.

			// if((vt->isPointerType() && pt->isPointerType() && vt->getPointerElementType() == pt->getPointerElementType()) == false)
			error("irbuilder: ptr is not a pointer to type of value (base types '%s' -> '%s' differ)", vt, pt->getPointerElementType());
		}


		Instruction* instr = make_instr(OpKind::Value_WritePtr, true, Type::getVoid(), { v, ptr });
		this->addInstruction(instr, "");
	}


	static std::pair<Instruction*, IRBlock*> getInstrThatMakesLValue(IRBuilder* irb, Type* type, const std::string& vname)
	{
		Instruction* instr = make_instr(OpKind::Value_CreateLVal, true, type, { ConstantValue::getZeroValue(type) }, Value::Kind::lvalue);

		Value* ret = instr->realOutput;
		ret->setName(vname);

		// get the parent function
		auto parent = irb->getCurrentBlock()->getParentFunction();
		iceAssert(parent);

		parent->addStackAllocation(type);

		// get the entry block
		auto entry = parent->getBlockList().front();
		iceAssert(entry);

		return { instr, entry };
	}


	Value* IRBuilder::CreateLValue(Type* type, const std::string& vname)
	{
		auto [ instr, entry ] = getInstrThatMakesLValue(this, type, vname);

		// needs to be hoisted also
		entry->instructions.insert(entry->instructions.begin(), instr);

		return instr->realOutput;
	}

	void IRBuilder::Store(Value* val, Value* lval)
	{
		if(lval->isConst())
			error("irbuilder: cannot store to constant lvalue");

		else if(!lval->islvalue())
			error("irbuilder: cannot store to non-lvalue");

		else if(val->getType() != lval->getType())
			error("irbuilder: cannot store value of type '%s' to lvalue of type '%s'", val->getType(), lval->getType());

		// ok...
		Instruction* instr = make_instr(OpKind::Value_Store, true, Type::getVoid(), { val, lval });
		this->addInstruction(instr, "");
	}

	Value* IRBuilder::Dereference(Value* val, const std::string& vname)
	{
		if(!val->getType()->isPointerType())
			error("irbuilder: cannot dereference non-pointer type '%s'", val->getType());

		Instruction* instr = make_instr(OpKind::Value_Dereference, true, val->getType()->getPointerElementType(),
			{ val }, Value::Kind::lvalue);

		if(val->getType()->isImmutablePointer())
			instr->realOutput->makeConst();

		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::AddressOf(Value* lval, bool mut, const std::string& vname)
	{
		if(!lval->islvalue())
			error("irbuilder: cannot take the address of a non-lvalue");

		// ok...
		Instruction* instr = make_instr(OpKind::Value_AddressOf, true,
			mut ? lval->getType()->getMutablePointerTo() : lval->getType()->getPointerTo(), { lval });
		return this->addInstruction(instr, vname);
	}



	Value* IRBuilder::SetUnionVariantData(Value* unn, size_t id, Value* data, const std::string& vname)
	{
		if(!unn->getType()->isUnionType())
			error("irbuilder: '%s' is not a union type", unn->getType());

		auto ut = unn->getType()->toUnionType();
		if(data->getType() != ut->getVariant(id)->getInteriorType())
			error("irbuilder: cannot store data '%s' into union variant '%s'", data->getType(), ut->getVariant(id)->getInteriorType());

		Instruction* instr = make_instr(OpKind::Union_SetValue, true, unn->getType(),
			{ unn, ConstantInt::getNative(id), data });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetUnionVariantData(Value* unn, size_t id, const std::string& vname)
	{
		if(!unn->getType()->isUnionType())
			error("irbuilder: '%s' is not a union type", unn->getType());

		auto ut = unn->getType()->toUnionType();

		Instruction* instr = make_instr(OpKind::Union_GetValue, true, ut->getVariant(id)->getInteriorType(),
			{ unn, ConstantInt::getNative(id) });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::GetUnionVariantID(Value* unn, const std::string& vname)
	{
		if(!unn->getType()->isUnionType())
			error("irbuilder: '%s' is not a union type", unn->getType());

		Instruction* instr = make_instr(OpKind::Union_GetVariantID, true, Type::getNativeWord(), { unn });
		return this->addInstruction(instr, vname);
	}

	Value* IRBuilder::SetUnionVariantID(Value* unn, size_t id, const std::string& vname)
	{
		if(!unn->getType()->isUnionType())
			error("irbuilder: '%s' is not a union type", unn->getType());

		Instruction* instr = make_instr(OpKind::Union_SetVariantID, true, unn->getType(),
			{ unn, ConstantInt::getNative(id) });

		return this->addInstruction(instr, vname);
	}





























	void IRBuilder::Unreachable()
	{
		this->addInstruction(make_instr(OpKind::Unreachable, true, Type::getVoid(), { }), "");
	}

	IRBlock* IRBuilder::addNewBlockInFunction(const std::string& name, Function* func)
	{
		IRBlock* block = new IRBlock(func);
		if(func != this->currentFunction)
		{
			// warn("changing current function in irbuilder (from %s to %s)",
			// 	(this->currentFunction ? this->currentFunction->getName().str() : "null"),
			// 	func->getName()
			// );

			this->currentFunction = block->parentFunction;
		}

		this->currentFunction->blocks.push_back(block);

		size_t cnt = 0;
		for(auto b : this->currentFunction->blocks)
			if(b->getName().str() == name) cnt++;

		block->setName(strprintf("%s%s", name, cnt > 0 ? strprintf(".%d", cnt) : ""));
		return block;
	}

	IRBlock* IRBuilder::addNewBlockAfter(const std::string& name, IRBlock* block)
	{
		IRBlock* nb = new IRBlock(block->parentFunction);
		if(nb->parentFunction != this->currentFunction)
		{
			// warn("changing current function in irbuilder (from %s to %s)",
			// 	(this->currentFunction ? this->currentFunction->getName().str() : "null"),
			// 	nb->parentFunction->getName()
			// );


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

				nb->setName(strprintf("%s%s", name, cnt > 0 ? strprintf(".%d", cnt) : ""));

				this->currentFunction->blocks.insert(this->currentFunction->blocks.begin() + i + 1, nb);
				return nb;
			}
		}

		iceAssert(0 && "no such block to insert after");
		nb->setName(name);
		return nb;
	}
}




