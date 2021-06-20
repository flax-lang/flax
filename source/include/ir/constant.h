// constant.h
// Copyright (c) 2014 - 2016, zhiayang
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

	// base class implicitly stores null
	struct ConstantValue : Value
	{
		friend struct Module;
		friend struct IRBuilder;

		// static stuff
		static ConstantValue* getZeroValue(Type* type);
		static ConstantValue* getNull();

		virtual std::string str();

	protected:
		ConstantValue(Type* type);
	};

	struct ConstantBool : ConstantValue
	{
		friend struct Module;

		static ConstantBool* get(bool value);
		bool getValue();

		virtual std::string str() override;

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

		virtual std::string str() override;

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

		virtual std::string str() override;

	protected:
		ConstantFP(Type* type, float val);
		ConstantFP(Type* type, double val);

		double value;
	};

	struct ConstantArray : ConstantValue
	{
		friend struct Module;

		static ConstantArray* get(Type* type, const std::vector<ConstantValue*>& vals);

		std::vector<ConstantValue*> getValues() { return this->values; }

		virtual std::string str() override;

	protected:
		ConstantArray(Type* type, const std::vector<ConstantValue*>& vals);

		std::vector<ConstantValue*> values;
	};

	struct ConstantStruct : ConstantValue
	{
		friend struct Module;

		static ConstantStruct* get(StructType* st, const std::vector<ConstantValue*>& members);

		virtual std::string str() override;

	protected:
		ConstantStruct(StructType* st, const std::vector<ConstantValue*>& members);
		std::vector<ConstantValue*> members;
	};

	struct ConstantEnumCase : ConstantValue
	{
		friend struct Module;

		static ConstantEnumCase* get(EnumType* en, ConstantInt* index, ConstantValue* value);

		ConstantInt* getIndex();
		ConstantValue* getValue();

		virtual std::string str() override;

	protected:
		ConstantEnumCase(EnumType* en, ConstantInt* index, ConstantValue* value);

		ConstantInt* index = 0;
		ConstantValue* value = 0;
	};

	struct ConstantCharSlice : ConstantValue
	{
		friend struct Module;

		static ConstantCharSlice* get(const std::string& value);
		std::string getValue();

		virtual std::string str() override;

	protected:
		ConstantCharSlice(const std::string& str);

		std::string value;
	};

	struct ConstantTuple : ConstantValue
	{
		friend struct Module;

		static ConstantTuple* get(const std::vector<ConstantValue*>& mems);
		std::vector<ConstantValue*> getValues();

		virtual std::string str() override;

	protected:
		ConstantTuple(const std::vector<ConstantValue*>& mems);
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

		virtual std::string str() override;

	protected:
		ConstantDynamicArray(DynamicArrayType* type);

		ConstantValue* data = 0;
		ConstantValue* length = 0;
		ConstantValue* capacity = 0;

		ConstantArray* arr = 0;
	};

	// this is the 'string' type!!
	struct ConstantDynamicString : ConstantValue
	{
		friend struct Module;

		static ConstantDynamicString* get(const std::string& s);
		std::string getValue();

		virtual std::string str() override;

	protected:
		ConstantDynamicString(const std::string& s);

		std::string value;
	};


	struct ConstantArraySlice : ConstantValue
	{
		friend struct Module;

		static ConstantArraySlice* get(ArraySliceType* type, ConstantValue* data, ConstantValue* length);

		ConstantValue* getData() { return this->data; }
		ConstantValue* getLength() { return this->length; }

		virtual std::string str() override;

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

		virtual std::string str() override;

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

		virtual std::string str() override;

	protected:
		GlobalValue(Module* mod, Type* type, LinkageType linkage, bool mut = false);

		Module* parentModule = 0;
	};

	struct GlobalVariable : GlobalValue
	{
		friend struct Module;

		GlobalVariable(const Name& idt, Module* module, Type* type, bool immutable, LinkageType linkage, ConstantValue* initValue);
		void setInitialValue(ConstantValue* constVal);
		ConstantValue* getInitialValue();

		virtual std::string str() override;

	protected:
		ConstantValue* initValue = 0;
	};
}




























