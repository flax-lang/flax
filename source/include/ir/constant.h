// constant.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "value.h"
#include "mpreal/mpreal.h"

namespace fir
{
	struct Value;
	struct ConstantValue;

	// base class implicitly stores null
	struct ConstantValue : Value
	{
		friend struct Module;
		friend struct IRBuilder;

		// static stuff
		static ConstantValue* getZeroValue(Type* type);
		static ConstantValue* getNull();


		protected:
		ConstantValue(Type* type);
	};

	struct ConstantNumber : ConstantValue
	{
		friend struct Module;

		static ConstantNumber* get(ConstantNumberType* cnt, const mpfr::mpreal& n);

		int8_t getInt8()        { return static_cast<int8_t>(this->number.toLLong()); }
		int16_t getInt16()      { return static_cast<int16_t>(this->number.toLLong()); }
		int32_t getInt32()      { return static_cast<int32_t>(this->number.toLLong()); }
		int64_t getInt64()      { return static_cast<int64_t>(this->number.toLLong()); }
		uint8_t getUint8()      { return static_cast<uint8_t>(this->number.toULLong()); }
		uint16_t getUint16()    { return static_cast<uint16_t>(this->number.toULLong()); }
		uint32_t getUint32()    { return static_cast<uint32_t>(this->number.toULLong()); }
		uint64_t getUint64()    { return static_cast<uint64_t>(this->number.toULLong()); }

		float getFloat()        { return this->number.toFloat(); }
		double getDouble()      { return this->number.toDouble(); }

		protected:
		ConstantNumber(ConstantNumberType* cnt, const mpfr::mpreal& n);

		mpfr::mpreal number;
	};

	struct ConstantBool : ConstantValue
	{
		friend struct Module;

		static ConstantBool* get(bool value);
		bool getValue();

		protected:
		ConstantBool(bool val);

		bool value;
	};

	struct ConstantInt : ConstantValue
	{
		friend struct Module;

		static ConstantInt* get(Type* intType, uint64_t val);

		static ConstantInt* getInt8(int8_t value);
		static ConstantInt* getInt16(int16_t value);
		static ConstantInt* getInt32(int32_t value);
		static ConstantInt* getInt64(int64_t value);
		static ConstantInt* getUint8(uint8_t value);
		static ConstantInt* getUint16(uint16_t value);
		static ConstantInt* getUint32(uint32_t value);
		static ConstantInt* getUint64(uint64_t value);

		static ConstantInt* getNative(int64_t value);
		static ConstantInt* getUNative(uint64_t value);

		int64_t getSignedValue();
		uint64_t getUnsignedValue();

		protected:
		ConstantInt(Type* type, int64_t val);
		ConstantInt(Type* type, uint64_t val);

		uint64_t value;
	};


	struct ConstantFP : ConstantValue
	{
		friend struct Module;

		static ConstantFP* get(Type* intType, float val);
		static ConstantFP* get(Type* intType, double val);

		static ConstantFP* getFloat32(float value);
		static ConstantFP* getFloat64(double value);

		double getValue();

		protected:
		ConstantFP(Type* type, float val);
		ConstantFP(Type* type, double val);

		double value;
	};

	struct ConstantArray : ConstantValue
	{
		friend struct Module;

		static ConstantArray* get(Type* type, std::vector<ConstantValue*> vals);

		std::vector<ConstantValue*> getValues() { return this->values; }

		protected:
		ConstantArray(Type* type, std::vector<ConstantValue*> vals);

		std::vector<ConstantValue*> values;
	};

	struct ConstantStruct : ConstantValue
	{
		friend struct Module;

		static ConstantStruct* get(StructType* st, std::vector<ConstantValue*> members);

		protected:
		ConstantStruct(StructType* st, std::vector<ConstantValue*> members);
		std::vector<ConstantValue*> members;
	};

	struct ConstantEnumCase : ConstantValue
	{
		friend struct Module;

		static ConstantEnumCase* get(EnumType* en, ConstantInt* index, ConstantValue* value);

		ConstantInt* getIndex();
		ConstantValue* getValue();

		protected:
		ConstantEnumCase(EnumType* en, ConstantInt* index, ConstantValue* value);

		ConstantInt* index = 0;
		ConstantValue* value = 0;
	};

	struct ConstantCharSlice : ConstantValue
	{
		friend struct Module;

		static ConstantCharSlice* get(std::string value);
		std::string getValue();

		protected:
		ConstantCharSlice(std::string str);

		std::string str;
	};

	struct ConstantTuple : ConstantValue
	{
		friend struct Module;

		static ConstantTuple* get(std::vector<ConstantValue*> mems);
		std::vector<ConstantValue*> getValues();

		protected:
		ConstantTuple(std::vector<ConstantValue*> mems);
		std::vector<ConstantValue*> values;
	};

	struct ConstantDynamicArray : ConstantValue
	{
		friend struct Module;

		static ConstantDynamicArray* get(DynamicArrayType* type, ConstantValue* data, ConstantValue* length, ConstantValue* capacity);
		static ConstantDynamicArray* get(ConstantArray* arr);

		ConstantValue* getData() { return this->data; }
		ConstantValue* getLength() { return this->length; }
		ConstantValue* getCapacity() { return this->capacity; }

		ConstantArray* getArray() { return this->arr; }

		protected:
		ConstantDynamicArray(DynamicArrayType* type);

		ConstantValue* data = 0;
		ConstantValue* length = 0;
		ConstantValue* capacity = 0;

		ConstantArray* arr = 0;
	};

	struct ConstantArraySlice : ConstantValue
	{
		friend struct Module;

		static ConstantArraySlice* get(ArraySliceType* type, ConstantValue* data, ConstantValue* length);

		ConstantValue* getData() { return this->data; }
		ConstantValue* getLength() { return this->length; }

		protected:
		ConstantArraySlice(ArraySliceType* type);

		ConstantValue* data = 0;
		ConstantValue* length = 0;
	};

	struct ConstantBitcast : ConstantValue
	{
		friend struct Module;

		static ConstantBitcast* get(ConstantValue* v, Type* target);

		ConstantValue* getValue() { return this->value; }

		protected:
		ConstantBitcast(ConstantValue* v, Type* target);

		ConstantValue* value = 0;
		Type* target = 0;
	};


	struct GlobalValue : ConstantValue
	{
		friend struct Module;

		LinkageType linkageType;

		Module* getParentModule() { return this->parentModule; }

		protected:
		GlobalValue(Module* mod, Type* type, LinkageType linkage, bool mut = false);

		Module* parentModule = 0;
	};

	struct GlobalVariable : GlobalValue
	{
		friend struct Module;

		GlobalVariable(const Identifier& idt, Module* module, Type* type, bool immutable, LinkageType linkage, ConstantValue* initValue);
		void setInitialValue(ConstantValue* constVal);
		ConstantValue* getInitialValue();

		protected:
		ConstantValue* initValue = 0;
	};
}




























