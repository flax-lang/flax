// interpreter.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "ir/value.h"
#include "ir/interp.h"
#include "ir/module.h"
#include "ir/function.h"
#include "ir/instruction.h"

#include "gluecode.h"

#define LARGE_DATA_SIZE 32

namespace fir {
namespace interp
{
	struct ir_saa
	{
		uint8_t* data;
		int64_t length;
		int64_t capacity;
		int64_t* refcount;
	};

	struct ir_slice
	{
		uint8_t* data;
		int64_t length;
	};

	struct ir_range
	{
		int64_t begin;
		int64_t end;
		int64_t step;
	};

	struct ir_union
	{
		int64_t variant;
		// more stuff.
	};

	struct ir_any
	{
		int64_t type_id;
		int64_t* refcount;
		uint8_t data[BUILTIN_ANY_DATA_BYTECOUNT];
	};






	template <typename T> static interp::Value makeValue(size_t id, Type* ty, const T& val);

	template <typename T>
	static interp::Value makeConstant(ConstantValue* c)
	{
		auto ty = c->getType();
		if(auto ci = dcast(ConstantInt, c))
		{
			// TODO: make this more robust, pretty sure it's wrong now.
			return makeValue(c->id, ty, ci->getSignedValue());
		}
		else if(auto cf = dcast(ConstantFP, c))
		{
			return makeValue(c->id, ty, cf->getValue());
		}
		else if(auto cb = dcast(ConstantBool, c))
		{
			return makeValue(c->id, ty, cb->getValue());
		}
		else if(auto cs = dcast(ConstantString, c))
		{
		}
		else if(auto cbc = dcast(ConstantBitcast, c))
		{
		}
		else if(auto ca = dcast(ConstantArray, c))
		{
		}
		else if(auto ct = dcast(ConstantTuple, c))
		{
		}
		else if(auto cec = dcast(ConstantEnumCase, c))
		{
		}
		else if(auto cas = dcast(ConstantArraySlice, c))
		{
		}
		else if(auto cda = dcast(ConstantDynamicArray, c))
		{
		}
		else if(auto fn = dcast(Function, c))
		{
		}
		else
		{
			error("interp: unsupported");
		}
	}

	InterpState::InterpState(Module* mod)
	{
		this->module = mod;

		for(const auto& g : mod->_getAllGlobals())
		{

		}
	}









	template <typename T>
	static interp::Value makeValue(size_t id, Type* ty, const T& val)
	{
		interp::Value ret;
		ret.id = id;
		ret.type = ty;
		ret.dataSize = sizeof(T);

		if(auto fsz = getSizeOfType(ty); fsz != sizeof(T))
			error("packing error of type '%s': predicted %d, actual %d!", ty, fsz, sizeof(T));

		memset(&ret.data[0], 0, 32);

		if(sizeof(T) > LARGE_DATA_SIZE)
		{
			ret.ptr = malloc(sizeof(T));
			memmove(ret.ptr, &T, sizeof(T));
		}
		else
		{
			memmove(&ret.data[0], &T, sizeof(T));
		}

		return ret;
	}

	template <typename T>
	static T getValue(interp::Value* v)
	{
		if(v->dataSize > LARGE_DATA_SIZE)
		{
			return *((T*) v->ptr);
		}
		else
		{
			return *((T*) &v->data[0]);
		}
	}


	static interp::Value cloneValue(size_t id, interp::Value* v)
	{
		interp::Value ret = *v;
		ret.id = id;

		return ret;
	}


	// this saves us a lot of copy/paste for the arithmetic ops.
	template <typename Functor>
	static interp::Value twoArgumentOp(const interp::Instruction& inst, fir::Type* ty, interp::Value* a, interp::Value* b, Functor op)
	{
		if(ty == Type::getInt8())           return makeValue(inst.result, ty, op(getValue<int8_t>(a), getValue<int8_t>(b)));
		else if(ty == Type::getInt16())     return makeValue(inst.result, ty, op(getValue<int16_t>(a), getValue<int16_t>(b)));
		else if(ty == Type::getInt32())     return makeValue(inst.result, ty, op(getValue<int32_t>(a), getValue<int32_t>(b)));
		else if(ty == Type::getInt64())     return makeValue(inst.result, ty, op(getValue<int64_t>(a), getValue<int64_t>(b)));
		else if(ty == Type::getUint8())     return makeValue(inst.result, ty, op(getValue<uint8_t>(a), getValue<uint8_t>(b)));
		else if(ty == Type::getUint16())    return makeValue(inst.result, ty, op(getValue<uint16_t>(a), getValue<uint16_t>(b)));
		else if(ty == Type::getUint32())    return makeValue(inst.result, ty, op(getValue<uint32_t>(a), getValue<uint32_t>(b)));
		else if(ty == Type::getUint64())    return makeValue(inst.result, ty, op(getValue<uint64_t>(a), getValue<uint64_t>(b)));
		else if(ty == Type::getFloat32())   return makeValue(inst.result, ty, op(getValue<float>(a), getValue<float>(b)));
		else if(ty == Type::getFloat64())   return makeValue(inst.result, ty, op(getValue<double>(a), getValue<double>(b)));
		else if(ty->isPointerType())        return makeValue(inst.result, ty, op(getValue<uintptr_t>(a), getValue<uintptr_t>(b)));
		else                                error("interp: unsupported type '%s' for arithmetic", ty);
	}

