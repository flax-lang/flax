// constant.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

#include "value.h"

namespace fir
{
	bool checkSignedIntLiteralFitsIntoType(fir::PrimitiveType* type, ssize_t val);
	bool checkUnsignedIntLiteralFitsIntoType(fir::PrimitiveType* type, size_t val);
	bool checkFloatingPointLiteralFitsIntoType(fir::PrimitiveType* type, long double val);

	struct Value;

	// base class implicitly stores null
	struct ConstantValue : Value
	{
		friend struct Module;

		// static stuff
		static ConstantValue* getNullValue(Type* type);
		static ConstantValue* getNull();


		protected:
		ConstantValue(Type* type);
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
}




























