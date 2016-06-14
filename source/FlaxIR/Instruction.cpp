// Instruction.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/block.h"
#include "ir/function.h"
#include "ir/constant.h"
#include "ir/instruction.h"

namespace fir
{

	Instruction::Instruction(OpKind kind, Type* out, std::deque<Value*> vals) : Value(out)
	{
		this->opKind = kind;
		this->operands = vals;

		this->realOutput = new Value(out);

		for(auto v : vals)
			v->addUser(this);
	}

	Value* Instruction::getResult()
	{
		if(this->realOutput) return this->realOutput;
		iceAssert(0 && "Calling getActualValue() when not in function! (no real value)");
	}

	void Instruction::setValue(Value* v)
	{
		this->realOutput = v;
	}

	void Instruction::clearValue()
	{
		this->realOutput = 0;
	}

	std::string Instruction::str()
	{
		std::string name;
		switch(this->opKind)
		{
			case OpKind::Signed_Add: 						name = "sadd"; break;
			case OpKind::Signed_Sub: 						name = "ssub"; break;
			case OpKind::Signed_Mul: 						name = "smul"; break;
			case OpKind::Signed_Div: 						name = "sdiv"; break;
			case OpKind::Signed_Mod: 						name = "srem"; break;
			case OpKind::Signed_Neg: 						name = "neg"; break;
			case OpKind::Unsigned_Add: 						name = "uadd"; break;
			case OpKind::Unsigned_Sub: 						name = "usub"; break;
			case OpKind::Unsigned_Mul: 						name = "umul"; break;
			case OpKind::Unsigned_Div: 						name = "udiv"; break;
			case OpKind::Unsigned_Mod: 						name = "urem"; break;
			case OpKind::Floating_Add: 						name = "fadd"; break;
			case OpKind::Floating_Sub: 						name = "fsub"; break;
			case OpKind::Floating_Mul: 						name = "fmul"; break;
			case OpKind::Floating_Div: 						name = "fdiv"; break;
			case OpKind::Floating_Mod: 						name = "frem"; break;
			case OpKind::Floating_Neg: 						name = "fneg"; break;
			case OpKind::Floating_Truncate: 				name = "ftrunc"; break;
			case OpKind::Floating_Extend: 					name = "fext"; break;
			case OpKind::ICompare_Equal: 					name = "icmp eq"; break;
			case OpKind::ICompare_NotEqual: 				name = "icmp ne"; break;
			case OpKind::ICompare_Greater: 					name = "icmp gt"; break;
			case OpKind::ICompare_Less: 					name = "icmp lt"; break;
			case OpKind::ICompare_GreaterEqual: 			name = "icmp ge"; break;
			case OpKind::ICompare_LessEqual: 				name = "icmp le"; break;
			case OpKind::FCompare_Equal_ORD: 				name = "fcmp ord eq"; break;
			case OpKind::FCompare_Equal_UNORD: 				name = "fcmp unord eq"; break;
			case OpKind::FCompare_NotEqual_ORD: 			name = "fcmp ord ne"; break;
			case OpKind::FCompare_NotEqual_UNORD: 			name = "fcmp unord ne"; break;
			case OpKind::FCompare_Greater_ORD: 				name = "fcmp ord gt"; break;
			case OpKind::FCompare_Greater_UNORD: 			name = "fcmp unord gt"; break;
			case OpKind::FCompare_Less_ORD: 				name = "fcmp ord lt"; break;
			case OpKind::FCompare_Less_UNORD: 				name = "fcmp unord lt"; break;
			case OpKind::FCompare_GreaterEqual_ORD: 		name = "fcmp ord ge"; break;
			case OpKind::FCompare_GreaterEqual_UNORD: 		name = "fcmp unord ge"; break;
			case OpKind::FCompare_LessEqual_ORD: 			name = "fcmp ord le"; break;
			case OpKind::FCompare_LessEqual_UNORD: 			name = "fcmp unord le"; break;
			case OpKind::Bitwise_Not: 						name = "not"; break;
			case OpKind::Bitwise_Xor: 						name = "xor"; break;
			case OpKind::Bitwise_Arithmetic_Shr: 			name = "ashr"; break;
			case OpKind::Bitwise_Logical_Shr: 				name = "lshr"; break;
			case OpKind::Bitwise_Shl: 						name = "shl"; break;
			case OpKind::Bitwise_And: 						name = "and"; break;
			case OpKind::Bitwise_Or: 						name = "or"; break;
			case OpKind::Cast_Bitcast: 						name = "bitcast"; break;
			case OpKind::Cast_IntSize: 						name = "intszcast"; break;
			case OpKind::Cast_Signedness: 					name = "signedcast"; break;
			case OpKind::Cast_FloatToInt: 					name = "fptoint"; break;
			case OpKind::Cast_IntToFloat: 					name = "inttofp"; break;
			case OpKind::Cast_PointerType: 					name = "ptrcast"; break;
			case OpKind::Cast_PointerToInt: 				name = "ptrtoint"; break;
			case OpKind::Cast_IntToPointer: 				name = "inttoptr"; break;
			case OpKind::Cast_IntSignedness: 				name = "signcast"; break;
			case OpKind::Integer_ZeroExt: 					name = "izeroext"; break;
			case OpKind::Integer_Truncate: 					name = "itrunc"; break;
			case OpKind::Value_Store: 						name = "store"; break;
			case OpKind::Logical_Not: 						name = "logicalNot"; break;
			case OpKind::Value_Load: 						name = "load"; break;
			case OpKind::Value_StackAlloc: 					name = "stackAlloc"; break;
			case OpKind::Value_CallFunction: 				name = "call"; break;
			case OpKind::Value_Return: 						name = "ret"; break;
			case OpKind::Value_GetPointerToStructMember: 	name = "gep"; break;
			case OpKind::Value_GetStructMember: 			name = "gep"; break;
			case OpKind::Value_GetPointer: 					name = "gep"; break;
			case OpKind::Value_GetGEP2: 					name = "gep"; break;
			case OpKind::Branch_UnCond: 					name = "jump"; break;
			case OpKind::Branch_Cond: 						name = "branch"; break;
			case OpKind::Value_PointerAddition:				name = "ptradd"; break;
			case OpKind::Value_PointerSubtraction:			name = "ptrsub"; break;
			case OpKind::Invalid:							name = "unknown"; break;
		}

		std::string ops;
		for(auto op : this->operands)
		{
			bool didfn = false;
			if(op->getType()->isFunctionType())
			{
				ops += "@" + op->getName();
				if(this->opKind == OpKind::Value_CallFunction)
				{
					ops += ", (";
					didfn = true;
				}
			}
			else if(ConstantInt* ci = dynamic_cast<ConstantInt*>(op))
			{
				ops += std::to_string(ci->getSignedValue());
			}
			else if(ConstantFP* cf = dynamic_cast<ConstantFP*>(op))
			{
				ops += std::to_string(cf->getValue());
			}
			else if(dynamic_cast<ConstantValue*>(op))
			{
				ops += "(null %" + std::to_string(op->id) + " :: " + op->getType()->str() + ")";
			}
			else if(IRBlock* ib = dynamic_cast<IRBlock*>(op))
			{
				ops += "$" + ib->getName();
			}
			else
			{
				ops += "%" + std::to_string(op->id) + " :: " + op->getType()->str();
			}

			if(!didfn)
				ops += ", ";
		}

		if(ops.length() > 0)
			ops = ops.substr(0, ops.length() - 2);


		if(this->opKind == OpKind::Value_CallFunction)
			ops += ")";


		std::string ret = "";
		if(this->realOutput->getType()->isVoidType())
		{
			ret = name + " " + ops;
		}
		else
		{
			ret = "%" + std::to_string(this->realOutput->id) + " :: " + this->realOutput->getType()->str() + " = " + name + " " + ops;
		}

		return ret;
	}
}










