	template <typename Functor>
	static interp::Value oneArgumentOp(const interp::Instruction& inst, fir::Type* ty, interp::Value* a, Functor op)
	{
		if(ty == Type::getInt8())           return makeValue(inst.result, ty, op(getValue<int8_t>(a)));
		else if(ty == Type::getInt16())     return makeValue(inst.result, ty, op(getValue<int16_t>(a)));
		else if(ty == Type::getInt32())     return makeValue(inst.result, ty, op(getValue<int32_t>(a)));
		else if(ty == Type::getInt64())     return makeValue(inst.result, ty, op(getValue<int64_t>(a)));
		else if(ty == Type::getUint8())     return makeValue(inst.result, ty, op(getValue<uint8_t>(a)));
		else if(ty == Type::getUint16())    return makeValue(inst.result, ty, op(getValue<uint16_t>(a)));
		else if(ty == Type::getUint32())    return makeValue(inst.result, ty, op(getValue<uint32_t>(a)));
		else if(ty == Type::getUint64())    return makeValue(inst.result, ty, op(getValue<uint64_t>(a)));
		else if(ty == Type::getFloat32())   return makeValue(inst.result, ty, op(getValue<float>(a)));
		else if(ty == Type::getFloat64())   return makeValue(inst.result, ty, op(getValue<double>(a)));
		else if(ty->isPointerType())        return makeValue(inst.result, ty, op(getValue<uintptr_t>(a)));
		else                                error("interp: unsupported type '%s' for arithmetic", ty);
	}




