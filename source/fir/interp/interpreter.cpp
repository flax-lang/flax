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

#define SLICE_DATA_INDEX            0
#define SLICE_LENGTH_INDEX          1

#define SAA_DATA_INDEX              0
#define SAA_LENGTH_INDEX            1
#define SAA_CAPACITY_INDEX          2
#define SAA_REFCOUNTPTR_INDEX       3

#define ANY_TYPEID_INDEX            0
#define ANY_REFCOUNTPTR_INDEX       1
#define ANY_DATA_ARRAY_INDEX        2

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

		error("interp: unsupported");
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
			error("packing error of type '%s': predicted size %d, actual size %d!", ty, fsz, sizeof(T));

		memset(&ret.data[0], 0, 32);

		if(sizeof(T) > LARGE_DATA_SIZE)
		{
			ret.ptr = malloc(sizeof(T));
			memmove(ret.ptr, &val, sizeof(T));
		}
		else
		{
			memmove(&ret.data[0], &val, sizeof(T));
		}

		return ret;
	}

	static interp::Value makeValue(size_t id, Type* ty)
	{
		interp::Value ret;
		ret.id = id;
		ret.type = ty;
		ret.dataSize = getSizeOfType(ty);

		memset(&ret.data[0], 0, 32);

		if(ret.dataSize > LARGE_DATA_SIZE)
			ret.ptr = calloc(1, ret.dataSize);

		return ret;
	}

	template <typename T>
	static T getActualValue(interp::Value* v)
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

		if(v->dataSize > LARGE_DATA_SIZE)
		{
			ret.ptr = calloc(1, v->dataSize);
			memmove(ret.ptr, v->ptr, v->dataSize);
		}
		return ret;
	}

	static void setValueRaw(InterpState* is, size_t id, void* value, size_t sz)
	{
		if(auto it = is->stackFrames.back().values.find(id); it != is->stackFrames.back().values.end())
		{
			auto& v = it->second;
			if(v.dataSize != sz)
				error("interp: cannot set value, size mismatch (%zu vs %zu)", v.dataSize, sz);

			if(sz > LARGE_DATA_SIZE)    memmove(v.ptr, value, sz);
			else                        memmove(&v.data[0], value, sz);
		}
		else
		{
			error("interp: no value with id %zu", id);
		}
	}

	static void setValue(InterpState* is, size_t id, const interp::Value& val)
	{
		if(auto it = is->stackFrames.back().values.find(id); it != is->stackFrames.back().values.end())
		{
			auto& v = it->second;
			if(v.type != val.type)
				error("interp: cannot set value, conflicting types '%s' and '%s'", v.type, val.type);

			if(val.dataSize > LARGE_DATA_SIZE)
				memmove(v.ptr, val.ptr, val.dataSize);

			else
				memmove(&v.data[0], &val.data[0], val.dataSize);
		}
		else
		{
			error("interp: no value with id %zu", id);
		}
	}


	static interp::Value doInsertValue(interp::InterpState* is, size_t resid, interp::Value* str, interp::Value* elm, int64_t idx)
	{
		iceAssert(str->type->isStructType() || str->type->isArrayType());

		// we clone the value first
		auto ret = cloneValue(resid, str);

		size_t ofs = 0;

		if(str->type->isStructType())
		{
			auto strty = str->type->toStructType();
			iceAssert(idx < strty->getElementCount());

			for(size_t i = 0; i < idx; i++)
				ofs += getSizeOfType(strty->getElementN(i));
		}
		else
		{
			auto arrty = str->type->toArrayType();
			iceAssert(idx < arrty->getArraySize());

			ofs = idx * getSizeOfType(arrty->getElementType());
		}


		uintptr_t dst = 0;
		if(str->dataSize > LARGE_DATA_SIZE) dst = (uintptr_t) ret.ptr;
		else                                dst = (uintptr_t) &ret.data[0];

		uintptr_t src = 0;
		if(elm->dataSize > LARGE_DATA_SIZE) src = (uintptr_t) elm->ptr;
		else                                src = (uintptr_t) &elm->data[0];

		memmove((void*) (dst + ofs), (void*) src, elm->dataSize);

		return ret;
	}


	static interp::Value doExtractValue(interp::InterpState* is, size_t resid, interp::Value* str, int64_t idx)
	{
		iceAssert(str->type->isStructType() || str->type->isArrayType());

		size_t ofs = 0;

		fir::Type* elm = 0;
		if(str->type->isStructType())
		{
			auto strty = str->type->toStructType();
			iceAssert(idx < strty->getElementCount());

			for(size_t i = 0; i < idx; i++)
				ofs += getSizeOfType(strty->getElementN(i));

			elm = strty->getElementN(idx);
		}
		else
		{
			auto arrty = str->type->toArrayType();
			iceAssert(idx < arrty->getArraySize());

			ofs = idx * getSizeOfType(arrty->getElementType());
			elm = arrty->getElementType();
		}

		auto ret = makeValue(resid, elm);

		uintptr_t src = 0;
		if(str->dataSize > LARGE_DATA_SIZE) src = (uintptr_t) str->ptr;
		else                                src = (uintptr_t) &str->data[0];

		uintptr_t dst = 0;
		if(ret.dataSize > LARGE_DATA_SIZE)  dst = (uintptr_t) ret.ptr;
		else                                dst = (uintptr_t) &ret.data[0];

		memmove((void*) dst, (void*) (src + ofs), ret.dataSize);

		return ret;
	}

	// this saves us a lot of copy/paste

	template <typename Functor>
	static interp::Value oneArgumentOpIntOnly(const interp::Instruction& inst, interp::Value* a, Functor op)
	{
		auto rty = inst.origRes->getType();
		auto rid = inst.result;

		auto ty = a->type;

		if(ty == Type::getInt8())    return makeValue(rid, rty, op(getActualValue<int8_t>(a)));
		if(ty == Type::getInt16())   return makeValue(rid, rty, op(getActualValue<int16_t>(a)));
		if(ty == Type::getInt32())   return makeValue(rid, rty, op(getActualValue<int32_t>(a)));
		if(ty == Type::getInt64())   return makeValue(rid, rty, op(getActualValue<int64_t>(a)));
		if(ty == Type::getUint8())   return makeValue(rid, rty, op(getActualValue<uint8_t>(a)));
		if(ty == Type::getUint16())  return makeValue(rid, rty, op(getActualValue<uint16_t>(a)));
		if(ty == Type::getUint32())  return makeValue(rid, rty, op(getActualValue<uint32_t>(a)));
		if(ty == Type::getUint64())  return makeValue(rid, rty, op(getActualValue<uint64_t>(a)));
		if(ty->isPointerType())      return makeValue(rid, rty, op(getActualValue<uintptr_t>(a)));
		else                         error("interp: unsupported type '%s'", ty);
	}

	template <typename Functor>
	static interp::Value oneArgumentOp(const interp::Instruction& inst, interp::Value* a, Functor op)
	{
		auto rty = inst.origRes->getType();
		auto rid = inst.result;

		auto ty = a->type;
		if(!ty->isFloatingPointType())  return oneArgumentOpIntOnly(inst, a, op);
		if(ty == Type::getFloat32())    return makeValue(rid, rty, op(getActualValue<float>(a)));
		if(ty == Type::getFloat64())    return makeValue(rid, rty, op(getActualValue<double>(a)));
		else                            error("interp: unsupported type '%s'", ty);
	}

	template <typename Functor>
	static interp::Value twoArgumentOpIntOnly(const interp::Instruction& inst, interp::Value* a, interp::Value* b, Functor op)
	{
		auto rty = inst.origRes->getType();
		auto rid = inst.result;

		auto aty = a->type;
		auto bty = b->type;

		using i8tT  = int8_t;   auto i8t  = Type::getInt8();
		using i16tT = int16_t;  auto i16t = Type::getInt16();
		using i32tT = int32_t;  auto i32t = Type::getInt32();
		using i64tT = int64_t;  auto i64t = Type::getInt64();
		using u8tT  = uint8_t;  auto u8t  = Type::getUint8();
		using u16tT = uint16_t; auto u16t = Type::getUint16();
		using u32tT = uint32_t; auto u32t = Type::getUint32();
		using u64tT = uint64_t; auto u64t = Type::getUint64();
		using f32tT = float;    auto f32t = Type::getFloat32();
		using f64tT = double;   auto f64t = Type::getFloat64();


		#define mv(x) makeValue(rid, rty, (x))
		#define gav(t, x) getActualValue<t>(x)

		#define If(at, bt) do { if(aty == (at) && bty == (bt)) return mv(op(gav(at##T, a), gav(bt##T, b))); } while(0)

		// FUCK LAH
		If(i8t,  i8t); If(i8t,  i16t); If(i8t,  i32t); If(i8t,  i64t);
		If(i16t, i8t); If(i16t, i16t); If(i16t, i32t); If(i16t, i64t);
		If(i32t, i8t); If(i32t, i16t); If(i32t, i32t); If(i32t, i64t);
		If(i64t, i8t); If(i64t, i16t); If(i64t, i32t); If(i64t, i64t);

		If(i8t,  u8t); If(i8t,  u16t); If(i8t,  u32t); If(i8t,  u64t);
		If(i16t, u8t); If(i16t, u16t); If(i16t, u32t); If(i16t, u64t);
		If(i32t, u8t); If(i32t, u16t); If(i32t, u32t); If(i32t, u64t);
		If(i64t, u8t); If(i64t, u16t); If(i64t, u32t); If(i64t, u64t);

		If(u8t,  i8t); If(u8t,  i16t); If(u8t,  i32t); If(u8t,  i64t);
		If(u16t, i8t); If(u16t, i16t); If(u16t, i32t); If(u16t, i64t);
		If(u32t, i8t); If(u32t, i16t); If(u32t, i32t); If(u32t, i64t);
		If(u64t, i8t); If(u64t, i16t); If(u64t, i32t); If(u64t, i64t);

		If(u8t,  u8t); If(u8t,  u16t); If(u8t,  u32t); If(u8t,  u64t);
		If(u16t, u8t); If(u16t, u16t); If(u16t, u32t); If(u16t, u64t);
		If(u32t, u8t); If(u32t, u16t); If(u32t, u32t); If(u32t, u64t);
		If(u64t, u8t); If(u64t, u16t); If(u64t, u32t); If(u64t, u64t);

		error("interp: unsupported '%s' for arithmetic", aty);

		#undef If
		#undef mv
		#undef gav
	}


	template <typename Functor>
	static interp::Value twoArgumentOp(const interp::Instruction& inst, interp::Value* a, interp::Value* b, Functor op)
	{
		if(a->type->isIntegerType() && b->type->isIntegerType())
			return twoArgumentOpIntOnly(inst, a, b, op);

		auto rty = inst.origRes->getType();
		auto rid = inst.result;

		auto aty = a->type;
		auto bty = b->type;

		using i8tT  = int8_t;   auto i8t  = Type::getInt8();
		using i16tT = int16_t;  auto i16t = Type::getInt16();
		using i32tT = int32_t;  auto i32t = Type::getInt32();
		using i64tT = int64_t;  auto i64t = Type::getInt64();
		using u8tT  = uint8_t;  auto u8t  = Type::getUint8();
		using u16tT = uint16_t; auto u16t = Type::getUint16();
		using u32tT = uint32_t; auto u32t = Type::getUint32();
		using u64tT = uint64_t; auto u64t = Type::getUint64();
		using f32tT = float;    auto f32t = Type::getFloat32();
		using f64tT = double;   auto f64t = Type::getFloat64();


		#define mv(x) makeValue(rid, rty, (x))
		#define gav(t, x) getActualValue<t>(x)

		#define If(at, bt) do { if(aty == (at) && bty == (bt)) return mv(op(gav(at##T, a), gav(bt##T, b))); } while(0)

		// FUCK LAH
		If(i8t,  f32t); If(i8t,  f64t); If(u8t,  f32t); If(u8t,  f64t);
		If(i16t, f32t); If(i16t, f64t); If(u16t, f32t); If(u16t, f64t);
		If(i32t, f32t); If(i32t, f64t); If(u32t, f32t); If(u32t, f64t);
		If(i64t, f32t); If(i64t, f64t); If(u64t, f32t); If(u64t, f64t);

		If(f32t,  i8t); If(f64t,  i8t); If(f32t,  u8t); If(f64t,  u8t);
		If(f32t, i16t); If(f64t, i16t); If(f32t, u16t); If(f64t, u16t);
		If(f32t, i32t); If(f64t, i32t); If(f32t, u32t); If(f64t, u32t);
		If(f32t, i64t); If(f64t, i64t); If(f32t, u64t); If(f64t, u64t);

		If(f32t, f32t); If(f32t, f64t); If(f64t, f32t); If(f64t, f64t);

		#undef If
		#undef mv
		#undef gav

		error("interp: unsupported types '%s' and '%s'", aty, bty);
	}








	static interp::Value runInstruction(InterpState* is, const interp::Instruction& inst);
	static interp::Value runBlock(InterpState* is, const interp::Block* blk)
	{
		interp::Value ret;
		for(const auto& inst : blk->instructions)
			runInstruction(is, inst);

		return ret;
	}

	interp::Value InterpState::runFunction(const interp::Function& fn, const std::vector<interp::Value>& args)
	{
		auto ffn = fn.origFunction;
		iceAssert(ffn && ffn->getArgumentCount() == args.size());

		// when we start a function, clear the "stack frame".
		this->stackFrames.push_back({ });

		for(const auto& arg : args)
			this->stackFrames.back().values[arg.id] = arg;

		if(fn.blocks.empty())
		{
			// wait, what?
			return interp::Value();
		}

		auto entry = &fn.blocks[0];
		this->stackFrames.back().currentFunction = &fn;
		this->stackFrames.back().currentBlock = entry;
		this->stackFrames.back().previousBlock = 0;

		auto ret = runBlock(this, entry);

		this->stackFrames.pop_back();

		return ret;
	}



	static interp::Value runInstruction(InterpState* is, const interp::Instruction& inst)
	{
		auto getArg = [is](const interp::Instruction& inst, size_t i) -> interp::Value* {
			iceAssert(i < inst.args.size());
			return &is->stackFrames.back().values[inst.args[i]];
		};

		auto getVal = [is](size_t id) -> interp::Value* {
			if(auto it = is->stackFrames.back().values.find(id); it != is->stackFrames.back().values.end())
				return &it->second;

			else
				error("interp: no value with id %zu", id);
		};

		auto setRet = [is](const interp::Instruction& inst, const interp::Value& val) -> void {
			is->stackFrames.back().values[inst.result] = val;
		};

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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
					return a / b;
				}));
				break;
			}

			case OpKind::Signed_Mod:
			case OpKind::Unsigned_Mod:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOpIntOnly(inst, a, b, [](auto a, auto b) -> auto {
					return a % b;
				}));
				break;
			}

			case OpKind::Floating_Mod:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type == b->type);
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
					return fmod(a, b);
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOp(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOpIntOnly(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOpIntOnly(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOpIntOnly(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOpIntOnly(inst, a, b, [](auto a, auto b) -> auto {
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
				setRet(inst, twoArgumentOpIntOnly(inst, a, b, [](auto a, auto b) -> auto {
					return a | b;
				}));
				break;
			}

			case OpKind::Signed_Neg:
			case OpKind::Floating_Neg:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				setRet(inst, oneArgumentOp(inst, a, [](auto a) -> auto {
					return -1 * a;
				}));
				break;
			}

			case OpKind::Bitwise_Not:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				setRet(inst, oneArgumentOpIntOnly(inst, a, [](auto a) -> auto {
					return ~a;
				}));
				break;
			}

			case OpKind::Logical_Not:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				setRet(inst, oneArgumentOpIntOnly(inst, a, [](auto a) -> auto {
					return !a;
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
					ret = makeValue(inst.result, a->type, (float) getActualValue<double>(a));

				else if(a->type == Type::getFloat32()) ret = makeValue(inst.result, a->type, (float) getActualValue<float>(a));
				else if(a->type == Type::getFloat64()) ret = makeValue(inst.result, a->type, (double) getActualValue<double>(a));
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
					ret = makeValue(inst.result, a->type, (double) getActualValue<float>(a));

				else if(a->type == Type::getFloat32()) ret = makeValue(inst.result, a->type, (float) getActualValue<float>(a));
				else if(a->type == Type::getFloat64()) ret = makeValue(inst.result, a->type, (double) getActualValue<double>(a));
				else                                   error("interp: unsupported");

				setRet(inst, ret);
				break;
			}

			case OpKind::Cast_IntSize:
			case OpKind::Integer_ZeroExt:
			case OpKind::Integer_Truncate:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto ot = a->type;
				auto tt = getArg(inst, 1)->type;
				auto r = inst.result;

				interp::Value ret;
				if(ot == tt)                                                ret = cloneValue(r, a);
				else if(ot == Type::getInt8() && tt == Type::getInt16())    ret = makeValue(r, tt, (int16_t) getActualValue<int8_t>(a));
				else if(ot == Type::getInt8() && tt == Type::getInt32())    ret = makeValue(r, tt, (int32_t) getActualValue<int8_t>(a));
				else if(ot == Type::getInt8() && tt == Type::getInt64())    ret = makeValue(r, tt, (int64_t) getActualValue<int8_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt32())   ret = makeValue(r, tt, (int32_t) getActualValue<int16_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt64())   ret = makeValue(r, tt, (int64_t) getActualValue<int16_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt8())    ret = makeValue(r, tt, (int8_t) getActualValue<int16_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt64())   ret = makeValue(r, tt, (int64_t) getActualValue<int32_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt16())   ret = makeValue(r, tt, (int16_t) getActualValue<int32_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt8())    ret = makeValue(r, tt, (int8_t) getActualValue<int32_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt32())   ret = makeValue(r, tt, (int32_t) getActualValue<int64_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt16())   ret = makeValue(r, tt, (int16_t) getActualValue<int64_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt8())    ret = makeValue(r, tt, (int8_t) getActualValue<int64_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint16())  ret = makeValue(r, tt, (uint16_t) getActualValue<uint8_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint32())  ret = makeValue(r, tt, (uint32_t) getActualValue<uint8_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint64())  ret = makeValue(r, tt, (uint64_t) getActualValue<uint8_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint32()) ret = makeValue(r, tt, (uint32_t) getActualValue<uint16_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint64()) ret = makeValue(r, tt, (uint64_t) getActualValue<uint16_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint8())  ret = makeValue(r, tt, (uint8_t) getActualValue<uint16_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint64()) ret = makeValue(r, tt, (uint64_t) getActualValue<uint32_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint16()) ret = makeValue(r, tt, (uint16_t) getActualValue<uint32_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint8())  ret = makeValue(r, tt, (uint8_t) getActualValue<uint32_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint32()) ret = makeValue(r, tt, (uint32_t) getActualValue<uint64_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint16()) ret = makeValue(r, tt, (uint16_t) getActualValue<uint64_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint8())  ret = makeValue(r, tt, (uint8_t) getActualValue<uint64_t>(a));
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

				auto ptr = (void*) getActualValue<uintptr_t>(b);
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

				auto ptr = (void*) getActualValue<uintptr_t>(a);

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



			case OpKind::Value_CreatePHI:
			{
				iceAssert(inst.args.size() == 1);
				auto ty = getArg(inst, 0)->type;

				auto phi = dcast(fir::PHINode, inst.origRes);
				iceAssert(phi);

				// make the empty thing first
				auto val = makeValue(inst.result, ty);

				bool found = false;
				for(auto [ blk, v ] : phi->getValues())
				{
					if(blk->id == is->stackFrames.back().previousBlock->id)
					{
						setValue(is, val.id, *getVal(v->id));
						found = true;
						break;
					}
				}

				if(!found) error("interp: predecessor was not listed in the PHI node (id %zu)!", phi->id);

				setRet(inst, val);
				break;
			}




			case OpKind::Value_CallFunction:
			{
				iceAssert(inst.args.size() >= 1);
				auto fnid = inst.args[0];

				interp::Function* target = 0;

				// we probably only compiled the entry function, so if we haven't compiled the target then please do
				if(auto it = is->compiledFunctions.find(fnid); it != is->compiledFunctions.end())
				{
					target = &it->second;
				}
				else
				{
					for(auto f : is->module->getAllFunctions())
					{
						if(f->id == fnid)
						{
							target = &is->compileFunction(f);
							break;
						}
					}

					if(!target) error("interp: no function %zu", fnid);
				}

				iceAssert(target);

				std::vector<interp::Value> args;
				for(size_t i = 1; i < inst.args.size(); i++)
					args.push_back(*getVal(inst.args[i]));

				setRet(inst, is->runFunction(*target, args));
				break;
			}

			case OpKind::Value_CallFunctionPointer:
			case OpKind::Value_CallVirtualMethod:
			{
				error("interp: not supported atm");
			}

			case OpKind::Value_Return:
			{
				if(inst.args.empty())
					return interp::Value();

				else
					return *getVal(inst.args[0]);
			}

			case OpKind::Branch_UnCond:
			{
				iceAssert(inst.args.size() == 1);
				auto blkid = inst.args[0];

				const interp::Block* target = 0;
				for(const auto& b : is->stackFrames.back().currentFunction->blocks)
				{
					if(b.id == blkid)
						target = &b;
				}

				if(!target) error("interp: branch to block %zu not in current function", blkid);

				return runBlock(is, target);
			}

			case OpKind::Branch_Cond:
			{
				iceAssert(inst.args.size() == 3);
				auto cond = getArg(inst, 0);
				iceAssert(cond->type->isBoolType());

				const interp::Block* trueblk = 0;
				const interp::Block* falseblk = 0;
				for(const auto& b : is->stackFrames.back().currentFunction->blocks)
				{
					if(b.id == inst.args[1])
						trueblk = &b;

					else if(b.id == inst.args[2])
						falseblk = &b;
				}

				if(!trueblk || !falseblk) error("interp: branch to blocks %zu or %zu not in current function", trueblk, falseblk);


				if(getActualValue<bool>(cond))
					return runBlock(is, trueblk);

				else
					return runBlock(is, falseblk);
			}


			// TODO: these could be made more robust!!
			case OpKind::Cast_Bitcast:
			case OpKind::Cast_Signedness:
			case OpKind::Cast_IntSignedness:
			case OpKind::Cast_PointerType:
			case OpKind::Cast_PointerToInt:
			case OpKind::Cast_IntToPointer:
			{
				iceAssert(inst.args.size() == 2);

				auto v = cloneValue(inst.result, getArg(inst, 0));
				v.type = getArg(inst, 1)->type;

				setRet(inst, v);
				break;
			}

			case OpKind::Cast_FloatToInt:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				interp::Value ret;
				if(b->type == fir::Type::getInt8())         ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (int8_t) a; });
				else if(b->type == fir::Type::getInt16())   ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (int16_t) a; });
				else if(b->type == fir::Type::getInt32())   ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (int32_t) a; });
				else if(b->type == fir::Type::getInt64())   ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (int64_t) a; });
				else if(b->type == fir::Type::getUint8())   ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (uint8_t) a; });
				else if(b->type == fir::Type::getUint16())  ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (uint16_t) a; });
				else if(b->type == fir::Type::getUint32())  ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (uint32_t) a; });
				else if(b->type == fir::Type::getUint64())  ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (uint64_t) a; });
				else                                        error("interp: unsupported type '%s'", b->type);

				setRet(inst, ret);
				break;
			}

			case OpKind::Cast_IntToFloat:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				interp::Value ret;
				if(b->type == fir::Type::getFloat32())      ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (float) a; });
				else if(b->type == fir::Type::getFloat64()) ret = oneArgumentOp(inst, a, [](auto a) -> auto { return (double) a; });
				else                                        error("interp: unsupported type '%s'", b->type);

				setRet(inst, ret);
				break;
			}


			case OpKind::Value_GetPointer:
			{
				// equivalent to GEP(ptr*, index)
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				iceAssert(a->type->isPointerType());
				iceAssert(b->type->isIntegerType());

				auto elmty = a->type->getPointerElementType();

				setRet(inst, twoArgumentOp(inst, a, b, [elmty](auto a, auto b) -> auto {
					// this doesn't do pointer arithmetic!! if it's a pointer type, the value we get
					// will be a uintptr_t.
					return a + (b * getSizeOfType(elmty));
				}));

				break;
			}

			case OpKind::Value_GetStructMember:
			{
				// equivalent to GEP(ptr*, 0, memberIndex)
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto idx = getActualValue<int64_t>(getArg(inst, 1));

				std::vector<fir::Type*> elms;

				if(str->type->isStructType())       elms = str->type->toStructType()->getElements();
				else if(str->type->isClassType())   elms = str->type->toClassType()->getElements();
				else                                error("interp: unsupported type '%s'", str->type);

				size_t ofs = 0;
				for(size_t i = 0; i < idx; i++)
					ofs += getSizeOfType(elms[i]);

				auto elmty = elms[idx];

				uintptr_t src = 0;

				if(str->dataSize > LARGE_DATA_SIZE) src = (uintptr_t) str->ptr;
				else                                src = (uintptr_t) &str->data[0];

				src += ofs;

				auto ret = makeValue(inst.result, elmty->getPointerTo());
				setValueRaw(is, ret.id, &src, sizeof(src));

				setRet(inst, ret);
				break;
			}


			case OpKind::Value_GetGEP2:
			{
				// equivalent to GEP(ptr*, index1, index2)
				iceAssert(inst.args.size() == 3);
				auto ptr = getArg(inst, 0);
				auto i1 = getArg(inst, 1);
				auto i2 = getArg(inst, 2);

				iceAssert(i1->type == i2->type);

				// so, ptr should be a pointer to an array.
				iceAssert(ptr->type->isPointerType() && ptr->type->getPointerElementType()->isArrayType());

				auto arrty = ptr->type->getPointerElementType();
				auto elmty = arrty->getArrayElementType();

				auto ofs = twoArgumentOp(inst, i1, i2, [arrty, elmty](auto a, auto b) -> auto {
					return (a * getSizeOfType(arrty)) + (b * getSizeOfType(elmty));
				});

				setRet(inst, twoArgumentOp(inst, ptr, &ofs, [](auto a, auto b) -> auto {
					// this is not pointer arithmetic!!
					return a + b;
				}));

				break;
			}


			case OpKind::Value_GetPointerToStructMember:
			{
				// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
				error("interp: enotsup");
			}

			case OpKind::Misc_Sizeof:
			{
				iceAssert(inst.args.size() == 1);
				auto ty = getArg(inst, 0)->type;

				auto ci = fir::ConstantInt::getNative(getSizeOfType(ty));

				if(fir::getNativeWordSizeInBits() == 64) setRet(inst, makeConstant<int64_t>(ci));
				if(fir::getNativeWordSizeInBits() == 32) setRet(inst, makeConstant<int32_t>(ci));
				if(fir::getNativeWordSizeInBits() == 16) setRet(inst, makeConstant<int16_t>(ci));
				if(fir::getNativeWordSizeInBits() == 8)  setRet(inst, makeConstant<int8_t>(ci));

				break;
			}


			case OpKind::Value_CreateLVal:
			case OpKind::Value_StackAlloc:
			{
				iceAssert(inst.args.size() == 1);
				auto ty = getArg(inst, 0)->type;

				auto val = makeValue(inst.result, ty);

				setRet(inst, val);
				break;
			}

			case OpKind::Value_Store:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(inst, 0);
				auto b = getArg(inst, 1);

				// due to reasons, we don't actually have it as a pointer.
				if(a->type != b->type)
					error("interp: cannot store '%s' into '%s'", a->type, b->type->getPointerTo());

				// since b is not really a pointer under the hood, we can just store it.
				setValue(is, b->id, *a);
				break;
			}

			case OpKind::Value_AddressOf:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				uintptr_t ptr = 0;
				if(a->dataSize > LARGE_DATA_SIZE)   ptr = (uintptr_t) a->ptr;
				else                                ptr = (uintptr_t) &a->data[0];

				setRet(inst, makeValue(inst.result, fir::Type::getNativeWord(), ptr));
				break;
			}

			case OpKind::Value_Dereference:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				iceAssert(a->type->isPointerType());
				auto p = (void*) getActualValue<uintptr_t>(a);

				auto ret = makeValue(inst.result, a->type->getPointerElementType());

				setValueRaw(is, ret.id, p, ret.dataSize);
				setRet(inst, ret);
				break;
			}

			case OpKind::Value_Select:
			{
				iceAssert(inst.args.size() == 3);
				auto cond = getArg(inst, 0);
				iceAssert(cond->type->isBoolType());

				auto trueval = getArg(inst, 1);
				auto falseval = getArg(inst, 2);

				if(getActualValue<bool>(cond))  setRet(inst, *trueval);
				else                            setRet(inst, *falseval);

				break;
			}



			case OpKind::Value_InsertValue:
			{
				iceAssert(inst.args.size() == 3);

				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);
				auto idx = getActualValue<int64_t>(getArg(inst, 2));

				setRet(inst, doInsertValue(is, inst.result, str, elm, idx));
				break;
			}


			case OpKind::Value_ExtractValue:
			{
				iceAssert(inst.args.size() >= 2);

				auto str = getArg(inst, 0);
				auto idx = getActualValue<int64_t>(getArg(inst, 2));

				setRet(inst, doExtractValue(is, inst.result, str, idx));
				break;
			}


			case OpKind::SAA_GetData:
			case OpKind::SAA_GetLength:
			case OpKind::SAA_GetCapacity:
			case OpKind::SAA_GetRefCountPtr:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(inst, 0);

				interp::Value ret;

				if(ok == OpKind::SAA_GetData)
					ret = doExtractValue(is, inst.result, str, SAA_DATA_INDEX);

				else if(ok == OpKind::SAA_GetLength)
					ret = doExtractValue(is, inst.result, str, SAA_LENGTH_INDEX);

				else if(ok == OpKind::SAA_GetCapacity)
					ret = doExtractValue(is, inst.result, str, SAA_CAPACITY_INDEX);

				else if(ok == OpKind::SAA_GetRefCountPtr)
					ret = doExtractValue(is, inst.result, str, SAA_REFCOUNTPTR_INDEX);

				setRet(inst, ret);
				break;
			}

			case OpKind::SAA_SetData:
			case OpKind::SAA_SetLength:
			case OpKind::SAA_SetCapacity:
			case OpKind::SAA_SetRefCountPtr:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);

				interp::Value ret;

				if(ok == OpKind::SAA_SetData)
					ret = doInsertValue(is, inst.result, str, elm, SAA_DATA_INDEX);

				else if(ok == OpKind::SAA_SetLength)
					ret = doInsertValue(is, inst.result, str, elm, SAA_LENGTH_INDEX);

				else if(ok == OpKind::SAA_SetCapacity)
					ret = doInsertValue(is, inst.result, str, elm, SAA_CAPACITY_INDEX);

				else if(ok == OpKind::SAA_SetRefCountPtr)
					ret = doInsertValue(is, inst.result, str, elm, SAA_REFCOUNTPTR_INDEX);

				setRet(inst, ret);
				break;
			}

			case OpKind::ArraySlice_GetData:
			case OpKind::ArraySlice_GetLength:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(inst, 0);

				interp::Value ret;

				if(ok == OpKind::ArraySlice_GetData)
					ret = doExtractValue(is, inst.result, str, SLICE_DATA_INDEX);

				else if(ok == OpKind::ArraySlice_GetLength)
					ret = doExtractValue(is, inst.result, str, SLICE_LENGTH_INDEX);

				setRet(inst, ret);
				break;
			}



			case OpKind::ArraySlice_SetData:
			case OpKind::ArraySlice_SetLength:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);

				interp::Value ret;

				if(ok == OpKind::ArraySlice_SetData)
					ret = doInsertValue(is, inst.result, str, elm, SLICE_DATA_INDEX);

				else if(ok == OpKind::ArraySlice_SetLength)
					ret = doInsertValue(is, inst.result, str, elm, SLICE_LENGTH_INDEX);

				setRet(inst, ret);
				break;
			}

			case OpKind::Any_GetData:
			case OpKind::Any_GetTypeID:
			case OpKind::Any_GetRefCountPtr:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(inst, 0);

				interp::Value ret;

				if(ok == OpKind::Any_GetTypeID)
					ret = doExtractValue(is, inst.result, str, ANY_TYPEID_INDEX);

				else if(ok == OpKind::Any_GetRefCountPtr)
					ret = doExtractValue(is, inst.result, str, ANY_REFCOUNTPTR_INDEX);

				else if(ok == OpKind::Any_GetData)
					ret = doExtractValue(is, inst.result, str, ANY_DATA_ARRAY_INDEX);

				setRet(inst, ret);
				break;
			}

			case OpKind::Any_SetData:
			case OpKind::Any_SetTypeID:
			case OpKind::Any_SetRefCountPtr:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);

				interp::Value ret;

				if(ok == OpKind::Any_SetTypeID)
					ret = doInsertValue(is, inst.result, str, elm, ANY_TYPEID_INDEX);

				else if(ok == OpKind::Any_SetRefCountPtr)
					ret = doInsertValue(is, inst.result, str, elm, ANY_REFCOUNTPTR_INDEX);

				else if(ok == OpKind::Any_SetData)
					ret = doInsertValue(is, inst.result, str, elm, ANY_DATA_ARRAY_INDEX);

				setRet(inst, ret);
				break;
			}



			case OpKind::Range_GetLower:
			case OpKind::Range_GetUpper:
			case OpKind::Range_GetStep:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(inst, 0);

				interp::Value ret;

				if(ok == OpKind::Range_GetLower)
					ret = doExtractValue(is, inst.result, str, 0);

				else if(ok == OpKind::Range_GetUpper)
					ret = doExtractValue(is, inst.result, str, 1);

				else if(ok == OpKind::Range_GetStep)
					ret = doExtractValue(is, inst.result, str, 2);

				setRet(inst, ret);
				break;
			}


			case OpKind::Range_SetLower:
			case OpKind::Range_SetUpper:
			case OpKind::Range_SetStep:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);

				interp::Value ret;

				if(ok == OpKind::Range_GetLower)
					ret = doInsertValue(is, inst.result, str, elm, 0);

				else if(ok == OpKind::Range_GetUpper)
					ret = doInsertValue(is, inst.result, str, elm, 1);

				else if(ok == OpKind::Range_GetStep)
					ret = doInsertValue(is, inst.result, str, elm, 2);

				setRet(inst, ret);
				break;
			}



			case OpKind::Enum_GetIndex:
			case OpKind::Enum_GetValue:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(inst, 0);

				interp::Value ret;

				if(ok == OpKind::Enum_GetIndex)
					ret = doExtractValue(is, inst.result, str, 0);

				else if(ok == OpKind::Enum_GetValue)
					ret = doExtractValue(is, inst.result, str, 1);

				setRet(inst, ret);
				break;
			}


			case OpKind::Enum_SetIndex:
			case OpKind::Enum_SetValue:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);

				interp::Value ret;

				if(ok == OpKind::Enum_SetIndex)
					ret = doInsertValue(is, inst.result, str, elm, 0);

				else if(ok == OpKind::Enum_SetValue)
					ret = doInsertValue(is, inst.result, str, elm, 1);

				setRet(inst, ret);
				break;
			}


			case OpKind::Union_GetVariantID:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(inst, 0);

				setRet(inst, doExtractValue(is, inst.result, str, 0));
				break;
			}

			case OpKind::Union_SetVariantID:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(inst, 0);
				auto elm = getArg(inst, 1);

				setRet(inst, doInsertValue(is, inst.result, str, elm, 0));
				break;
			}

			case OpKind::Union_GetValue:
			case OpKind::Union_SetValue:
			{
				error("interp: not supported atm");
			}


			#if 0

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

			#endif

			case OpKind::Unreachable:
			{
				error("interp: unreachable op!");
			}

			case OpKind::Invalid:
			{
				// note we don't use "default" to catch
				// new opkinds that we forget to add.
				error("interp: invalid opcode %d!", inst.opcode);
			}
		}

		return interp::Value();
	}


}
}


















