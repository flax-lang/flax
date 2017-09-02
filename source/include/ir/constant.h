// constant.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "value.h"

namespace fir
{
	struct Value;
	struct ConstantValue;

	bool checkLiteralFitsIntoType(fir::PrimitiveType* type, mpfr::mpreal num);

	ConstantValue* createConstantValueCast(ConstantValue* cv, fir::Type* type);


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

		static ConstantNumber* get(mpfr::mpreal n);
		mpfr::mpreal getValue();

		protected:
		ConstantNumber(mpfr::mpreal n);
		mpfr::mpreal number;
	};

	struct ConstantInt : ConstantValue
	{
		friend struct Module;

		static ConstantInt* get(Type* intType, size_t val);

		static ConstantInt* getBool(bool value, FTContext* tc = 0);
		static ConstantInt* getInt8(int8_t value, FTContext* tc = 0);
		static ConstantInt* getInt16(int16_t value, FTContext* tc = 0);
		static ConstantInt* getInt32(int32_t value, FTContext* tc = 0);
		static ConstantInt* getInt64(int64_t value, FTContext* tc = 0);
		static ConstantInt* getUint8(uint8_t value, FTContext* tc = 0);
		static ConstantInt* getUint16(uint16_t value, FTContext* tc = 0);
		static ConstantInt* getUint32(uint32_t value, FTContext* tc = 0);
		static ConstantInt* getUint64(uint64_t value, FTContext* tc = 0);


		ssize_t getSignedValue();
		size_t getUnsignedValue();

		protected:
		ConstantInt(Type* type, ssize_t val);
		ConstantInt(Type* type, size_t val);

		size_t value;
	};

	struct ConstantChar : ConstantValue
	{
		friend struct Module;

		static ConstantChar* get(char c, FTContext* tc = 0) { return new ConstantChar(c); }

		char getValue() { return this->value; }

		protected:
		ConstantChar(char v) : ConstantValue(Type::getCharType()), value(v) { }
		char value;
	};


	struct ConstantFP : ConstantValue
	{
		friend struct Module;

		static ConstantFP* get(Type* intType, float val);
		static ConstantFP* get(Type* intType, double val);
		static ConstantFP* get(Type* intType, long double val);

		static ConstantFP* getFloat32(float value, FTContext* tc = 0);
		static ConstantFP* getFloat64(double value, FTContext* tc = 0);
		static ConstantFP* getFloat80(long double value, FTContext* tc = 0);

		long double getValue();

		protected:
		ConstantFP(Type* type, float val);
		ConstantFP(Type* type, double val);
		ConstantFP(Type* type, long double val);

		long double value;
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

	struct ConstantString : ConstantValue
	{
		friend struct Module;

		static ConstantString* get(std::string value);
		std::string getValue();

		protected:
		ConstantString(std::string str);

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



	struct GlobalValue : ConstantValue
	{
		friend struct Module;

		LinkageType linkageType;

		Module* getParentModule() { return this->parentModule; }

		protected:
		GlobalValue(Module* mod, Type* type, LinkageType linkage);

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




