	static void runInstruction(InterpState* is, const interp::Instruction& inst)
	{
		auto getArg = [is](const interp::Instruction& inst, size_t i) -> interp::Value* {
			return &is->values[inst.args[i]];
		};

		auto setRet = [is](const interp::Instruction& inst, const interp::Value& val) -> void {
			is->values[inst.result] = val;
		};

		const auto boolTy = Type::getBool();

		const auto i64Ty = Type::getInt64();


		auto ok = (OpKind) inst.opcode;
		switch(ok)
		{
			case OpKind::Signed_Add:
			case OpKind::Unsigned_Add:
			case OpKind::Floating_Add:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a + b;
				}));
				break;
			}

			case OpKind::Signed_Sub:
			case OpKind::Unsigned_Sub:
			case OpKind::Floating_Sub:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a - b;
				}));
				break;
			}

			case OpKind::Signed_Mul:
			case OpKind::Unsigned_Mul:
			case OpKind::Floating_Mul:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a * b;
				}));
				break;
			}

			case OpKind::Signed_Div:
			case OpKind::Unsigned_Div:
			case OpKind::Floating_Div:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a / b;
				}));
				break;
			}

			case OpKind::Signed_Mod:
			case OpKind::Unsigned_Mod:
			case OpKind::Floating_Mod:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a % b;
				}));
				break;
			}

			case OpKind::ICompare_Equal:
			case OpKind::FCompare_Equal_ORD:
			case OpKind::FCompare_Equal_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a == b;
				}));
				break;
			}

			case OpKind::ICompare_NotEqual:
			case OpKind::FCompare_NotEqual_ORD:
			case OpKind::FCompare_NotEqual_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a != b;
				}));
				break;
			}

			case OpKind::ICompare_Greater:
			case OpKind::FCompare_Greater_ORD:
			case OpKind::FCompare_Greater_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a > b;
				}));
				break;
			}

			case OpKind::ICompare_Less:
			case OpKind::FCompare_Less_ORD:
			case OpKind::FCompare_Less_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a < b;
				}));
				break;
			}


			case OpKind::ICompare_GreaterEqual:
			case OpKind::FCompare_GreaterEqual_ORD:
			case OpKind::FCompare_GreaterEqual_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a >= b;
				}));
				break;
			}

			case OpKind::ICompare_LessEqual:
			case OpKind::FCompare_LessEqual_ORD:
			case OpKind::FCompare_LessEqual_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a <= b;
				}));
				break;
			}

			case OpKind::ICompare_Multi:
			case OpKind::FCompare_Multi:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					if(a == b)  return 0;
					if(a > b)   return 1;
					else        return -1;
				}));
				break;
			}

			case OpKind::Bitwise_Xor:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a ^ b;
				}));
				break;
			}

			case OpKind::Bitwise_Logical_Shr:
			case OpKind::Bitwise_Arithmetic_Shr:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type->isIntegerType());
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a >> b;
				}));
				break;
			}

			case OpKind::Bitwise_Shl:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type->isIntegerType());
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a << b;
				}));
				break;
			}

			case OpKind::Bitwise_And:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a & b;
				}));
				break;
			}

			case OpKind::Bitwise_Or:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a->type, a, b, [](auto a, auto b) -> auto {
					return a | b;
				}));
				break;
			}

			case OpKind::Signed_Neg:
			case OpKind::Floating_Neg:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				setRet(inst, oneArgumentOp(inst, a->type, a, [](auto a) -> auto {
					return -a;
				}));
				break;
			}

			case OpKind::Bitwise_Not:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				setRet(inst, oneArgumentOp(inst, a->type, a, [](auto a) -> auto {
					return ~a;
				}));
				break;
			}

			case OpKind::Floating_Truncate:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);
				auto t = getArg(inst, 1)->type;

				interp::Value ret;
				if(a->type == Type::getFloat64() && t == Type::getFloat32())
					ret = makeValue(inst.result, a->type, (float) getValue<double>(a));

				else if(a->type == Type::getFloat32()) ret = makeValue(inst.result, a->type, (float) getValue<float>(a));
				else if(a->type == Type::getFloat64()) ret = makeValue(inst.result, a->type, (double) getValue<double>(a));
				else                                   error("interp: unsupported");

				setRet(inst, ret);
				break;
			}

			case OpKind::Floating_Extend:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);
				auto t = getArg(inst, 1)->type;

				interp::Value ret;
				if(a->type == Type::getFloat32() && t == Type::getFloat64())
					ret = makeValue(inst.result, a->type, (double) getValue<float>(a));

				else if(a->type == Type::getFloat32()) ret = makeValue(inst.result, a->type, (float) getValue<float>(a));
				else if(a->type == Type::getFloat64()) ret = makeValue(inst.result, a->type, (double) getValue<double>(a));
				else                                   error("interp: unsupported");

				setRet(inst, ret);
				break;
			}

			case OpKind::Integer_ZeroExt:
			case OpKind::Integer_Truncate:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto ot = a->type;
				auto tt = getArg(inst, 1)->type;

				interp::Value ret;
				if(ot == tt)                                                ret = cloneValue(inst.result, a);
				else if(ot == Type::getInt8() && tt == Type::getInt16())    ret = makeValue(inst.result, tt, (int16_t) getValue<int8_t>(a));
				else if(ot == Type::getInt8() && tt == Type::getInt32())    ret = makeValue(inst.result, tt, (int32_t) getValue<int8_t>(a));
				else if(ot == Type::getInt8() && tt == Type::getInt64())    ret = makeValue(inst.result, tt, (int64_t) getValue<int8_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt32())   ret = makeValue(inst.result, tt, (int32_t) getValue<int16_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt64())   ret = makeValue(inst.result, tt, (int64_t) getValue<int16_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt8())    ret = makeValue(inst.result, tt, (int8_t) getValue<int16_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt64())   ret = makeValue(inst.result, tt, (int64_t) getValue<int32_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt16())   ret = makeValue(inst.result, tt, (int16_t) getValue<int32_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt8())    ret = makeValue(inst.result, tt, (int8_t) getValue<int32_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt32())   ret = makeValue(inst.result, tt, (int32_t) getValue<int64_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt16())   ret = makeValue(inst.result, tt, (int16_t) getValue<int64_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt8())    ret = makeValue(inst.result, tt, (int8_t) getValue<int64_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint16())  ret = makeValue(inst.result, tt, (uint16_t) getValue<uint8_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint32())  ret = makeValue(inst.result, tt, (uint32_t) getValue<uint8_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint64())  ret = makeValue(inst.result, tt, (uint64_t) getValue<uint8_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint32()) ret = makeValue(inst.result, tt, (uint32_t) getValue<uint16_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint64()) ret = makeValue(inst.result, tt, (uint64_t) getValue<uint16_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint8())  ret = makeValue(inst.result, tt, (uint8_t) getValue<uint16_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint64()) ret = makeValue(inst.result, tt, (uint64_t) getValue<uint32_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint16()) ret = makeValue(inst.result, tt, (uint16_t) getValue<uint32_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint8())  ret = makeValue(inst.result, tt, (uint8_t) getValue<uint32_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint32()) ret = makeValue(inst.result, tt, (uint32_t) getValue<uint64_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint16()) ret = makeValue(inst.result, tt, (uint16_t) getValue<uint64_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint8())  ret = makeValue(inst.result, tt, (uint8_t) getValue<uint64_t>(a));
				else                                                        error("interp: unsupported");

				setRet(inst, ret);
				break;
			}



			case OpKind::Value_WritePtr:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				if(a->type != b->type->getPointerElementType())
					error("interp: cannot store '%s' into '%s'", a->type, b->type);

				auto ptr = (void*) getValue<uintptr_t>(b);
				if(a->dataSize > LARGE_DATA_SIZE)
				{
					// just a memcopy.
					memmove(ptr, a->ptr, a->dataSize);
				}
				else
				{
					// still a memcopy, but slightly more involved.
					memmove(ptr, &a->data[0], a->dataSize);
				}

				break;
			}

			case OpKind::Value_ReadPtr:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				auto ty = a->type->getPointerElementType();
				auto sz = getSizeOfType(ty);

				auto ptr = (void*) getValue<uintptr_t>(a);

				interp::Value ret;
				ret.id = inst.result;
				ret.dataSize = sz;
				ret.type = ty;

				if(sz > LARGE_DATA_SIZE)
				{
					// clone the memory and store it.
					auto newmem = malloc(sz);
					memmove(newmem, ptr, sz);
					ret.ptr = newmem;
				}
				else
				{
					// memcopy.
					memmove(&ret.data[0], ptr, sz);
				}

				setRet(inst, ret);
				break;
			}









			/*
				! stuff below this is not done !

				right now, we should just allocate a block of memory and use it as the stack; possibly expandable, up to a certain
				limit. we push/pop by type, but the typechecker should prevent us from doing dumb things accidentally?

				wrt. stack: allocate the block of memory; need a way to keep track (internal interpreter state) of the number
				of things we pushed to the stack, and pop that shit once we return. we follow cdecl, so the calling function cleans
				up the arguments from the stack.


				wrt. phi nodes: we need to figure out a way to lower them to something that we can interpret, since they're not
				directly executable. a simple solution is to keep track of the "previous block" in the interpreter state.

				when we do a branch, we change "previous block" to be "this block", then branch. that should let us figure out which
				block was the predecessor and thus select the correct value from the PHI node list.


				wrt. function calls: push the arguments to the stack, then the return address (or in this case, instruction number?),
				then branch. on 'ret', pop the return address and branch to it.


				the rest of the instructions should just be memory fiddling (like insertvalue/extractvalue, the SAA stuff, etc.). hopefully
				we should be able to throw out a working prototype for (at least single-threaded) interpreter...
			*/




			case OpKind::Value_StackAlloc:
			{
				iceAssert(inst.args.size() == 1);
				auto ty = getArg(inst, 0)->type;


				break;
			}

			case OpKind::Value_CreatePHI:
			{
				iceAssert(inst.args.size() == 1);
				llvm::Type* t = typeToLlvm(inst.args[0]->getType(), module);

				auto phi = dcast(PHINode, inst.realOutput);
				iceAssert(phi);

				llvm::PHINode* ret = builder.CreatePHI(t, (unsigned int) phi->getValues().size());

				for(auto v : phi->getValues())
					ret->addIncoming(decay(v.second, getValue(v.second)), llvm::cast<llvm::BasicBlock>(decay(v.first, getValue(v.first))));

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_CallFunction:
			{
				iceAssert(inst.args.size() >= 1);
				Function* fn = dcast(Function, inst.args[0]);
				iceAssert(fn);

				llvm::Function* a = llvm::cast<llvm::Function>(getUndecayedOperand(inst, 0));

				std::vector<llvm::Value*> args;

				std::vector<Value*> fargs = inst.args;

				for(size_t i = 1; i < fargs.size(); ++i)
				{
					args.push_back(decay(fargs[i], getValue(fargs[i])));
					// args.back()->dump();
				}

				// a->dump();
				llvm::Value* ret = builder.CreateCall(a, args);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_CallFunctionPointer:
			{
				iceAssert(inst.args.size() >= 1);
				llvm::Value* fn = getOperand(inst, 0);

				std::vector<llvm::Value*> args;

				std::vector<Value*> fargs = inst.args;

				for(size_t i = 1; i < fargs.size(); ++i)
					args.push_back(decay(fargs[i], getValue(fargs[i])));

				llvm::Type* lft = typeToLlvm(inst.args.front()->getType(), module);

				iceAssert(lft->isPointerTy());
				iceAssert(lft->getPointerElementType()->isFunctionTy());

				llvm::FunctionType* ft = llvm::cast<llvm::FunctionType>(lft->getPointerElementType());
				iceAssert(ft);

				llvm::Value* ret = builder.CreateCall(ft, fn, args);

				addValueToMap(ret, inst.realOutput);

				break;
			}

			case OpKind::Value_CallVirtualMethod:
			{
				// args are: 0. class, 1. index, 2. functiontype, 3...N args
				auto clsty = inst.args[0]->getType()->toClassType();
				iceAssert(clsty);

				std::vector<llvm::Value*> args;
				for(size_t i = 3; i < inst.args.size(); i++)
					args.push_back(decay(inst.args[i], getValue(inst.args[i])));

				llvm::Value* vtable = builder.CreateLoad(builder.CreateStructGEP(typeToLlvm(clsty, module), args[0], 0));

				vtable = builder.CreateBitOrPointerCast(vtable,
					llvm::ArrayType::get(llvm::FunctionType::get(llvm::Type::getVoidTy(gc), false)->getPointerTo(),
					clsty->getVirtualMethodCount())->getPointerTo());

				auto fptr = builder.CreateConstInBoundsGEP2_32(vtable->getType()->getPointerElementType(), vtable,
					0, (unsigned int) dcast(ConstantInt, inst.args[1])->getUnsignedValue());

				auto ffty = inst.args[2]->getType()->toFunctionType();

				fptr = builder.CreateBitOrPointerCast(builder.CreateLoad(fptr), typeToLlvm(ffty, module));

				llvm::FunctionType* ft = llvm::cast<llvm::FunctionType>(typeToLlvm(ffty, module)->getPointerElementType());
				iceAssert(ft);
				llvm::Value* ret = builder.CreateCall(ft, fptr, args);

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_Return:
			{
				llvm::Value* ret = 0;
				if(inst.args.size() == 0)
				{
					ret = builder.CreateRetVoid();
				}
				else
				{
					iceAssert(inst.args.size() == 1);
					llvm::Value* a = getOperand(inst, 0);

					ret = builder.CreateRet(a);
				}

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Branch_UnCond:
			{
				iceAssert(inst.args.size() == 1);
				llvm::Value* a = getOperand(inst, 0);

				llvm::Value* ret = builder.CreateBr(llvm::cast<llvm::BasicBlock>(a));
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Branch_Cond:
			{
				iceAssert(inst.args.size() == 3);
				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);
				llvm::Value* c = getOperand(inst, 2);

				llvm::Value* ret = builder.CreateCondBr(a, llvm::cast<llvm::BasicBlock>(b), llvm::cast<llvm::BasicBlock>(c));
				addValueToMap(ret, inst.realOutput);
				break;
			}




			case OpKind::Cast_Bitcast:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = builder.CreateBitCast(a, t);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Cast_IntSize:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);

				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = builder.CreateIntCast(a, t, ft->isSignedIntType());
				addValueToMap(ret, inst.realOutput);

				break;
			}

			case OpKind::Cast_Signedness:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);

				// no-op
				addValueToMap(a, inst.realOutput);
				break;
			}

			case OpKind::Cast_FloatToInt:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = 0;
				if(ft->isSignedIntType())
					ret = builder.CreateFPToSI(a, t);
				else
					ret = builder.CreateFPToUI(a, t);

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Cast_IntToFloat:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = 0;
				if(inst.args[0]->getType()->isSignedIntType())
					ret = builder.CreateSIToFP(a, t);
				else
					ret = builder.CreateUIToFP(a, t);

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Cast_PointerType:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = builder.CreatePointerCast(a, t);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Cast_PointerToInt:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = builder.CreatePtrToInt(a, t);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Cast_IntToPointer:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				Type* ft = inst.args[1]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = builder.CreateIntToPtr(a, t);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Cast_IntSignedness:
			{
				// is no op.
				// since llvm does not differentiate signed and unsigned.

				iceAssert(inst.args.size() == 2);
				llvm::Value* ret = getOperand(inst, 0);

				addValueToMap(ret, inst.realOutput);
				break;
			}





			case OpKind::Value_GetPointer:
			{
				// equivalent to GEP(ptr*, index)
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(!inst.args[0]->getType()->isClassType() && !inst.args[0]->getType()->isStructType());

				llvm::Value* ret = builder.CreateGEP(a, b);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_GetGEP2:
			{
				// equivalent to GEP(ptr*, index)
				iceAssert(inst.args.size() == 3);
				llvm::Value* a = getOperand(inst, 0);

				iceAssert(!inst.args[0]->getType()->isClassType() && !inst.args[0]->getType()->isStructType());

				std::vector<llvm::Value*> indices = { getOperand(inst, 1), getOperand(inst, 2) };
				llvm::Value* ret = builder.CreateGEP(a, indices);

				addValueToMap(ret, inst.realOutput);
				break;
			}



			case OpKind::Misc_Sizeof:
			{
				iceAssert(inst.args.size() == 1);

				llvm::Type* t = getOperand(inst, 0)->getType();
				iceAssert(t);

				llvm::Value* gep = builder.CreateConstGEP1_64(llvm::ConstantPointerNull::get(t->getPointerTo()), 1);
				gep = builder.CreatePtrToInt(gep, llvm::Type::getInt64Ty(gc));

				addValueToMap(gep, inst.realOutput);
				break;
			}










			case OpKind::Logical_Not:
			{
				iceAssert(inst.args.size() == 1);
				llvm::Value* a = getOperand(inst, 0);

				llvm::Value* ret = builder.CreateICmpEQ(a, llvm::Constant::getNullValue(a->getType()));
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_PointerAddition:
			{
				iceAssert(inst.args.size() == 2);

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isPointerTy());
				iceAssert(b->getType()->isIntegerTy());

				llvm::Value* ret = builder.CreateInBoundsGEP(a, b);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_PointerSubtraction:
			{
				iceAssert(inst.args.size() == 2);

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isPointerTy());
				iceAssert(b->getType()->isIntegerTy());

				llvm::Value* negb = builder.CreateNeg(b);
				llvm::Value* ret = builder.CreateInBoundsGEP(a, negb);
				addValueToMap(ret, inst.realOutput);
				break;
			}




			case OpKind::Value_InsertValue:
			{
				iceAssert(inst.args.size() >= 3);

				llvm::Value* str = getOperand(inst, 0);
				llvm::Value* elm = getOperand(inst, 1);

				std::vector<unsigned int> inds;
				for(size_t i = 2; i < inst.args.size(); i++)
				{
					ConstantInt* ci = dcast(ConstantInt, inst.args[i]);
					iceAssert(ci);

					inds.push_back((unsigned int) ci->getUnsignedValue());
				}


				iceAssert(str->getType()->isStructTy() || str->getType()->isArrayTy());
				if(str->getType()->isStructTy())
				{
					iceAssert(elm->getType() == llvm::cast<llvm::StructType>(str->getType())->getElementType(inds[0]));
				}
				else if(str->getType()->isArrayTy())
				{
					iceAssert(elm->getType() == llvm::cast<llvm::ArrayType>(str->getType())->getElementType());
				}
				else
				{
					iceAssert(0);
				}

				llvm::Value* ret = builder.CreateInsertValue(str, elm, inds);
				addValueToMap(ret, inst.realOutput);

				break;
			}

			case OpKind::Value_ExtractValue:
			{
				iceAssert(inst.args.size() >= 2);

				llvm::Value* str = getOperand(inst, 0);

				std::vector<unsigned int> inds;
				for(size_t i = 1; i < inst.args.size(); i++)
				{
					ConstantInt* ci = dcast(ConstantInt, inst.args[i]);
					iceAssert(ci);

					inds.push_back((unsigned int) ci->getUnsignedValue());
				}

				iceAssert(str->getType()->isStructTy() || str->getType()->isArrayTy());

				llvm::Value* ret = builder.CreateExtractValue(str, inds);
				addValueToMap(ret, inst.realOutput);

				break;
			}


















			case OpKind::SAA_GetData:
			case OpKind::SAA_GetLength:
			case OpKind::SAA_GetCapacity:
			case OpKind::SAA_GetRefCountPtr:
			{
				iceAssert(inst.args.size() == 1);

				llvm::Value* a = getOperand(inst, 0);
				iceAssert(a->getType()->isStructTy());

				int ind = 0;
				if(inst.opKind == OpKind::SAA_GetData)
					ind = SAA_DATA_INDEX;

				else if(inst.opKind == OpKind::SAA_GetLength)
					ind = SAA_LENGTH_INDEX;

				else if(inst.opKind == OpKind::SAA_GetCapacity)
					ind = SAA_CAPACITY_INDEX;

				else if(inst.opKind == OpKind::SAA_GetRefCountPtr)
					ind = SAA_REFCOUNTPTR_INDEX;

				else
					iceAssert(0 && "invalid");

				llvm::Value* ret = builder.CreateExtractValue(a, ind);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::SAA_SetData:
			case OpKind::SAA_SetLength:
			case OpKind::SAA_SetCapacity:
			case OpKind::SAA_SetRefCountPtr:
			{
				iceAssert(inst.args.size() == 2);

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isStructTy());

				int ind = 0;
				if(inst.opKind == OpKind::SAA_SetData)
					ind = SAA_DATA_INDEX;

				else if(inst.opKind == OpKind::SAA_SetLength)
					ind = SAA_LENGTH_INDEX;

				else if(inst.opKind == OpKind::SAA_SetCapacity)
					ind = SAA_CAPACITY_INDEX;

				else if(inst.opKind == OpKind::SAA_SetRefCountPtr)
					ind = SAA_REFCOUNTPTR_INDEX;

				else
					iceAssert(0 && "invalid");

				llvm::Value* ret = builder.CreateInsertValue(a, b, ind);
				addValueToMap(ret, inst.realOutput);
				break;
			}









			case OpKind::ArraySlice_GetData:
			case OpKind::ArraySlice_GetLength:
			{
				iceAssert(inst.args.size() == 1);

				llvm::Value* a = getOperand(inst, 0);
				iceAssert(a->getType()->isStructTy());

				int ind = 0;
				if(inst.opKind == OpKind::ArraySlice_GetData)         ind = SLICE_DATA_INDEX;
				else if(inst.opKind == OpKind::ArraySlice_GetLength)  ind = SLICE_LENGTH_INDEX;
				else                                                        iceAssert(0 && "invalid");

				llvm::Value* ret = builder.CreateExtractValue(a, ind);
				addValueToMap(ret, inst.realOutput);
				break;
			}



			case OpKind::ArraySlice_SetData:
			case OpKind::ArraySlice_SetLength:
			{
				iceAssert(inst.args.size() == 2);

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isStructTy());

				int ind = 0;
				if(inst.opKind == OpKind::ArraySlice_SetData)         ind = SLICE_DATA_INDEX;
				else if(inst.opKind == OpKind::ArraySlice_SetLength)  ind = SLICE_LENGTH_INDEX;
				else                                                        iceAssert(0 && "invalid");

				llvm::Value* ret = builder.CreateInsertValue(a, b, ind);
				addValueToMap(ret, inst.realOutput);
				break;
			}









			case OpKind::Any_GetData:
			case OpKind::Any_GetTypeID:
			case OpKind::Any_GetRefCountPtr:
			{
				iceAssert(inst.args.size() == 1);

				llvm::Value* a = getOperand(inst, 0);
				iceAssert(a->getType()->isStructTy());

				int ind = 0;
				if(inst.opKind == OpKind::Any_GetTypeID)
					ind = ANY_TYPEID_INDEX;

				else if(inst.opKind == OpKind::Any_GetRefCountPtr)
					ind = ANY_REFCOUNTPTR_INDEX;

				else if(inst.opKind == OpKind::Any_GetData)
					ind = ANY_DATA_ARRAY_INDEX;

				else
					iceAssert(0 && "invalid");

				llvm::Value* ret = builder.CreateExtractValue(a, ind);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Any_SetData:
			case OpKind::Any_SetTypeID:
			case OpKind::Any_SetRefCountPtr:
			{
				iceAssert(inst.args.size() == 2);

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isStructTy());

				int ind = 0;
				if(inst.opKind == OpKind::Any_SetTypeID)
					ind = ANY_TYPEID_INDEX;

				else if(inst.opKind == OpKind::Any_SetRefCountPtr)
					ind = ANY_REFCOUNTPTR_INDEX;

				else if(inst.opKind == OpKind::Any_SetData)
					ind = ANY_DATA_ARRAY_INDEX;

				else
					iceAssert(0 && "invalid");

				llvm::Value* ret = builder.CreateInsertValue(a, b, ind);
				addValueToMap(ret, inst.realOutput);
				break;
			}



			case OpKind::Range_GetLower:
			case OpKind::Range_GetUpper:
			case OpKind::Range_GetStep:
			{
				unsigned int pos = 0;
				if(inst.opKind == OpKind::Range_GetUpper)
					pos = 1;

				else if(inst.opKind == OpKind::Range_GetStep)
					pos = 2;

				llvm::Value* a = getOperand(inst, 0);
				iceAssert(a->getType()->isStructTy());

				llvm::Value* val = builder.CreateExtractValue(a, { pos });
				addValueToMap(val, inst.realOutput);

				break;
			}


			case OpKind::Range_SetLower:
			case OpKind::Range_SetUpper:
			case OpKind::Range_SetStep:
			{
				unsigned int pos = 0;
				if(inst.opKind == OpKind::Range_SetUpper)
					pos = 1;

				else if(inst.opKind == OpKind::Range_SetStep)
					pos = 2;

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isStructTy());
				iceAssert(b->getType()->isIntegerTy());

				llvm::Value* ret = builder.CreateInsertValue(a, b, { pos });
				addValueToMap(ret, inst.realOutput);

				break;
			}



			case OpKind::Enum_GetIndex:
			case OpKind::Enum_GetValue:
			{
				unsigned int pos = 0;
				if(inst.opKind == OpKind::Enum_GetValue)
					pos = 1;

				llvm::Value* a = getOperand(inst, 0);
				iceAssert(a->getType()->isStructTy());

				llvm::Value* val = builder.CreateExtractValue(a, { pos });
				addValueToMap(val, inst.realOutput);

				break;
			}


			case OpKind::Enum_SetIndex:
			case OpKind::Enum_SetValue:
			{
				unsigned int pos = 0;
				if(inst.opKind == OpKind::Enum_SetValue)
					pos = 1;

				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getOperand(inst, 1);

				iceAssert(a->getType()->isStructTy());
				if(pos == 0)	iceAssert(b->getType()->isIntegerTy());

				llvm::Value* ret = builder.CreateInsertValue(a, b, { pos });
				addValueToMap(ret, inst.realOutput);

				break;
			}

			case OpKind::Value_Select:
			{
				llvm::Value* cond = getOperand(inst, 0);
				llvm::Value* one = getOperand(inst, 1);
				llvm::Value* two = getOperand(inst, 2);

				iceAssert(cond->getType()->isIntegerTy() && cond->getType()->getIntegerBitWidth() == 1);
				iceAssert(one->getType() == two->getType());

				llvm::Value* ret = builder.CreateSelect(cond, one, two);
				addValueToMap(ret, inst.realOutput);

				break;
			}




			case OpKind::Value_CreateLVal:
			{
				iceAssert(inst.args.size() == 1);
				Type* ft = inst.args[0]->getType();
				llvm::Type* t = typeToLlvm(ft, module);

				llvm::Value* ret = builder.CreateAlloca(t);
				builder.CreateStore(llvm::Constant::getNullValue(t), ret);

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_Store:
			{
				iceAssert(inst.args.size() == 2);
				llvm::Value* a = getOperand(inst, 0);
				llvm::Value* b = getUndecayedOperand(inst, 1);

				if(a->getType() != b->getType()->getPointerElementType())
					error("llvm: cannot store '%s' into '%s'", inst.args[0]->getType(), inst.args[1]->getType());

				llvm::Value* ret = builder.CreateStore(a, b);
				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Value_AddressOf:
			{
				iceAssert(inst.args.size() == 1);
				llvm::Value* a = getUndecayedOperand(inst, 0);

				addValueToMap(a, inst.realOutput);
				break;
			}

			case OpKind::Value_Dereference:
			{
				iceAssert(inst.args.size() == 1);
				llvm::Value* a = getOperand(inst, 0);

				addValueToMap(a, inst.realOutput);
				break;
			}




			case OpKind::Value_GetPointerToStructMember:
			{
				// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
				error("llvm: enotsup");
			}

			case OpKind::Value_GetStructMember:
			{
				// equivalent to GEP(ptr*, 0, memberIndex)
				iceAssert(inst.args.size() == 2);
				llvm::Value* ptr = getUndecayedOperand(inst, 0);

				ConstantInt* ci = dcast(ConstantInt, inst.args[1]);
				iceAssert(ci);

				// ptr->dump();
				llvm::Value* ret = builder.CreateStructGEP(ptr->getType()->getPointerElementType(),
					ptr, (unsigned int) ci->getUnsignedValue());

				addValueToMap(ret, inst.realOutput);
				break;
			}


			case OpKind::Union_GetVariantID:
			{
				// fairly straightforward.
				iceAssert(inst.args.size() == 1);
				iceAssert(inst.args[0]->getType()->isUnionType());

				llvm::Value* uv = getOperand(inst, 0);
				llvm::Value* ret = builder.CreateExtractValue(uv, { 0 });

				addValueToMap(ret, inst.realOutput);
				break;
			}

			case OpKind::Union_SetVariantID:
			{
				iceAssert(inst.args.size() == 2);
				iceAssert(inst.args[0]->getType()->isUnionType());

				llvm::Value* uv = getOperand(inst, 0);
				llvm::Value* ret = builder.CreateInsertValue(uv, getOperand(inst, 1), { 0 });

				addValueToMap(ret, inst.realOutput);
				break;
			}


			case OpKind::Union_GetValue:
			{
				iceAssert(inst.args.size() == 2);
				iceAssert(inst.args[0]->getType()->isUnionType());

				auto ut = inst.args[0]->getType()->toUnionType();
				auto vid = dcast(ConstantInt, inst.args[1])->getSignedValue();

				iceAssert((size_t) vid < ut->getVariantCount());
				auto vt = ut->getVariant(vid)->getInteriorType();

				auto lut = typeToLlvm(ut, module);
				auto lvt = typeToLlvm(vt, module);

				llvm::Value* unionVal = getOperand(inst, 0);
				llvm::Value* arrp = builder.CreateAlloca(lut->getStructElementType(1));
				builder.CreateStore(builder.CreateExtractValue(unionVal, { 1 }), arrp);

				// cast to the appropriate type.
				llvm::Value* ret = builder.CreatePointerCast(arrp, lvt->getPointerTo());
				ret = builder.CreateLoad(ret);

				addValueToMap(ret, inst.realOutput);
				break;
			}


			case OpKind::Union_SetValue:
			{
				iceAssert(inst.args.size() == 3);
				iceAssert(inst.args[0]->getType()->isUnionType());

				auto luv = getOperand(inst, 0);
				auto lut = luv->getType();

				auto val = getOperand(inst, 2);

				// this is not really efficient, but without a significant
				// re-architecting of how we handle structs and pointers and shit
				// (to let us use GEP for everything), this will have to do.

				llvm::Value* arrp = builder.CreateAlloca(lut->getStructElementType(1));

				// cast to the correct pointer type
				auto valp = builder.CreateBitCast(arrp, val->getType()->getPointerTo());
				builder.CreateStore(val, valp);

				// cast it back, then load it.
				arrp = builder.CreateBitCast(valp, arrp->getType());
				auto arr = builder.CreateLoad(arrp);

				// insert it back into the union.
				luv = builder.CreateInsertValue(luv, arr, { 1 });

				// then insert the id.
				luv = builder.CreateInsertValue(luv, getOperand(inst, 1), { 0 });

				addValueToMap(luv, inst.realOutput);
				break;
			}

			case OpKind::Unreachable:
			{
				builder.CreateUnreachable();
				break;
			}

			case OpKind::Invalid:
			{
				// note we don't use "default" to catch
				// new opkinds that we forget to add.
				iceAssert("invalid opcode" && 0);
			}
		}
	}

	interp::Value InterpState::runFunction(const std::string& name, const std::vector<interp::Value>& args)
	{
		interp::Value ret;


		return ret;
	}
}
}


















