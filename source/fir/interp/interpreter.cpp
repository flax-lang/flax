// interpreter.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "ir/value.h"
#include "ir/interp.h"
#include "ir/module.h"
#include "ir/function.h"
#include "ir/instruction.h"

#include "frontend.h"
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
#else
	/*
		so msvc doesn't really warn on this. we disable these warnings locally, because of the way we're doing the
		operations. we can guarantee (because IRBuilder says so) that we won't be doing strange things like
		bitwise-not-ing a boolean or left-shifting a floating point, but the compiler won't know so we just ignore
		these things to clean up the output.
	*/
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wpragmas"
	#pragma GCC diagnostic ignored "-Wunknown-warning-option"

	#pragma GCC diagnostic ignored "-Wbool-operation"
	#pragma GCC diagnostic ignored "-Wint-in-bool-context"
	#pragma GCC diagnostic ignored "-Wimplicit-conversion-floating-point-to-bool"
	#pragma GCC diagnostic ignored "-Wdelete-incomplete"
#endif



namespace fir {
namespace interp
{
	//! ACHTUNG !
	//* in the interpreter, we assume all structs are packed, and there are no padding/alignment bytes anywhere.
	//* this greatly simplifies everything, and the performance impact is probably insignificant next to the (power of the force)
	//* whole interpreter anyway.


	template <typename T>
	static interp::Value makeValue(fir::Value* fv, const T& val)
	{
		interp::Value ret;
		ret.val = fv;
		ret.type = fv->getType();
		ret.dataSize = sizeof(T);

		if(auto fsz = getSizeOfType(ret.type); fsz != sizeof(T))
			error("packing error of type '%s': predicted size %d, actual size %d!", ret.type, fsz, sizeof(T));

		memset(&ret.data[0], 0, LARGE_DATA_SIZE);

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

		memset(&ret.data[0], 0, LARGE_DATA_SIZE);

		if(ret.dataSize > LARGE_DATA_SIZE)
			ret.ptr = calloc(1, ret.dataSize);

		return ret;
	}


	interp::Value InterpState::makeValue(fir::Value* fv)
	{
		return makeValueOfType(fv, fv->getType());
	}


	template <typename T>
	static T getActualValue(const interp::Value& v)
	{
		if(v.dataSize > LARGE_DATA_SIZE)
		{
			return *(static_cast<T*>(v.ptr));
		}
		else
		{
			return *(reinterpret_cast<T*>(const_cast<uint8_t*>(&v.data[0])));
		}
	}


	static interp::Value cloneValue(const interp::Value& v)
	{
		interp::Value ret = v;

		if(v.dataSize > LARGE_DATA_SIZE)
		{
			ret.ptr = calloc(1, v.dataSize);
			memmove(ret.ptr, v.ptr, v.dataSize);
		}
		return ret;
	}

	static interp::Value cloneValue(fir::Value* fv, const interp::Value& v)
	{
		interp::Value ret = v;
		ret.val = fv;
		ret.globalValTracker = v.globalValTracker;

		if(v.dataSize > LARGE_DATA_SIZE)
		{
			ret.ptr = calloc(1, v.dataSize);
			memmove(ret.ptr, v.ptr, v.dataSize);
		}
		return ret;
	}

