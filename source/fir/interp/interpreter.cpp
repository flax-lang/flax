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
#include "platform.h"

#define FFI_BUILDING
#include <ffi.h>

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


#ifdef _MSC_VER
	#pragma warning(push, 0)
	#pragma warning(disable: 4018)
#endif



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






	static interp::Value makeValue(fir::Value* fv);
	template <typename T> static interp::Value makeValue(fir::Value* fv, const T& val);

	static void setValueRaw(InterpState* is, interp::Value* target, void* value, size_t sz);
	static void setValue(InterpState* is, interp::Value* target, const interp::Value& val);


	static std::map<ConstantValue*, interp::Value> cachedConstants;
	static interp::Value makeConstant(ConstantValue* c)
	{
		if(auto ci = dcast(ConstantInt, c))
		{
			return cachedConstants[c] = makeValue(c, ci->getSignedValue());
		}
		else if(auto cf = dcast(ConstantFP, c))
		{
			return cachedConstants[c] = makeValue(c, cf->getValue());
		}
		else if(auto cb = dcast(ConstantBool, c))
		{
			return cachedConstants[c] = makeValue(c, cb->getValue());
		}
		else if(auto cs = dcast(ConstantString, c))
		{
			auto str = cs->getValue();
			error("interp: const string");
		}
		else if(auto cbc = dcast(ConstantBitcast, c))
		{
			error("interp: const bitcast");
		}
		else if(auto ca = dcast(ConstantArray, c))
		{
			error("interp: const array");
		}
		else if(auto ct = dcast(ConstantTuple, c))
		{
			error("interp: const tuple");
		}
		else if(auto cec = dcast(ConstantEnumCase, c))
		{
			error("interp: const enumcase");
		}
		else if(auto cas = dcast(ConstantArraySlice, c))
		{
			error("interp: const slice");
		}
		else if(auto cda = dcast(ConstantDynamicArray, c))
		{
			error("interp: const dynarray");
		}
		else if(auto fn = dcast(Function, c))
		{
			error("interp: const function?");
		}
		else
		{
			return cachedConstants[c] = makeValue(c);
		}
	}

	InterpState::InterpState(Module* mod)
	{
		this->module = mod;

		for(const auto [ id, glob ] : mod->_getGlobals())
		{
			auto val = makeValue(glob);
			if(auto init = glob->getInitialValue(); init)
				setValue(this, &val, makeConstant(init));

			this->globals[glob] = val;
		}

		for(const auto [ str, glob ] : mod->_getGlobalStrings())
		{
			auto val = makeValue(glob);

			auto s = new char[str.size() + 1];
			memmove(s, str.c_str(), str.size());
			s[str.size()] = 0;

			this->strings.push_back(s);

			setValueRaw(this, &val, &s, sizeof(char*));
			this->globals[glob] = val;
		}
	}






	static ffi_type* convertTypeToLibFFI(fir::Type* ty)
	{
		if(ty->isPointerType())
		{
			return &ffi_type_pointer;
		}
		else if(ty->isBoolType())
		{
			//? HMMM....
			return &ffi_type_uint8;
		}
		else if(ty->isVoidType())
		{
			return &ffi_type_void;
		}
		else if(ty->isIntegerType())
		{
			if(ty == Type::getInt8())       return &ffi_type_sint8;
			if(ty == Type::getInt16())      return &ffi_type_sint16;
			if(ty == Type::getInt32())      return &ffi_type_sint32;
			if(ty == Type::getInt64())      return &ffi_type_sint64;

			if(ty == Type::getUint8())      return &ffi_type_uint8;
			if(ty == Type::getUint16())     return &ffi_type_uint16;
			if(ty == Type::getUint32())     return &ffi_type_uint32;
			if(ty == Type::getUint64())     return &ffi_type_uint64;
		}
		else if(ty->isFloatingPointType())
		{
			if(ty == Type::getFloat32())    return &ffi_type_float;
			if(ty == Type::getFloat64())    return &ffi_type_double;
		}
		else
		{

		}

		error("interp: unsupported type '%s' in libffi-translation", ty);
	}

	static interp::Value runFunctionWithLibFFI(fir::Function* fn, const std::vector<interp::Value>& args)
	{
		void* fnptr = platform::getSymbol(fn->getName().str());
		if(!fnptr) error("interp: failed to find symbol named '%s'\n", fn->getName().str());

		// we are assuming the values in 'args' are correct!
		ffi_type** arg_types = new ffi_type*[args.size()];
		{
			std::vector<ffi_type*> tmp;
			for(size_t i = 0; i < args.size(); i++)
			{
				tmp.push_back(convertTypeToLibFFI(args[i].type));
				arg_types[i] = tmp[i];
			}
		}

		ffi_cif fn_cif;
		{
			auto retty = convertTypeToLibFFI(fn->getReturnType());

			if(args.size() > fn->getArgumentCount())
			{
				iceAssert(fn->isCStyleVarArg());
				auto st = ffi_prep_cif_var(&fn_cif, FFI_DEFAULT_ABI, fn->getArgumentCount(), args.size(), retty, arg_types);
				if(st != FFI_OK)
					error("interp: ffi_prep_cif_var failed! (%d)", st);
			}
			else
			{
				auto st = ffi_prep_cif(&fn_cif, FFI_DEFAULT_ABI, args.size(), retty, arg_types);
				if(st != FFI_OK)
					error("interp: ffi_prep_cif failed! (%d)", st);
			}
		}

		void** arg_pointers = new void*[args.size()];
		{
			void** arg_values = new void*[args.size()];

			// because this thing is dumb
			for(size_t i = 0; i < args.size(); i++)
			{
				if(args[i].dataSize <= LARGE_DATA_SIZE)
					arg_values[i] = (void*) &args[i].data[0];
			}

			for(size_t i = 0; i < args.size(); i++)
			{
				if(args[i].dataSize <= LARGE_DATA_SIZE)
					arg_pointers[i] = (void*) arg_values[i];

				else
					arg_pointers[i] = (void*) args[i].ptr;
			}

			delete[] arg_values;
		}

		int64_t ret = 0;
		ffi_call(&fn_cif, FFI_FN(fnptr), &ret, arg_pointers);


		delete[] arg_types;
		delete[] arg_pointers;

		return interp::Value();
	}




	template <typename T>
	static interp::Value makeValue(fir::Value* fv, const T& val)
	{
		interp::Value ret;
		ret.val = fv;
		ret.type = fv->getType();
		ret.dataSize = sizeof(T);

		if(auto fsz = getSizeOfType(ret.type); fsz != sizeof(T))
			error("packing error of type '%s': predicted size %d, actual size %d!", ret.type, fsz, sizeof(T));

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


	// this lets us specify the type, instead of using the one in the Value
	static interp::Value makeValueOfType(fir::Value* fv, fir::Type* ty)
	{
		interp::Value ret;
		ret.val = fv;
		ret.type = ty;
		ret.dataSize = getSizeOfType(ret.type);

		memset(&ret.data[0], 0, 32);

		if(ret.dataSize > LARGE_DATA_SIZE)
			ret.ptr = calloc(1, ret.dataSize);

		return ret;
	}


	static interp::Value makeValue(fir::Value* fv)
	{
		return makeValueOfType(fv, fv->getType());
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


	static interp::Value cloneValue(fir::Value* fv, interp::Value* v)
	{
		interp::Value ret = *v;
		ret.val = fv;

		if(v->dataSize > LARGE_DATA_SIZE)
		{
			ret.ptr = calloc(1, v->dataSize);
			memmove(ret.ptr, v->ptr, v->dataSize);
		}
		return ret;
	}

	static void setValueRaw(InterpState* is, interp::Value* target, void* value, size_t sz)
	{
		if(target->dataSize != sz)
			error("interp: cannot set value, size mismatch (%zu vs %zu)", target->dataSize, sz);

		if(sz > LARGE_DATA_SIZE)    memmove(target->ptr, value, sz);
		else                        memmove(&target->data[0], value, sz);
	}

	static void setValue(InterpState* is, interp::Value* target, const interp::Value& val)
	{
		if(target->type != val.type)
			error("interp: cannot set value, conflicting types '%s' and '%s'", target->type, val.type);

		if(val.dataSize > LARGE_DATA_SIZE)
			memmove(target->ptr, val.ptr, val.dataSize);

		else
			memmove(&target->data[0], &val.data[0], val.dataSize);
	}


	static interp::Value doInsertValue(interp::InterpState* is, fir::Value* res, interp::Value* str, interp::Value* elm, int64_t idx)
	{
		iceAssert(str->type->isStructType() || str->type->isArrayType());

		// we clone the value first
		auto ret = cloneValue(res, str);

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


	static interp::Value doExtractValue(interp::InterpState* is, fir::Value* res, interp::Value* str, int64_t idx)
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

		auto ret = makeValue(res);
		iceAssert(ret.type == elm);

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
		auto res = inst.result;
		auto ty = a->type;

		if(ty == Type::getInt8())    return makeValue(res, op(getActualValue<int8_t>(a)));
		if(ty == Type::getInt16())   return makeValue(res, op(getActualValue<int16_t>(a)));
		if(ty == Type::getInt32())   return makeValue(res, op(getActualValue<int32_t>(a)));
		if(ty == Type::getInt64())   return makeValue(res, op(getActualValue<int64_t>(a)));
		if(ty == Type::getUint8())   return makeValue(res, op(getActualValue<uint8_t>(a)));
		if(ty == Type::getUint16())  return makeValue(res, op(getActualValue<uint16_t>(a)));
		if(ty == Type::getUint32())  return makeValue(res, op(getActualValue<uint32_t>(a)));
		if(ty == Type::getUint64())  return makeValue(res, op(getActualValue<uint64_t>(a)));
		if(ty->isPointerType())      return makeValue(res, op(getActualValue<uintptr_t>(a)));
		else                         error("interp: unsupported type '%s'", ty);
	}

	template <typename Functor>
	static interp::Value oneArgumentOp(const interp::Instruction& inst, interp::Value* a, Functor op)
	{
		auto res = inst.result;
		auto ty = a->type;

		if(!ty->isFloatingPointType())  return oneArgumentOpIntOnly(inst, a, op);
		if(ty == Type::getFloat32())    return makeValue(res, op(getActualValue<float>(a)));
		if(ty == Type::getFloat64())    return makeValue(res, op(getActualValue<double>(a)));
		else                            error("interp: unsupported type '%s'", ty);
	}

	template <typename Functor>
	static interp::Value twoArgumentOpIntOnly(const interp::Instruction& inst, interp::Value* a, interp::Value* b, Functor op)
	{
		auto res = inst.result;

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

		#define mv(x) makeValue(res, (x))
		#define gav(t, x) getActualValue<t>(x)

		#define If(at, bt) do { if(aty == (at) && bty == (bt)) return mv(op(gav(at##T, a), gav(bt##T, b))); } while(0)

		// FUCK LAH
		If(i8t,  i8t); If(i8t,  i16t); If(i8t,  i32t); If(i8t,  i64t);
		If(i16t, i8t); If(i16t, i16t); If(i16t, i32t); If(i16t, i64t);
		If(i32t, i8t); If(i32t, i16t); If(i32t, i32t); If(i32t, i64t);
		If(i64t, i8t); If(i64t, i16t); If(i64t, i32t); If(i64t, i64t);

		// If(i8t,  u8t); If(i8t,  u16t); If(i8t,  u32t); If(i8t,  u64t);
		// If(i16t, u8t); If(i16t, u16t); If(i16t, u32t); If(i16t, u64t);
		// If(i32t, u8t); If(i32t, u16t); If(i32t, u32t); If(i32t, u64t);
		// If(i64t, u8t); If(i64t, u16t); If(i64t, u32t); If(i64t, u64t);

		// If(u8t,  i8t); If(u8t,  i16t); If(u8t,  i32t); If(u8t,  i64t);
		// If(u16t, i8t); If(u16t, i16t); If(u16t, i32t); If(u16t, i64t);
		// If(u32t, i8t); If(u32t, i16t); If(u32t, i32t); If(u32t, i64t);
		// If(u64t, i8t); If(u64t, i16t); If(u64t, i32t); If(u64t, i64t);

		If(u8t,  u8t); If(u8t,  u16t); If(u8t,  u32t); If(u8t,  u64t);
		If(u16t, u8t); If(u16t, u16t); If(u16t, u32t); If(u16t, u64t);
		If(u32t, u8t); If(u32t, u16t); If(u32t, u32t); If(u32t, u64t);
		If(u64t, u8t); If(u64t, u16t); If(u64t, u32t); If(u64t, u64t);

		error("interp: unsupported types '%s' and '%s' for arithmetic", aty, bty);

		#undef If
		#undef mv
		#undef gav
	}


	template <typename Functor>
	static interp::Value twoArgumentOp(const interp::Instruction& inst, interp::Value* a, interp::Value* b, Functor op)
	{
		if(a->type->isIntegerType() && b->type->isIntegerType())
			return twoArgumentOpIntOnly(inst, a, b, op);

		auto res = inst.result;

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


		#define mv(x) makeValue(res, (x))
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
		auto ffn = fn.func;
		iceAssert(ffn && ffn->getArgumentCount() == args.size());


		if(fn.blocks.empty())
		{
			// it's probably an extern!
			// use libffi.
			return runFunctionWithLibFFI(ffn, args);
		}
		else
		{
			// when we start a function, clear the "stack frame".
			this->stackFrames.push_back({ });

			for(const auto& arg : args)
				this->stackFrames.back().values[arg.val] = arg;

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
	}



	static interp::Value runInstruction(InterpState* is, const interp::Instruction& inst)
	{
		auto getArg = [is](const interp::Instruction& inst, size_t i) -> interp::Value* {
			iceAssert(i < inst.args.size());
			return &is->stackFrames.back().values[inst.args[i]];
		};

		auto getVal = [is](fir::Value* fv) -> interp::Value* {
			if(auto it = is->stackFrames.back().values.find(fv); it != is->stackFrames.back().values.end())
				return &it->second;

			else if(auto it2 = is->globals.find(fv); it2 != is->globals.end())
				return &it2->second;

			else
				error("interp: no value with id %zu", fv->id);
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
					ret = makeValue(inst.result, (float) getActualValue<double>(a));

				else if(a->type == Type::getFloat32()) ret = makeValue(inst.result, (float) getActualValue<float>(a));
				else if(a->type == Type::getFloat64()) ret = makeValue(inst.result, (double) getActualValue<double>(a));
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
					ret = makeValue(inst.result, (double) getActualValue<float>(a));

				else if(a->type == Type::getFloat32()) ret = makeValue(inst.result, (float) getActualValue<float>(a));
				else if(a->type == Type::getFloat64()) ret = makeValue(inst.result, (double) getActualValue<double>(a));
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
				else if(ot == Type::getInt8() && tt == Type::getInt16())    ret = makeValue(r, (int16_t) getActualValue<int8_t>(a));
				else if(ot == Type::getInt8() && tt == Type::getInt32())    ret = makeValue(r, (int32_t) getActualValue<int8_t>(a));
				else if(ot == Type::getInt8() && tt == Type::getInt64())    ret = makeValue(r, (int64_t) getActualValue<int8_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt32())   ret = makeValue(r, (int32_t) getActualValue<int16_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt64())   ret = makeValue(r, (int64_t) getActualValue<int16_t>(a));
				else if(ot == Type::getInt16() && tt == Type::getInt8())    ret = makeValue(r, (int8_t) getActualValue<int16_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt64())   ret = makeValue(r, (int64_t) getActualValue<int32_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt16())   ret = makeValue(r, (int16_t) getActualValue<int32_t>(a));
				else if(ot == Type::getInt32() && tt == Type::getInt8())    ret = makeValue(r, (int8_t) getActualValue<int32_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt32())   ret = makeValue(r, (int32_t) getActualValue<int64_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt16())   ret = makeValue(r, (int16_t) getActualValue<int64_t>(a));
				else if(ot == Type::getInt64() && tt == Type::getInt8())    ret = makeValue(r, (int8_t) getActualValue<int64_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint16())  ret = makeValue(r, (uint16_t) getActualValue<uint8_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint32())  ret = makeValue(r, (uint32_t) getActualValue<uint8_t>(a));
				else if(ot == Type::getUint8() && tt == Type::getUint64())  ret = makeValue(r, (uint64_t) getActualValue<uint8_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint32()) ret = makeValue(r, (uint32_t) getActualValue<uint16_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint64()) ret = makeValue(r, (uint64_t) getActualValue<uint16_t>(a));
				else if(ot == Type::getUint16() && tt == Type::getUint8())  ret = makeValue(r, (uint8_t) getActualValue<uint16_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint64()) ret = makeValue(r, (uint64_t) getActualValue<uint32_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint16()) ret = makeValue(r, (uint16_t) getActualValue<uint32_t>(a));
				else if(ot == Type::getUint32() && tt == Type::getUint8())  ret = makeValue(r, (uint8_t) getActualValue<uint32_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint32()) ret = makeValue(r, (uint32_t) getActualValue<uint64_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint16()) ret = makeValue(r, (uint16_t) getActualValue<uint64_t>(a));
				else if(ot == Type::getUint64() && tt == Type::getUint8())  ret = makeValue(r, (uint8_t) getActualValue<uint64_t>(a));
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
				ret.val = inst.result;
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

				auto phi = dcast(fir::PHINode, inst.result);
				iceAssert(phi);

				// make the empty thing first
				auto val = makeValue(inst.result);

				bool found = false;
				for(auto [ blk, v ] : phi->getValues())
				{
					if(blk == is->stackFrames.back().previousBlock->blk)
					{
						setValue(is, &val, *getVal(v));
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
				auto fn = inst.args[0];

				interp::Function* target = 0;

				// we probably only compiled the entry function, so if we haven't compiled the target then please do
				if(auto it = is->compiledFunctions.find(fn); it != is->compiledFunctions.end())
				{
					target = &it->second;
				}
				else
				{
					for(auto f : is->module->getAllFunctions())
					{
						if(f == fn)
						{
							target = &is->compileFunction(f);
							break;
						}
					}

					if(!target) error("interp: no function %zu (name '%s')", fn->id, fn->getName().str());
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
				auto blk = inst.args[0];

				const interp::Block* target = 0;
				for(const auto& b : is->stackFrames.back().currentFunction->blocks)
				{
					if(b.blk == blk)
						target = &b;
				}

				if(!target) error("interp: branch to block %zu not in current function", blk->id);

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
					if(b.blk == inst.args[1])
						trueblk = &b;

					else if(b.blk == inst.args[2])
						falseblk = &b;
				}

				if(!trueblk || !falseblk) error("interp: branch to blocks %zu or %zu not in current function", trueblk->blk->id, falseblk->blk->id);


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

				auto ret = makeValue(inst.result);
				setValueRaw(is, &ret, &src, sizeof(src));

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

				if(fir::getNativeWordSizeInBits() == 64) setRet(inst, makeValue(inst.result, (int64_t) ci->getSignedValue()));
				if(fir::getNativeWordSizeInBits() == 32) setRet(inst, makeValue(inst.result, (int32_t) ci->getSignedValue()));
				if(fir::getNativeWordSizeInBits() == 16) setRet(inst, makeValue(inst.result, (int16_t) ci->getSignedValue()));
				if(fir::getNativeWordSizeInBits() == 8)  setRet(inst, makeValue(inst.result, (int8_t)  ci->getSignedValue()));

				break;
			}


			case OpKind::Value_CreateLVal:
			case OpKind::Value_StackAlloc:
			{
				iceAssert(inst.args.size() == 1);
				auto ty = getArg(inst, 0)->type;

				// we need to override the type, here.
				auto val = makeValueOfType(inst.result, ty);

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
				setValue(is, b, *a);
				break;
			}

			case OpKind::Value_AddressOf:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				uintptr_t ptr = 0;
				if(a->dataSize > LARGE_DATA_SIZE)   ptr = (uintptr_t) a->ptr;
				else                                ptr = (uintptr_t) &a->data[0];

				setRet(inst, makeValue(inst.result, ptr));
				break;
			}

			case OpKind::Value_Dereference:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(inst, 0);

				iceAssert(a->type->isPointerType());
				auto p = (void*) getActualValue<uintptr_t>(a);

				auto ret = makeValue(inst.result);

				setValueRaw(is, &ret, p, ret.dataSize);
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


#ifdef _MSC_VER
	#pragma warning(pop)
#endif
