	static void setValueRaw(InterpState* is, interp::Value* target, void* value, size_t sz)
	{
		if(target->dataSize != sz)
			error("interp: cannot set value, size mismatch (%d vs %d)", target->dataSize, sz);

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

	static char* makeGlobalString(InterpState* is, const std::string& str)
	{
		auto s = new char[str.size() + 1];
		memmove(s, str.c_str(), str.size());
		s[str.size()] = 0;

		is->strings.push_back(s);

		return s;
	}

	static interp::Value loadFromPtr(const interp::Value& x, fir::Type* ty)
	{
		auto ptr = reinterpret_cast<void*>(getActualValue<uintptr_t>(x));

		interp::Value ret;
		ret.dataSize = getSizeOfType(ty);
		ret.type = ty;

		if(ret.dataSize > LARGE_DATA_SIZE)
		{
			// clone the memory and store it.
			auto newmem = malloc(ret.dataSize);
			memmove(newmem, ptr, ret.dataSize);
			ret.ptr = newmem;
		}
		else
		{
			// memcopy.
			memmove(&ret.data[0], ptr, ret.dataSize);
		}

		return ret;
	}


	static std::map<ConstantValue*, interp::Value> cachedConstants;
	static interp::Value makeConstant(InterpState* is, ConstantValue* c)
	{
		auto constructStructThingy2 = [](fir::Value* val, size_t datasize, const std::vector<interp::Value>& inserts) -> interp::Value {

			uint8_t* buffer = 0;

			interp::Value ret;
			ret.dataSize = datasize;
			ret.type = val->getType();
			ret.val = val;

			if(datasize > LARGE_DATA_SIZE)  { buffer = new uint8_t[datasize]; ret.ptr = buffer; }
			else                            { buffer = &ret.data[0]; }

			iceAssert(buffer);

			uint8_t* ofs = buffer;
			for(const auto& v : inserts)
			{
				if(v.dataSize > LARGE_DATA_SIZE)    memmove(ofs, v.ptr, v.dataSize);
				else                                memmove(ofs, &v.data[0], v.dataSize);

				ofs += v.dataSize;
			}

			return ret;
		};

		auto constructStructThingy = [is, &constructStructThingy2](fir::Value* val, size_t datasize,
			const std::vector<ConstantValue*>& inserts) -> interp::Value
		{
			std::vector<interp::Value> vals;
			for(const auto& x : inserts)
				vals.push_back(makeConstant(is, x));

			return constructStructThingy2(val, datasize, vals);
		};


		if(auto ci = dcast(fir::ConstantInt, c))
		{
			interp::Value ret;

			if(ci->getType() == fir::Type::getInt8())        ret = makeValue(c, static_cast<int8_t>(ci->getSignedValue()));
			else if(ci->getType() == fir::Type::getInt16())  ret = makeValue(c, static_cast<int16_t>(ci->getSignedValue()));
			else if(ci->getType() == fir::Type::getInt32())  ret = makeValue(c, static_cast<int32_t>(ci->getSignedValue()));
			else if(ci->getType() == fir::Type::getInt64())  ret = makeValue(c, static_cast<int64_t>(ci->getSignedValue()));
			else if(ci->getType() == fir::Type::getUint8())  ret = makeValue(c, static_cast<uint8_t>(ci->getUnsignedValue()));
			else if(ci->getType() == fir::Type::getUint16()) ret = makeValue(c, static_cast<uint16_t>(ci->getUnsignedValue()));
			else if(ci->getType() == fir::Type::getUint32()) ret = makeValue(c, static_cast<uint32_t>(ci->getUnsignedValue()));
			else if(ci->getType() == fir::Type::getUint64()) ret = makeValue(c, static_cast<uint64_t>(ci->getUnsignedValue()));
			else error("interp: unsupported type '%s' for integer constant", ci->getType());

			return (cachedConstants[c] = ret);
		}
		else if(auto cf = dcast(fir::ConstantFP, c))
		{
			return cachedConstants[c] = makeValue(c, cf->getValue());
		}
		else if(auto cb = dcast(fir::ConstantBool, c))
		{
			return cachedConstants[c] = makeValue(c, cb->getValue());
		}
		else if(auto cs = dcast(fir::ConstantCharSlice, c))
		{
			auto str = cs->getValue();

			interp::Value ret;
			ret.dataSize = sizeof(char*);
			ret.type = cs->getType();
			ret.val = cs;

			auto s = makeGlobalString(is, str);

			setValueRaw(is, &ret, &s, sizeof(char*));

			return (cachedConstants[c] = ret);
		}
		else if(auto cbc = dcast(fir::ConstantBitcast, c))
		{
			auto thing = makeConstant(is, cbc->getValue());
			auto ret = cloneValue(cbc, thing);

			return (cachedConstants[c] = ret);
		}
		else if(auto ca = dcast(fir::ConstantArray, c))
		{
			auto bytecount = ca->getValues().size() * getSizeOfType(ca->getType()->getArrayElementType());

			auto ret = constructStructThingy(ca, bytecount, ca->getValues());
			return (cachedConstants[c] = ret);
		}
		else if(auto ct = dcast(fir::ConstantTuple, c))
		{
			size_t bytecount = 0;
			for(auto t : ct->getValues())
				bytecount += getSizeOfType(t->getType());

			auto ret = constructStructThingy(ct, bytecount, ct->getValues());
			return (cachedConstants[c] = ret);
		}
		else if(auto cec = dcast(fir::ConstantEnumCase, c))
		{
			auto bytecount = getSizeOfType(cec->getIndex()->getType()) + getSizeOfType(cec->getValue()->getType());

			auto ret = constructStructThingy(cec, bytecount, { cec->getIndex(), cec->getValue() });
			return (cachedConstants[c] = ret);
		}
		else if(auto cas = dcast(fir::ConstantArraySlice, c))
		{
			auto ptr = cas->getData();
			auto len = cas->getLength();

			auto bytecount = getSizeOfType(ptr->getType()) + getSizeOfType(len->getType());
			auto ret = constructStructThingy(cas, bytecount, { ptr, len });

			return (cachedConstants[c] = ret);
		}
		else if(auto cda = dcast(fir::ConstantDynamicArray, c))
		{
			std::vector<interp::Value> mems;
			auto bytecount = getSizeOfType(cda->getType());

			if(cda->getArray())
			{
				auto theArray = cda->getArray();
				auto sz = getSizeOfType(theArray->getType());

				void* buffer = new uint8_t[sz]; memset(buffer, 0, sz);
				is->globalAllocs.push_back(buffer);

				uint8_t* ofs = reinterpret_cast<uint8_t*>(buffer);
				for(const auto& x : theArray->getValues())
				{
					auto v = makeConstant(is, x);

					if(v.dataSize > LARGE_DATA_SIZE)    memmove(ofs, v.ptr, v.dataSize);
					else                                memmove(ofs, &v.data[0], v.dataSize);

					ofs += v.dataSize;
				}

				interp::Value fakeptr;
				fakeptr.val = 0;
				fakeptr.type = cda->getType()->getArrayElementType()->getMutablePointerTo();
				fakeptr.dataSize = sizeof(void*);

				setValueRaw(is, &fakeptr, &buffer, sizeof(void*));

				mems = {
					fakeptr, makeConstant(is, fir::ConstantInt::getNative(theArray->getValues().size())),
					makeConstant(is, fir::ConstantInt::getNative(-1)), makeConstant(is, fir::ConstantInt::getNative(0))
				};
			}
			else
			{
				mems = {
					makeConstant(is, cda->getData()), makeConstant(is, cda->getLength()),
					makeConstant(is, cda->getCapacity()), makeConstant(is, fir::ConstantInt::getNative(0))
				};
			}

			auto ret = constructStructThingy2(cda, bytecount, mems);
			return (cachedConstants[c] = ret);
		}
		else if(auto fn = dcast(fir::Function, c))
		{
			// ok -- when we get a "function" as a constant, what really happened in the source code is that we referred to a function
			// by name, without calling it. we expect, then, to get a function pointer out of it.

			// the problem is, we can also call function pointers that come from raw pointers that point to *real* CPU instructions
			// somewhere --- nobody is stopping the program from calling dlsym() or whatever and calling a function using
			// that pointer.

			// so, in order to support that, we return the fir::Function itself as a pointer. when we do the pointer-call, the
			// stub function will check the list of functions to see if it was a source-originated function, and if so call it
			// normally. if not, then we will use libffi to call it.

			// make sure we compile it first, so it gets added to InterpState::compiledFunctions
			is->compileFunction(fn);

			auto ret = makeValue(fn, reinterpret_cast<uintptr_t>(fn));
			return (cachedConstants[c] = ret);
		}
		else if(auto glob = dcast(fir::GlobalValue, c))
		{
			if(auto it = is->globals.find(c); it != is->globals.end())
				return it->second.first;

			else
				error("interp: global value '%s' id %d was not found", glob->getName().str(), glob->id);
		}
		else
		{
			auto ret = is->makeValue(c);
			return (cachedConstants[c] = ret);
		}
	}



	InterpState::InterpState(Module* mod) : module(mod)
	{
	}

	InterpState::~InterpState()
	{
		for(void* p : this->globalAllocs)
			delete[] p;
	}

	void InterpState::initialise()
	{
		for(const auto [ str, glob ] : this->module->_getGlobalStrings())
		{
			auto val = makeValue(glob);
			auto s = makeGlobalString(this, str);

			setValueRaw(this, &val, &s, sizeof(char*));

			val.globalValTracker = glob;
			this->globals[glob] = { val, false };
		}

		for(const auto [ id, glob ] : this->module->_getGlobals())
		{
			auto ty = glob->getType();
			auto sz = getSizeOfType(ty);

			void* buffer = new uint8_t[sz];
			memset(buffer, 0, sz);

			this->globalAllocs.push_back(buffer);

			if(auto init = glob->getInitialValue(); init)
			{
				auto x = makeConstant(this, init);
				if(x.dataSize > LARGE_DATA_SIZE)    memmove(buffer, x.ptr, x.dataSize);
				else                                memmove(buffer, &x.data[0], x.dataSize);
			}

			auto ret = makeValueOfType(glob, ty->getPointerTo());
			setValueRaw(this, &ret, &buffer, sizeof(void*));

			ret.globalValTracker = glob;
			this->globals[glob] = { ret, false };
		}

		for(auto intr : this->module->_getIntrinsicFunctions())
		{
			auto fname = Identifier("__interp_intrinsic_" + intr.first.str(), IdKind::Name);
			auto fn = this->module->getOrCreateFunction(fname, intr.second->getType(), fir::LinkageType::ExternalWeak);

			// interp::compileFunction already maps the newly compiled interp::Function, but since we created a
			// new function here `fn` that doesn't match the intrinsic function `intr`, we need to map that as well.
			this->compiledFunctions[intr.second] = this->compileFunction(fn);
		}


		#if OS_WINDOWS

		// ok, this is mostly for windows --- printf and friends are not *real* functions, but rather
		// macros defined in stdio.h, or something like that. so, we make "intrinsic wrappers" that call
		// it in the interpreter, which we dlopen into the context anyway.
		{
			std::vector<std::string> names = {
				"printf",
				"sprintf",
				"snprintf",
				"fprintf"
			};

			for(const auto& name : names)
			{
				auto fn = this->module->getFunction(Identifier(name, IdKind::Name));
				if(fn)
				{
					auto wrapper = this->module->getOrCreateFunction(Identifier("__interp_wrapper_" + name, IdKind::Name),
						fn->getType()->toFunctionType(), fir::LinkageType::ExternalWeak);

					this->compiledFunctions[fn] = this->compileFunction(wrapper);
				}
			}
		}
		#endif
	}


	// the purpose of this function is to write-back any changes we made to globals while interpreting,
	// back to the "value" of the global -- mainly for compile-time execution, so modifications will
	// propagate to the backend code generation.
	void InterpState::finalise()
	{
		for(const auto [ id, glob ] : this->module->_getGlobals())
		{
			// by right we are not supposed to add (or even change the FIR module at all) between calling
			// initialise() and finalise(), but be defensive a bit.
			if(auto it = this->globals.find(glob); it != this->globals.end())
			{
				// only write-back if we modified the global.
				if(it->second.second)
				{
					auto val = loadFromPtr(it->second.first, it->first->getType());
					glob->setInitialValue(this->unwrapInterpValueIntoConstant(val));
				}
			}
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


		error("interp: unsupported type '%s' in libffi-translation", ty);
	}








	static std::vector<fir::Type*> getTypeListOfType(fir::Type* ty)
	{
		if(ty->isStructType())
		{
			return ty->toStructType()->getElements();
		}
		else if(ty->isClassType())
		{
			auto ret = ty->toClassType()->getAllElementsIncludingBase();
			if(ty->toClassType()->getVirtualMethodCount() > 0)
				ret.insert(ret.begin(), fir::Type::getInt8Ptr());

			return ret;
		}
		else if(ty->isTupleType())
		{
			return ty->toTupleType()->getElements();
		}
		else if(ty->isArraySliceType())
		{
			if(ty->toArraySliceType()->isMutable())
				return { ty->getArrayElementType()->getMutablePointerTo(), fir::Type::getNativeWord() };

			else
				return { ty->getArrayElementType()->getPointerTo(), fir::Type::getNativeWord() };
		}
		else if(ty->isAnyType())
		{
			return {
				fir::Type::getNativeUWord(), fir::Type::getNativeWordPtr(),
				fir::ArrayType::get(fir::Type::getInt8(), BUILTIN_ANY_DATA_BYTECOUNT)
			};
		}
		else if(ty->isRangeType())
		{
			return {
				fir::Type::getNativeWord(), fir::Type::getNativeWord(), fir::Type::getNativeWord()
			};
		}
		else if(ty->isEnumType())
		{
			return {
				fir::Type::getNativeWord(), ty->toEnumType()->getCaseType()
			};
		}
		else if(ty->isStringType() || ty->isDynamicArrayType())
		{
			std::vector<fir::Type*> mems(4);

			if(ty->isDynamicArrayType())    mems[SAA_DATA_INDEX] = ty->getArrayElementType()->getMutablePointerTo();
			else                            mems[SAA_DATA_INDEX] = fir::Type::getMutInt8Ptr();

			mems[SAA_LENGTH_INDEX]      = fir::Type::getNativeWord();
			mems[SAA_CAPACITY_INDEX]    = fir::Type::getNativeWord();
			mems[SAA_REFCOUNTPTR_INDEX] = fir::Type::getNativeWordPtr();

			return mems;
		}
		else
		{
			error("interp: unsupported type '%s' for insert/extractvalue", ty);
		}
	}

	static interp::Value doInsertValue(interp::InterpState* is, fir::Value* res, const interp::Value& str, const interp::Value& elm, size_t idx)
	{
		// we clone the value first
		auto ret = cloneValue(res, str);

		size_t ofs = 0;

		if(str.type->isArrayType())
		{
			auto arrty = str.type->toArrayType();
			iceAssert(idx < arrty->getArraySize());

			ofs = idx * getSizeOfType(arrty->getElementType());
		}
		else if(str.type->isUnionType())
		{
			// we only support getting the id with insert/extractvalue.
			iceAssert(idx == 0);
			ofs = 0;
		}
		else
		{
			auto typelist = getTypeListOfType(str.type);

			iceAssert(idx < typelist.size());

			for(size_t i = 0; i < idx; i++)
				ofs += getSizeOfType(typelist[i]);
		}

		uintptr_t dst = 0;
		if(str.dataSize > LARGE_DATA_SIZE)  dst = reinterpret_cast<uintptr_t>(ret.ptr);
		else                                dst = reinterpret_cast<uintptr_t>(&ret.data[0]);

		uintptr_t src = 0;
		if(elm.dataSize > LARGE_DATA_SIZE)  src = reinterpret_cast<uintptr_t>(elm.ptr);
		else                                src = reinterpret_cast<uintptr_t>(&elm.data[0]);

		memmove(reinterpret_cast<void*>(dst + ofs), reinterpret_cast<void*>(src), elm.dataSize);

		return ret;
	}


	static interp::Value doExtractValue(interp::InterpState* is, fir::Value* res, const interp::Value& str, size_t idx)
	{
		size_t ofs = 0;

		fir::Type* elm = 0;
		if(str.type->isArrayType())
		{
			auto arrty = str.type->toArrayType();
			iceAssert(idx < arrty->getArraySize());

			ofs = idx * getSizeOfType(arrty->getElementType());
			elm = arrty->getElementType();
		}
		else if(str.type->isUnionType())
		{
			// we only support getting the id with insert/extractvalue.
			iceAssert(idx == 0);
			elm = fir::Type::getNativeWord();
			ofs = 0;
		}
		else
		{
			auto typelist = getTypeListOfType(str.type);

			iceAssert(idx < typelist.size());

			for(size_t i = 0; i < idx; i++)
				ofs += getSizeOfType(typelist[i]);

			elm = typelist[idx];
		}

		auto ret = is->makeValue(res);
		iceAssert(ret.type == elm);

		uintptr_t src = 0;
		if(str.dataSize > LARGE_DATA_SIZE)  src = reinterpret_cast<uintptr_t>(str.ptr);
		else                                src = reinterpret_cast<uintptr_t>(&str.data[0]);

		uintptr_t dst = 0;
		if(ret.dataSize > LARGE_DATA_SIZE)  dst = reinterpret_cast<uintptr_t>(ret.ptr);
		else                                dst = reinterpret_cast<uintptr_t>(&ret.data[0]);

		memmove(reinterpret_cast<void*>(dst), reinterpret_cast<void*>(src + ofs), ret.dataSize);

		return ret;
	}

	// this saves us a lot of copy/paste

	template <typename Functor>
	static interp::Value oneArgumentOpIntOnly(InterpState* is, fir::Type* resty, const interp::Value& a, Functor op)
	{
		auto ty = a.type;

		interp::Value res;
		res.dataSize = getSizeOfType(resty);
		res.type = resty;

		if(ty == Type::getInt8())   { auto tmp = op(getActualValue<int8_t>(a));    setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getInt16())  { auto tmp = op(getActualValue<int16_t>(a));   setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getInt32())  { auto tmp = op(getActualValue<int32_t>(a));   setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getInt64())  { auto tmp = op(getActualValue<int64_t>(a));   setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getUint8())  { auto tmp = op(getActualValue<uint8_t>(a));   setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getUint16()) { auto tmp = op(getActualValue<uint16_t>(a));  setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getUint32()) { auto tmp = op(getActualValue<uint32_t>(a));  setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getUint64()) { auto tmp = op(getActualValue<uint64_t>(a));  setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty->isPointerType())     { auto tmp = op(getActualValue<uintptr_t>(a)); setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty->isBoolType())        { auto tmp = op(getActualValue<bool>(a));      setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		else                        error("interp: unsupported type '%s'", ty);
	}

	template <typename Functor>
	static interp::Value oneArgumentOpIntOnly(InterpState* is, const interp::Instruction& inst, const interp::Value& a, Functor op)
	{
		auto ret = oneArgumentOpIntOnly(is, inst.result->getType(), a, op);
		ret.val = inst.result;

		return ret;
	}

	template <typename Functor>
	static interp::Value oneArgumentOp(InterpState* is, fir::Type* resty, const interp::Value& a, Functor op)
	{
		auto ty = a.type;

		if(!ty->isFloatingPointType())
			return oneArgumentOpIntOnly(is, resty, a, op);

		interp::Value res;
		res.dataSize = getSizeOfType(resty);
		res.type = resty;

		if(ty == Type::getFloat32())    { auto tmp = op(getActualValue<float>(a));  setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		if(ty == Type::getFloat64())    { auto tmp = op(getActualValue<double>(a)); setValueRaw(is, &res, &tmp, sizeof(tmp)); return res; }
		else                            error("interp: unsupported type '%s'", ty);
	}

	template <typename Functor>
	static interp::Value oneArgumentOp(InterpState* is, const interp::Instruction& inst, const interp::Value& a, Functor op)
	{
		auto ret = oneArgumentOp(is, inst.result->getType(), a, op);
		ret.val = inst.result;

		return ret;
	}



	template <typename Functor>
	static interp::Value twoArgumentOpIntOnly(InterpState* is, fir::Type* resty, const interp::Value& a,
		const interp::Value& b, Functor op)
	{
		auto aty = a.type;
		auto bty = b.type;

		using i8tT  = int8_t;   auto i8t  = Type::getInt8();
		using i16tT = int16_t;  auto i16t = Type::getInt16();
		using i32tT = int32_t;  auto i32t = Type::getInt32();
		using i64tT = int64_t;  auto i64t = Type::getInt64();
		using u8tT  = uint8_t;  auto u8t  = Type::getUint8();
		using u16tT = uint16_t; auto u16t = Type::getUint16();
		using u32tT = uint32_t; auto u32t = Type::getUint32();
		using u64tT = uint64_t; auto u64t = Type::getUint64();

		#define gav(t, x) getActualValue<t>(x)

		interp::Value res;
		res.dataSize = getSizeOfType(resty);
		res.type = resty;

		#define If(at, bt) do { if(aty == (at) && bty == (bt)) {    \
			auto tmp = op(gav(at##T, a), gav(bt##T, b));            \
			setValueRaw(is, &res, &tmp, sizeof(tmp));               \
			return res;                                             \
		} } while(0)

		// FUCK LAH
		If(i8t,  i8t); If(i8t,  i16t); If(i8t,  i32t); If(i8t,  i64t);
		If(i16t, i8t); If(i16t, i16t); If(i16t, i32t); If(i16t, i64t);
		If(i32t, i8t); If(i32t, i16t); If(i32t, i32t); If(i32t, i64t);
		If(i64t, i8t); If(i64t, i16t); If(i64t, i32t); If(i64t, i64t);

		If(u8t,  u8t); If(u8t,  u16t); If(u8t,  u32t); If(u8t,  u64t);
		If(u16t, u8t); If(u16t, u16t); If(u16t, u32t); If(u16t, u64t);
		If(u32t, u8t); If(u32t, u16t); If(u32t, u32t); If(u32t, u64t);
		If(u64t, u8t); If(u64t, u16t); If(u64t, u32t); If(u64t, u64t);

		if(aty->isPointerType() && bty->isPointerType())
		{
			auto tmp = op(gav(uintptr_t, a), gav(uintptr_t, b));
			setValueRaw(is, &res, &tmp, sizeof(tmp));
			return res;
		}
		if(aty->isBoolType() && bty->isBoolType())
		{
			auto tmp = op(gav(bool, a), gav(bool, b));
			setValueRaw(is, &res, &tmp, sizeof(tmp));
			return res;
		}

		error("interp: unsupported types '%s' and '%s' for arithmetic", aty, bty);

		#undef If
		#undef gav
	}


	template <typename Functor>
	static interp::Value twoArgumentOp(InterpState* is, fir::Type* resty, const interp::Value& a,
			const interp::Value& b, Functor op)
	{
		if(!(a.type->isFloatingPointType() || b.type->isFloatingPointType()))
			return twoArgumentOpIntOnly(is, resty, a, b, op);

		auto aty = a.type;
		auto bty = b.type;

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

		#define gav(t, x) getActualValue<t>(x)

		interp::Value res;
		res.dataSize = getSizeOfType(resty);
		res.type = resty;

		#define If(at, bt) do { if(aty == (at) && bty == (bt)) {    \
			auto tmp = op(gav(at##T, a), gav(bt##T, b));            \
			setValueRaw(is, &res, &tmp, sizeof(tmp));               \
			return res;                                             \
		} } while(0)

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
		#undef gav

		error("interp: unsupported types '%s' and '%s'", aty, bty);
	}




	template <typename Functor>
	static interp::Value twoArgumentOpIntOnly(InterpState* is, const interp::Instruction& inst, const interp::Value& a,
		const interp::Value& b, Functor op)
	{
		auto ret = twoArgumentOpIntOnly(is, inst.result->getType(), a, b, op);
		ret.val = inst.result;

		return ret;
	}

	template <typename Functor>
	static interp::Value twoArgumentOp(InterpState* is, const interp::Instruction& inst, const interp::Value& a,
		const interp::Value& b, Functor op)
	{
		auto ret = twoArgumentOp(is, inst.result->getType(), a, b, op);
		ret.val = inst.result;

		return ret;
	}




	static void complainAboutFFIEscape(void* fnptr, fir::Type* fnty, const std::string& name)
	{
		if(frontend::getCanFFIEscape())
			return;

		BareError* e = 0;
		if(name.empty())
		{
			e = BareError::make("interp: cannot call external function (name unavailable; symbol address %p, function type '%s')",
					fnptr, fnty->str());
		}
		else
		{
			e = BareError::make("interp: cannot call external function '%s'", name);
		}

		e->append(BareError::make(MsgType::Note, "to call external functions (ffi), pass '--ffi-escape' as a command-line flag"));
		e->postAndQuit();
	}


	static interp::Value runFunctionWithLibFFI(InterpState* is, void* fnptr, fir::FunctionType* fnty, const std::vector<interp::Value>& args,
		const std::string& nameIfAvailable = "")
	{
		complainAboutFFIEscape(fnptr, fnty, nameIfAvailable);

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

		ffi_type* ffi_retty = 0;
		ffi_cif fn_cif;
		{
			ffi_retty = convertTypeToLibFFI(fnty->getReturnType());

			if(args.size() > fnty->getArgumentCount())
			{
				iceAssert(fnty->isCStyleVarArg());
				auto st = ffi_prep_cif_var(&fn_cif, FFI_DEFAULT_ABI, fnty->getArgumentCount(), args.size(), ffi_retty, arg_types);
				if(st != FFI_OK)
					error("interp: ffi_prep_cif_var failed! (%d)", st);
			}
			else
			{
				auto st = ffi_prep_cif(&fn_cif, FFI_DEFAULT_ABI, args.size(), ffi_retty, arg_types);
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
					arg_values[i] = reinterpret_cast<void*>(const_cast<uint8_t*>(&args[i].data[0]));
			}

			for(size_t i = 0; i < args.size(); i++)
			{
				if(args[i].dataSize <= LARGE_DATA_SIZE)
					arg_pointers[i] = reinterpret_cast<void*>(arg_values[i]);

				else
					arg_pointers[i] = reinterpret_cast<void*>(args[i].ptr);
			}

			delete[] arg_values;
		}

		void* ret_buffer = new uint8_t[std::max(ffi_retty->size, size_t(8))];
		ffi_call(&fn_cif, reinterpret_cast<void(*)()>(fnptr), ret_buffer, arg_pointers);

		interp::Value ret;
		ret.type = fnty->getReturnType();
		ret.dataSize = ffi_retty->size;

		setValueRaw(is, &ret, ret_buffer, ret.dataSize);
		delete[] ret_buffer;

		delete[] arg_types;
		delete[] arg_pointers;

		return ret;
	}

	static interp::Value runFunctionWithLibFFI(InterpState* is, fir::Function* fn, const std::vector<interp::Value>& args)
	{
		void* fnptr = platform::compiler::getSymbol(fn->getName().str());
		if(!fnptr) error("interp: failed to find symbol named '%s'\n", fn->getName().str());

		return runFunctionWithLibFFI(is, fnptr, fn->getType(), args, /* name: */ fn->getName().str());
	}

	static interp::Value runFunctionWithLibFFI(InterpState* is, const interp::Function& fn, const std::vector<interp::Value>& args)
	{
		void* fnptr = platform::compiler::getSymbol(fn.func->getName().str());
		if(!fnptr) error("interp: failed to find symbol named '%s'\n", fn.func->getName().str());

		return runFunctionWithLibFFI(is, fnptr, fn.func->getType(), args, /* name: */ fn.extFuncName);
	}


	static const interp::Block* prepareFunctionToRun(InterpState* is, const interp::Function& fn, const std::vector<interp::Value>& args)
	{
		iceAssert(args.size() == fn.func->getArgumentCount());

		// when we start a function, clear the "stack frame".
		is->stackFrames.push_back({ });

		for(size_t i = 0; i < args.size(); i++)
		{
			auto farg = fn.func->getArguments()[i];
			is->stackFrames.back().values[farg] = cloneValue(farg, args[i]);
		}

		iceAssert(!fn.blocks.empty());

		auto entry = &fn.blocks[0];
		is->stackFrames.back().currentFunction = &fn;
		is->stackFrames.back().currentBlock = entry;
		is->stackFrames.back().previousBlock = 0;

		return entry;
	}

	static void leaveFunction(InterpState* is)
	{
		auto frame = is->stackFrames.back();

		for(void* alloca : frame.stackAllocs)
			delete[] alloca;

		is->stackFrames.pop_back();
	}



	constexpr int FLOW_NORMAL = 0;
	constexpr int FLOW_RETURN = 1;
	constexpr int FLOW_BRANCH = 2;
	constexpr int FLOW_FNCALL = 3;
	constexpr int FLOW_DYCALL = 4;


	struct InstrResult
	{
		// for ret
		interp::Value returnValue;

		// for branches
		const interp::Block* targetBlk = 0;

		// for calls
		interp::Function* callTarget = 0;
		std::vector<interp::Value> callArguments;
		fir::Value* callResultValue = 0;

		// for virtual calls. the args and resultvalue are shared.
		interp::Value virtualCallTarget;
	};

	static int runInstruction(InterpState* is, const interp::Instruction& inst, InstrResult* instrRes);

	static interp::Value runBlock(InterpState* is, const interp::Block* blk)
	{
		for(size_t i = 0; i < blk->instructions.size();)
		{
			is->stackFrames.back().currentInstrIndex = i;

			InstrResult res;
			int flow = runInstruction(is, blk->instructions[i], &res);

			switch(flow)
			{
				case FLOW_NORMAL: {
					i += 1;
				} break;

				case FLOW_BRANCH: {
					is->stackFrames.back().previousBlock = blk;
					is->stackFrames.back().currentBlock = res.targetBlk;

					blk = res.targetBlk; i = 0;
				} break;

				case FLOW_FNCALL: {
					if(res.callTarget->isExternal)
					{
						is->stackFrames.back().values[res.callResultValue] = runFunctionWithLibFFI(is, *res.callTarget, res.callArguments);
						i += 1;
					}
					else
					{
						is->stackFrames.back().callResultOutput = res.callResultValue;

						auto newblk = prepareFunctionToRun(is, *res.callTarget, res.callArguments);
						blk = newblk; i = 0;
					}
				} break;

				case FLOW_DYCALL: {
					auto ptr = getActualValue<uintptr_t>(res.virtualCallTarget);
					auto firfn = reinterpret_cast<fir::Function*>(ptr);

					if(auto it = is->compiledFunctions.find(firfn); it != is->compiledFunctions.end())
					{
						is->stackFrames.back().callResultOutput = res.callResultValue;

						auto newblk = prepareFunctionToRun(is, it->second, res.callArguments);
						blk = newblk; i = 0;
					}
					else
					{
						auto targ = res.virtualCallTarget;

						// uwu. use libffi.
						fir::FunctionType* fnty = 0;
						if(targ.type->isFunctionType())
							fnty = targ.type->toFunctionType();

						else if(targ.type->isPointerType() && targ.type->getPointerElementType()->isFunctionType())
							fnty = targ.type->getPointerElementType()->toFunctionType();

						else
							error("interp: call to function pointer with invalid type '%s'", targ.type);

						is->stackFrames.back().values[res.callResultValue] = runFunctionWithLibFFI(is, reinterpret_cast<void*>(ptr),
							fnty, res.callArguments, /* name: */ res.callTarget->extFuncName);

						i += 1;
					}
				} break;

				case FLOW_RETURN: {
					if(is->stackFrames.size() > 1)
					{
						leaveFunction(is);
						auto& frm = is->stackFrames.back();

						res.returnValue.val = frm.callResultOutput;
						frm.values[frm.callResultOutput] = res.returnValue;

						blk = frm.currentBlock;
						i   = frm.currentInstrIndex + 1;
						break;
					}
					else
					{
						return res.returnValue;
					}
				}

				default: {
					error("interp: invalid flow state");
				} break;
			}
		}

		error("interp: invaild state");
	}




	interp::Value InterpState::runFunction(const interp::Function& fn, const std::vector<interp::Value>& args)
	{
		auto ffn = fn.func;
		if((!fn.func->isCStyleVarArg() && args.size() != fn.func->getArgumentCount())
			|| (fn.func->isCStyleVarArg() && args.size() < fn.func->getArgumentCount()))
		{
			error("interp: mismatched argument count in call to '%s': need %d, received %d",
				fn.func->getName().str(), fn.func->getArgumentCount(), args.size());
		}

		if(fn.blocks.empty() || fn.func->isCStyleVarArg())
		{
			// it's probably an extern!
			// use libffi.
			return runFunctionWithLibFFI(this, ffn, args);
		}
		else
		{
			auto entry = prepareFunctionToRun(this, fn, args);
			auto ret = runBlock(this, entry);
			leaveFunction(this);

			return ret;
		}
	}



	static interp::Value* getVal2(InterpState* is, fir::Value* fv)
	{
		if(auto it = is->stackFrames.back().values.find(fv); it != is->stackFrames.back().values.end())
			return &it->second;

		else if(auto it2 = is->globals.find(fv); it2 != is->globals.end())
			return &it2->second.first;

		else
			return 0;
	}

	static interp::Value getVal(InterpState* is, fir::Value* fv)
	{
		if(auto hmm = getVal2(is, fv); hmm)
			return *hmm;

		else if(auto cnst = dcast(fir::ConstantValue, fv); cnst)
			return makeConstant(is, cnst);

		else
			error("interp: no value with id %d", fv->id);
	}


	static interp::Value performGEP2(InterpState* is, fir::Type* resty, const interp::Value& ptr, const interp::Value& i1, const interp::Value& i2)
	{
		iceAssert(i1.type == i2.type);

		// so, ptr should be a pointer to an array.
		iceAssert(ptr.type->isPointerType() && ptr.type->getPointerElementType()->isArrayType());

		auto arrty = ptr.type->getPointerElementType();
		auto elmty = arrty->getArrayElementType();

		auto ofs = twoArgumentOp(is, resty, i1, i2, [arrty, elmty](auto a, auto b) -> auto {
			return (getSizeOfType(arrty) * a) + (getSizeOfType(elmty) * b);
		});

		auto realptr = getActualValue<uintptr_t>(ptr);
		auto ret = oneArgumentOp(is, resty, ofs, [realptr](auto b) -> auto {
			// this is not pointer arithmetic!!
			return realptr + b;
		});

		ret.globalValTracker = ptr.globalValTracker;
		return ret;
	}


	static interp::Value performStructGEP(InterpState* is, fir::Type* resty, const interp::Value& str, uint64_t idx)
	{
		iceAssert(str.type->isPointerType());
		auto strty = str.type->getPointerElementType();

		if(!strty->isStructType() && !strty->isClassType() && !strty->isTupleType())
			error("interp: unsupported type '%s' for struct gep", strty);

		std::vector<fir::Type*> elms = getTypeListOfType(strty);

		size_t ofs = 0;
		for(uint64_t i = 0; i < idx; i++)
			ofs += getSizeOfType(elms[i]);

		uintptr_t src = getActualValue<uintptr_t>(str);
		src += ofs;

		auto ret = cloneValue(str);
		ret.type = resty;
		setValueRaw(is, &ret, &src, sizeof(src));

		ret.globalValTracker = str.globalValTracker;
		return ret;
	}

	static interp::Value decay(const interp::Value& val)
	{
		if(val.val && val.val->islvalue())
		{
			auto ret = loadFromPtr(val, val.val->getType());
			ret.val = val.val;

			return ret;
		}
		else
		{
			return val;
		}
	}

	static interp::Value& getUndecayedArg(InterpState* is, const interp::Instruction& inst, size_t i)
	{
		iceAssert(i < inst.args.size());
		auto ret = getVal2(is, inst.args[i]);
		iceAssert(ret);

		return *ret;
	}

	static interp::Value getArg(InterpState* is, const interp::Instruction& inst, size_t i)
	{
		iceAssert(i < inst.args.size());
		return decay(getVal(is, inst.args[i]));
	}

	static void setRet(InterpState* is, const interp::Instruction& inst, const interp::Value& val)
	{
		is->stackFrames.back().values[inst.result] = val;
	}

	static bool areTypesSufficientlyEqual(fir::Type* a, fir::Type* b)
	{
		return a == b || (a->isPointerType() && b->isPointerType() && a->getPointerElementType() == b->getPointerElementType());
	}


	// returns either FLOW_NORMAL, FLOW_BRANCH or FLOW_RETURN.
	static int runInstruction(InterpState* is, const interp::Instruction& inst, InstrResult* instrRes)
	{
		auto ok = static_cast<OpKind>(inst.opcode);
		switch(ok)
		{
			case OpKind::Signed_Add:
			case OpKind::Unsigned_Add:
			case OpKind::Floating_Add:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a + b;
				}));
				break;
			}

			case OpKind::Signed_Sub:
			case OpKind::Unsigned_Sub:
			case OpKind::Floating_Sub:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a - b;
				}));
				break;
			}

			case OpKind::Signed_Mul:
			case OpKind::Unsigned_Mul:
			case OpKind::Floating_Mul:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a * b;
				}));
				break;
			}

			case OpKind::Signed_Div:
			case OpKind::Unsigned_Div:
			case OpKind::Floating_Div:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a / b;
				}));
				break;
			}

			case OpKind::Signed_Mod:
			case OpKind::Unsigned_Mod:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOpIntOnly(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a % b;
				}));
				break;
			}

			case OpKind::Floating_Mod:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return fmod(a, b);
				}));
				break;
			}


			case OpKind::ICompare_Equal:
			case OpKind::FCompare_Equal_ORD:
			case OpKind::FCompare_Equal_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> bool {
					return a == b;
				}));
				break;
			}

			case OpKind::ICompare_NotEqual:
			case OpKind::FCompare_NotEqual_ORD:
			case OpKind::FCompare_NotEqual_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> bool {
					return a != b;
				}));
				break;
			}

			case OpKind::ICompare_Greater:
			case OpKind::FCompare_Greater_ORD:
			case OpKind::FCompare_Greater_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> bool {
					return a > b;
				}));
				break;
			}

			case OpKind::ICompare_Less:
			case OpKind::FCompare_Less_ORD:
			case OpKind::FCompare_Less_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> bool {
					return a < b;
				}));
				break;
			}


			case OpKind::ICompare_GreaterEqual:
			case OpKind::FCompare_GreaterEqual_ORD:
			case OpKind::FCompare_GreaterEqual_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> bool {
					return a >= b;
				}));
				break;
			}

			case OpKind::ICompare_LessEqual:
			case OpKind::FCompare_LessEqual_ORD:
			case OpKind::FCompare_LessEqual_UNORD:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> bool {
					return a <= b;
				}));
				break;
			}

			case OpKind::ICompare_Multi:
			case OpKind::FCompare_Multi:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> int {
					if(a == b)  return 0;
					if(a > b)   return 1;
					else        return -1;
				}));
				break;
			}

			case OpKind::Bitwise_Xor:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOpIntOnly(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a ^ b;
				}));
				break;
			}

			case OpKind::Bitwise_Logical_Shr:
			case OpKind::Bitwise_Arithmetic_Shr:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(a.type->isIntegerType());
				setRet(is, inst, twoArgumentOpIntOnly(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a >> b;
				}));
				break;
			}

			case OpKind::Bitwise_Shl:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(a.type->isIntegerType());
				setRet(is, inst, twoArgumentOpIntOnly(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a << b;
				}));
				break;
			}

			case OpKind::Bitwise_And:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOpIntOnly(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a & b;
				}));
				break;
			}

			case OpKind::Bitwise_Or:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(areTypesSufficientlyEqual(a.type, b.type));
				setRet(is, inst, twoArgumentOpIntOnly(is, inst, a, b, [](auto a, auto b) -> decltype(a) {
					return a | b;
				}));
				break;
			}

			case OpKind::Signed_Neg:
			case OpKind::Floating_Neg:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);

				setRet(is, inst, oneArgumentOp(is, inst, a, [](auto a) -> auto {
					return -1 * a;
				}));
				break;
			}

			case OpKind::Bitwise_Not:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);

				setRet(is, inst, oneArgumentOpIntOnly(is, inst, a, [](auto a) -> auto {
					return ~a;
				}));
				break;
			}

			case OpKind::Logical_Not:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);

				setRet(is, inst, oneArgumentOpIntOnly(is, inst, a, [](auto a) -> auto {
					return !a;
				}));
				break;
			}

			case OpKind::Floating_Truncate:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);
				auto t = inst.args[1]->getType();

				interp::Value ret;
				if(a.type == Type::getFloat64() && t == Type::getFloat32())
					ret = makeValue(inst.result, static_cast<float>(getActualValue<double>(a)));

				else if(a.type == Type::getFloat32())   ret = makeValue(inst.result, getActualValue<float>(a));
				else if(a.type == Type::getFloat64())   ret = makeValue(inst.result, getActualValue<double>(a));
				else                                    error("interp: unsupported");

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Floating_Extend:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);
				auto t = inst.args[1]->getType();

				interp::Value ret;
				if(a.type == Type::getFloat32() && t == Type::getFloat64())
					ret = makeValue(inst.result, static_cast<double>(getActualValue<float>(a)));

				else if(a.type == Type::getFloat32())   ret = makeValue(inst.result, getActualValue<float>(a));
				else if(a.type == Type::getFloat64())   ret = makeValue(inst.result, getActualValue<double>(a));
				else                                    error("interp: unsupported");

				setRet(is, inst, ret);
				break;
			}




			case OpKind::Value_WritePtr:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				if(a.type != b.type->getPointerElementType())
					error("interp: cannot write '%s' into '%s'", a.type, b.type);

				auto ptr = reinterpret_cast<void*>(getActualValue<uintptr_t>(b));
				if(a.dataSize > LARGE_DATA_SIZE)
				{
					// just a memcopy.
					memmove(ptr, a.ptr, a.dataSize);
				}
				else
				{
					// still a memcopy, but slightly more involved.
					memmove(ptr, &a.data[0], a.dataSize);
				}

				break;
			}

			case OpKind::Value_ReadPtr:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);

				auto ty = a.type->getPointerElementType();

				auto ret = loadFromPtr(a, ty);
				ret.val = inst.result;

				setRet(is, inst, ret);
				break;
			}



			case OpKind::Value_CreatePHI:
			{
				iceAssert(inst.args.size() == 1);
				auto phi = dcast(fir::PHINode, inst.result);
				iceAssert(phi);

				// make the empty thing first
				auto val = is->makeValue(inst.result);

				bool found = false;
				for(auto [ blk, v ] : phi->getValues())
				{
					if(blk == is->stackFrames.back().previousBlock->blk)
					{
						setValue(is, &val, decay(getVal(is, v)));
						found = true;
						break;
					}
				}

				if(!found) error("interp: predecessor was not listed in the PHI node (id %d)!", phi->id);

				setRet(is, inst, val);
				break;
			}





			case OpKind::Value_Return:
			{
				if(!inst.args.empty())
					instrRes->returnValue = getArg(is, inst, 0);

				return FLOW_RETURN;
			}

			case OpKind::Branch_UnCond:
			{
				iceAssert(inst.args.size() == 1);
				auto blk = inst.args[0];

				const interp::Block* target = 0;
				for(const auto& b : is->stackFrames.back().currentFunction->blocks)
				{
					if(b.blk == blk)
					{
						target = &b;
						break;
					}
				}

				if(!target) error("interp: branch to block %d not in current function", blk->id);

				instrRes->targetBlk = target;
				return FLOW_BRANCH;
			}

			case OpKind::Branch_Cond:
			{
				iceAssert(inst.args.size() == 3);
				auto cond = getArg(is, inst, 0);
				iceAssert(cond.type->isBoolType());

				const interp::Block* trueblk = 0;
				const interp::Block* falseblk = 0;
				for(const auto& b : is->stackFrames.back().currentFunction->blocks)
				{
					if(b.blk == inst.args[1])
						trueblk = &b;

					else if(b.blk == inst.args[2])
						falseblk = &b;

					if(trueblk && falseblk)
						break;
				}

				if(!trueblk || !falseblk) error("interp: branch to blocks %d or %d not in current function", trueblk->blk->id, falseblk->blk->id);


				if(getActualValue<bool>(cond))
					instrRes->targetBlk = trueblk;

				else
					instrRes->targetBlk = falseblk;

				return FLOW_BRANCH;
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

					if(!target) error("interp: no function %d (name '%s')", fn->id, fn->getName().str());
				}

				iceAssert(target);

				std::vector<interp::Value> args;
				for(size_t i = 1; i < inst.args.size(); i++)
					args.push_back(getArg(is, inst, i));

				instrRes->callTarget = target;
				instrRes->callArguments = args;
				instrRes->callResultValue = inst.result;

				return FLOW_FNCALL;
			}



			case OpKind::Value_CallFunctionPointer:
			{
				iceAssert(inst.args.size() >= 1);
				auto fn = getArg(is, inst, 0);

				std::vector<interp::Value> args;
				for(size_t i = 1; i < inst.args.size(); i++)
					args.push_back(getArg(is, inst, i));

				instrRes->callArguments = args;
				instrRes->virtualCallTarget = fn;
				instrRes->callResultValue = inst.result;

				return FLOW_DYCALL;
			}


			case OpKind::Value_CallVirtualMethod:
			{
				// args are: 0. classtype, 1. index, 2. functiontype, 3...N args
				auto clsty = inst.args[0]->getType()->toClassType();
				auto fnty = inst.args[2]->getType()->toFunctionType();
				iceAssert(clsty);

				std::vector<interp::Value> args;
				for(size_t i = 3; i < inst.args.size(); i++)
					args.push_back(getArg(is, inst, i));

				//* this is very hacky! we rely on these things not using ::val, because it's null!!
				auto vtable = loadFromPtr(performStructGEP(is, fir::Type::getInt8Ptr(), args[0], 0), fir::Type::getInt8Ptr());
				auto vtablety = fir::ArrayType::get(fir::FunctionType::get({ }, fir::Type::getVoid())->getPointerTo(), clsty->getVirtualMethodCount());
				vtable.type = vtablety->getPointerTo();

				vtable = performGEP2(is, vtablety->getPointerTo(), vtable, makeConstant(is, fir::ConstantInt::getNative(0)), getArg(is, inst, 1));

				auto fnptr = loadFromPtr(vtable, fnty->getPointerTo());

				instrRes->callArguments = args;
				instrRes->virtualCallTarget = fnptr;
				instrRes->callResultValue = inst.result;

				return FLOW_DYCALL;
			}





			case OpKind::Cast_IntSize:
			case OpKind::Integer_ZeroExt:
			case OpKind::Integer_Truncate:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				interp::Value ret;

				if(a.type->isBoolType())
				{
					// since we don't want to add more cases to the thingy, we do a hack --
					// just get the value as a bool, then cast it to an i64 ourselves, then
					// pass *that* to the ops.

					auto boolval = static_cast<int64_t>(getActualValue<bool>(a));
					ret = oneArgumentOp(is, inst, b, [boolval](auto b) -> auto {
						return static_cast<decltype(b)>(boolval);
					});
				}
				else if(b.type->isBoolType())
				{
					ret = oneArgumentOp(is, inst, a, [](auto a) -> auto {
						return static_cast<bool>(a);
					});
				}
				else
				{
					ret = twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> auto {
						return static_cast<decltype(b)>(a);
					});
				}

				setRet(is, inst, ret);
				break;
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

				auto v = cloneValue(inst.result, getArg(is, inst, 0));
				v.type = inst.args[1]->getType();

				setRet(is, inst, v);
				break;
			}

			case OpKind::Cast_FloatToInt:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				interp::Value ret = twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> auto {
					return static_cast<decltype(b)>(a);
				});

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Cast_IntToFloat:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				interp::Value ret = twoArgumentOp(is, inst, a, b, [](auto a, auto b) -> auto {
					return static_cast<decltype(b)>(a);
				});

				setRet(is, inst, ret);
				break;
			}


			case OpKind::Value_GetPointer:
			{
				// equivalent to GEP(ptr*, index)
				iceAssert(inst.args.size() == 2);
				auto ptr = getArg(is, inst, 0);
				auto b = getArg(is, inst, 1);

				iceAssert(ptr.type->isPointerType());
				iceAssert(b.type->isIntegerType());

				auto elmty = ptr.type->getPointerElementType();

				auto realptr = getActualValue<uintptr_t>(ptr);
				setRet(is, inst, oneArgumentOp(is, inst, b, [realptr, elmty](auto b) -> auto {
					// this doesn't do pointer arithmetic!! if it's a pointer type, the value we get
					// will be a uintptr_t.
					return realptr + (getSizeOfType(elmty) * b);
				}));

				break;
			}

			case OpKind::Value_GetGEP2:
			{
				// equivalent to GEP(ptr*, index1, index2)
				iceAssert(inst.args.size() == 3);
				auto ptr = getArg(is, inst, 0);
				auto i1 = getArg(is, inst, 1);
				auto i2 = getArg(is, inst, 2);

				auto ret = performGEP2(is, inst.result->getType(), ptr, i1, i2);
				ret.val = inst.result;

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Value_GetStructMember:
			{
				// equivalent to GEP(ptr*, 0, memberIndex)
				iceAssert(inst.args.size() == 2);
				auto str = getUndecayedArg(is, inst, 0);
				auto idx = dcast(fir::ConstantInt, inst.args[1])->getUnsignedValue();

				auto ret = performStructGEP(is, inst.result->getType()->getPointerTo(), str, idx);
				ret.val = inst.result;

				setRet(is, inst, ret);
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
				auto ty = inst.args[0]->getType();

				auto ci = fir::ConstantInt::getNative(getSizeOfType(ty));

				if(fir::getNativeWordSizeInBits() == 64) setRet(is, inst, makeValue(inst.result, static_cast<int64_t>(ci->getSignedValue())));
				if(fir::getNativeWordSizeInBits() == 32) setRet(is, inst, makeValue(inst.result, static_cast<int32_t>(ci->getSignedValue())));
				if(fir::getNativeWordSizeInBits() == 16) setRet(is, inst, makeValue(inst.result, static_cast<int16_t>(ci->getSignedValue())));
				if(fir::getNativeWordSizeInBits() == 8)  setRet(is, inst, makeValue(inst.result, static_cast<int8_t>(ci->getSignedValue())));

				break;
			}


			case OpKind::Value_CreateLVal:
			case OpKind::Value_StackAlloc:
			{
				iceAssert(inst.args.size() == 1);

				auto ty = inst.args[0]->getType();
				auto sz = getSizeOfType(ty);

				void* buffer = new uint8_t[sz];
				memset(buffer, 0, sz);

				is->stackFrames.back().stackAllocs.push_back(buffer);

				auto ret = makeValueOfType(inst.result, ty->getPointerTo());
				setValueRaw(is, &ret, &buffer, sizeof(void*));

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Value_Store:
			{
				iceAssert(inst.args.size() == 2);
				auto a = getArg(is, inst, 0);
				auto b = getUndecayedArg(is, inst, 1);

				auto ptr = reinterpret_cast<void*>(getActualValue<uintptr_t>(b));
				if(a.dataSize > LARGE_DATA_SIZE)    memmove(ptr, a.ptr, a.dataSize);
				else                                memmove(ptr, &a.data[0], a.dataSize);

				if(b.globalValTracker)
				{
					// set the modified flag to true!
					is->globals[b.globalValTracker].second = true;
				}
				break;
			}

			case OpKind::Value_AddressOf:
			{
				iceAssert(inst.args.size() == 1);
				auto ret = cloneValue(inst.result, getUndecayedArg(is, inst, 0));

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Value_Dereference:
			{
				iceAssert(inst.args.size() == 1);
				auto a = getArg(is, inst, 0);
				auto ret = cloneValue(inst.result, a);

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Value_Select:
			{
				iceAssert(inst.args.size() == 3);
				auto cond = getArg(is, inst, 0);
				iceAssert(cond.type->isBoolType());

				auto trueval = getArg(is, inst, 1);
				auto falseval = getArg(is, inst, 2);

				if(getActualValue<bool>(cond))  setRet(is, inst, trueval);
				else                            setRet(is, inst, falseval);

				break;
			}



			case OpKind::Value_InsertValue:
			{
				iceAssert(inst.args.size() == 3);

				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);
				auto idx = static_cast<size_t>(getActualValue<int64_t>(getArg(is, inst, 2)));

				setRet(is, inst, doInsertValue(is, inst.result, str, elm, idx));
				break;
			}


			case OpKind::Value_ExtractValue:
			{
				iceAssert(inst.args.size() >= 2);

				auto str = getArg(is, inst, 0);
				auto idx = static_cast<size_t>(getActualValue<int64_t>(getArg(is, inst, 1)));

				setRet(is, inst, doExtractValue(is, inst.result, str, idx));
				break;
			}


			case OpKind::SAA_GetData:
			case OpKind::SAA_GetLength:
			case OpKind::SAA_GetCapacity:
			case OpKind::SAA_GetRefCountPtr:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(is, inst, 0);

				interp::Value ret;

				if(ok == OpKind::SAA_GetData)
					ret = doExtractValue(is, inst.result, str, SAA_DATA_INDEX);

				else if(ok == OpKind::SAA_GetLength)
					ret = doExtractValue(is, inst.result, str, SAA_LENGTH_INDEX);

				else if(ok == OpKind::SAA_GetCapacity)
					ret = doExtractValue(is, inst.result, str, SAA_CAPACITY_INDEX);

				else if(ok == OpKind::SAA_GetRefCountPtr)
					ret = doExtractValue(is, inst.result, str, SAA_REFCOUNTPTR_INDEX);

				setRet(is, inst, ret);
				break;
			}

			case OpKind::SAA_SetData:
			case OpKind::SAA_SetLength:
			case OpKind::SAA_SetCapacity:
			case OpKind::SAA_SetRefCountPtr:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);

				interp::Value ret;

				if(ok == OpKind::SAA_SetData)
					ret = doInsertValue(is, inst.result, str, elm, SAA_DATA_INDEX);

				else if(ok == OpKind::SAA_SetLength)
					ret = doInsertValue(is, inst.result, str, elm, SAA_LENGTH_INDEX);

				else if(ok == OpKind::SAA_SetCapacity)
					ret = doInsertValue(is, inst.result, str, elm, SAA_CAPACITY_INDEX);

				else if(ok == OpKind::SAA_SetRefCountPtr)
					ret = doInsertValue(is, inst.result, str, elm, SAA_REFCOUNTPTR_INDEX);

				setRet(is, inst, ret);
				break;
			}

			case OpKind::ArraySlice_GetData:
			case OpKind::ArraySlice_GetLength:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(is, inst, 0);

				interp::Value ret;

				if(ok == OpKind::ArraySlice_GetData)
					ret = doExtractValue(is, inst.result, str, SLICE_DATA_INDEX);

				else if(ok == OpKind::ArraySlice_GetLength)
					ret = doExtractValue(is, inst.result, str, SLICE_LENGTH_INDEX);

				setRet(is, inst, ret);
				break;
			}



			case OpKind::ArraySlice_SetData:
			case OpKind::ArraySlice_SetLength:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);

				interp::Value ret;

				if(ok == OpKind::ArraySlice_SetData)
					ret = doInsertValue(is, inst.result, str, elm, SLICE_DATA_INDEX);

				else if(ok == OpKind::ArraySlice_SetLength)
					ret = doInsertValue(is, inst.result, str, elm, SLICE_LENGTH_INDEX);

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Any_GetData:
			case OpKind::Any_GetTypeID:
			case OpKind::Any_GetRefCountPtr:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(is, inst, 0);

				interp::Value ret;

				if(ok == OpKind::Any_GetTypeID)
					ret = doExtractValue(is, inst.result, str, ANY_TYPEID_INDEX);

				else if(ok == OpKind::Any_GetRefCountPtr)
					ret = doExtractValue(is, inst.result, str, ANY_REFCOUNTPTR_INDEX);

				else if(ok == OpKind::Any_GetData)
					ret = doExtractValue(is, inst.result, str, ANY_DATA_ARRAY_INDEX);

				setRet(is, inst, ret);
				break;
			}

			case OpKind::Any_SetData:
			case OpKind::Any_SetTypeID:
			case OpKind::Any_SetRefCountPtr:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);

				interp::Value ret;

				if(ok == OpKind::Any_SetTypeID)
					ret = doInsertValue(is, inst.result, str, elm, ANY_TYPEID_INDEX);

				else if(ok == OpKind::Any_SetRefCountPtr)
					ret = doInsertValue(is, inst.result, str, elm, ANY_REFCOUNTPTR_INDEX);

				else if(ok == OpKind::Any_SetData)
					ret = doInsertValue(is, inst.result, str, elm, ANY_DATA_ARRAY_INDEX);

				setRet(is, inst, ret);
				break;
			}



			case OpKind::Range_GetLower:
			case OpKind::Range_GetUpper:
			case OpKind::Range_GetStep:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(is, inst, 0);

				interp::Value ret;

				if(ok == OpKind::Range_GetLower)
					ret = doExtractValue(is, inst.result, str, 0);

				else if(ok == OpKind::Range_GetUpper)
					ret = doExtractValue(is, inst.result, str, 1);

				else if(ok == OpKind::Range_GetStep)
					ret = doExtractValue(is, inst.result, str, 2);

				setRet(is, inst, ret);
				break;
			}


			case OpKind::Range_SetLower:
			case OpKind::Range_SetUpper:
			case OpKind::Range_SetStep:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);

				interp::Value ret;

				if(ok == OpKind::Range_SetLower)
					ret = doInsertValue(is, inst.result, str, elm, 0);

				else if(ok == OpKind::Range_SetUpper)
					ret = doInsertValue(is, inst.result, str, elm, 1);

				else if(ok == OpKind::Range_SetStep)
					ret = doInsertValue(is, inst.result, str, elm, 2);

				setRet(is, inst, ret);
				break;
			}



			case OpKind::Enum_GetIndex:
			case OpKind::Enum_GetValue:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(is, inst, 0);

				interp::Value ret;

				if(ok == OpKind::Enum_GetIndex)
					ret = doExtractValue(is, inst.result, str, 0);

				else if(ok == OpKind::Enum_GetValue)
					ret = doExtractValue(is, inst.result, str, 1);

				setRet(is, inst, ret);
				break;
			}


			case OpKind::Enum_SetIndex:
			case OpKind::Enum_SetValue:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);

				interp::Value ret;

				if(ok == OpKind::Enum_SetIndex)
					ret = doInsertValue(is, inst.result, str, elm, 0);

				else if(ok == OpKind::Enum_SetValue)
					ret = doInsertValue(is, inst.result, str, elm, 1);

				setRet(is, inst, ret);
				break;
			}


			case OpKind::Union_GetVariantID:
			{
				iceAssert(inst.args.size() == 1);
				auto str = getArg(is, inst, 0);

				setRet(is, inst, doExtractValue(is, inst.result, str, 0));
				break;
			}

			case OpKind::Union_SetVariantID:
			{
				iceAssert(inst.args.size() == 2);
				auto str = getArg(is, inst, 0);
				auto elm = getArg(is, inst, 1);

				setRet(is, inst, doInsertValue(is, inst.result, str, elm, 0));
				break;
			}

			case OpKind::Union_GetValue:
			{
				iceAssert(inst.args.size() == 2);
				iceAssert(inst.args[0]->getType()->isUnionType());

				auto ut = inst.args[0]->getType()->toUnionType();
				auto vid = dcast(fir::ConstantInt, inst.args[1])->getSignedValue();

				iceAssert(static_cast<size_t>(vid) < ut->getVariantCount());
				auto vt = ut->getVariant(vid)->getInteriorType();

				// because we can operate with the raw memory values, we can probably do this a bit more efficiently
				// than we can with LLVM, where we needed to create a temporary stack value to store the thing from
				// the extractvalue so we could cast-to-pointer then load.

				// first we just get the argument:
				auto theUnion = getArg(is, inst, 0);

				// then, get the array:
				uintptr_t arrayAddr = 0;
				if(theUnion.dataSize > LARGE_DATA_SIZE) arrayAddr = reinterpret_cast<uintptr_t>(theUnion.ptr);
				else                                    arrayAddr = reinterpret_cast<uintptr_t>(&theUnion.data[0]);

				// offset it appropriately:
				arrayAddr += getSizeOfType(fir::Type::getNativeWord());

				// ok so now we just do a 'setRaw' to get the value out.
				auto ret = is->makeValue(inst.result);
				setValueRaw(is, &ret, reinterpret_cast<void*>(arrayAddr), getSizeOfType(vt));

				setRet(is, inst, ret);
				break;
			}


			case OpKind::Union_SetValue:
			{
				iceAssert(inst.args.size() == 3);
				iceAssert(inst.args[0]->getType()->isUnionType());

				auto ut = inst.args[0]->getType()->toUnionType();
				auto vid = static_cast<intptr_t>(dcast(fir::ConstantInt, inst.args[1])->getSignedValue());

				iceAssert(static_cast<size_t>(vid) < ut->getVariantCount());

				// again, we do this "manually" because we can access the raw bytes, so we don't have to
				// twist ourselves through hoops like with llvm.

				// first we just get the argument:
				auto theUnion = cloneValue(inst.result, getArg(is, inst, 0));

				// then, get the array:
				uintptr_t baseAddr = 0;
				if(theUnion.dataSize > LARGE_DATA_SIZE) baseAddr = reinterpret_cast<uintptr_t>(theUnion.ptr);
				else                                    baseAddr = reinterpret_cast<uintptr_t>(&theUnion.data[0]);

				// offset it appropriately:
				auto arrayAddr = baseAddr + getSizeOfType(fir::Type::getNativeWord());

				// ok, now we just do a memcpy into the struct.
				iceAssert(sizeof(intptr_t) == (fir::getNativeWordSizeInBits() / CHAR_BIT));
				memmove(reinterpret_cast<void*>(baseAddr), &vid, sizeof(intptr_t));

				uintptr_t valueAddr = 0;
				auto theValue = getArg(is, inst, 2);
				if(theValue.dataSize > LARGE_DATA_SIZE) valueAddr = reinterpret_cast<uintptr_t>(theValue.ptr);
				else                                    valueAddr = reinterpret_cast<uintptr_t>(&theValue.data[0]);

				memmove(reinterpret_cast<void*>(arrayAddr), reinterpret_cast<void*>(valueAddr), theValue.dataSize);

				setRet(is, inst, theUnion);
				break;
			}


			case OpKind::RawUnion_GEP:
			{
				iceAssert(inst.args.size() == 2);
				auto targtype = inst.args[1]->getType();

				// again. just manipulate the memory.
				auto unn = getUndecayedArg(is, inst, 0);
				auto buffer = getActualValue<uintptr_t>(unn);

				auto ret = makeValueOfType(inst.result, targtype->getPointerTo());
				setValueRaw(is, &ret, &buffer, sizeof(uintptr_t));

				setRet(is, inst, ret);
				break;
			}



			case OpKind::Unreachable:
			{
				error("interp: unreachable op!");
			}

			case OpKind::Invalid:
			default:
			{
				// note we don't use "default" to catch
				// new opkinds that we forget to add.
				error("interp: invalid opcode %d!", inst.opcode);
			}
		}

		return FLOW_NORMAL;
	}


	fir::ConstantValue* InterpState::unwrapInterpValueIntoConstant(const interp::Value& val)
	{
		auto ty = val.type;

		#define gav getActualValue

		if(ty->isPrimitiveType() || ty->isBoolType())
		{
			if(ty == fir::Type::getBool())          return fir::ConstantBool::get(gav<bool>(val));
			else if(ty == fir::Type::getInt8())     return fir::ConstantInt::get(ty, gav<int8_t>(val));
			else if(ty == fir::Type::getInt16())    return fir::ConstantInt::get(ty, gav<int16_t>(val));
			else if(ty == fir::Type::getInt32())    return fir::ConstantInt::get(ty, gav<int32_t>(val));
			else if(ty == fir::Type::getInt64())    return fir::ConstantInt::get(ty, gav<int64_t>(val));
			else if(ty == fir::Type::getUint8())    return fir::ConstantInt::get(ty, gav<uint8_t>(val));
			else if(ty == fir::Type::getUint16())   return fir::ConstantInt::get(ty, gav<uint16_t>(val));
			else if(ty == fir::Type::getUint32())   return fir::ConstantInt::get(ty, gav<uint32_t>(val));
			else if(ty == fir::Type::getUint64())   return fir::ConstantInt::get(ty, gav<uint64_t>(val));
			else if(ty == fir::Type::getFloat32())  return fir::ConstantFP::get(ty, gav<float>(val));
			else if(ty == fir::Type::getFloat64())  return fir::ConstantFP::get(ty, gav<double>(val));
		}

		#undef gav

		error("interp: cannot unwrap type '%s'", ty);
	}
}
}


#ifdef _MSC_VER
	#pragma warning(pop)
#else
	#pragma GCC diagnostic pop
#endif
















