// Type.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <unordered_map>

#include "../include/codegen.h"
#include "../include/compiler.h"
#include "../include/ir/type.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"

namespace Codegen
{
	std::string unwrapPointerType(std::string, int*);
}

namespace flax
{
	// NOTE: some global state.
	// structs

	Type* FTContext::normaliseType(Type* type)
	{
		size_t ind = 0;
		if(type->isPointerType())
			ind = type->toPointerType()->getIndirections();


		if(Type::areTypesEqual(type, this->voidType))
			return this->voidType;


		std::vector<Type*>& list = typeCache[ind];

		// find in the list.
		// todo: make this more efficient
		for(auto t : list)
		{
			if(Type::areTypesEqual(t, type))
				return t;
		}


		list.push_back(type);
		return type;
	}


	static FTContext* defaultFTContext = 0;
	void setDefaultFTContext(FTContext* tc)
	{
		iceAssert(tc && "Tried to set null type context as default");
		defaultFTContext = tc;
	}

	FTContext* getDefaultFTContext()
	{
		if(defaultFTContext == 0)
		{
			// iceAssert(0);
			defaultFTContext = createFTContext();
		}

		return defaultFTContext;
	}

	FTContext* createFTContext()
	{
		FTContext* tc = new FTContext();

		tc->llvmContext = &llvm::getGlobalContext();

		// fill in primitives

		// void.
		tc->voidType = new PrimitiveType(0, FTypeKind::Void);


		// bool
		{
			PrimitiveType* t = new PrimitiveType(1, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[1].push_back(t);
			tc->typeCache[0].push_back(t);
		}




		// int8
		{
			PrimitiveType* t = new PrimitiveType(8, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// int16
		{
			PrimitiveType* t = new PrimitiveType(16, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// int32
		{
			PrimitiveType* t = new PrimitiveType(32, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// int64
		{
			PrimitiveType* t = new PrimitiveType(64, FTypeKind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].push_back(t);
		}




		// uint8
		{
			PrimitiveType* t = new PrimitiveType(8, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// uint16
		{
			PrimitiveType* t = new PrimitiveType(16, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// uint32
		{
			PrimitiveType* t = new PrimitiveType(32, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// uint64
		{
			PrimitiveType* t = new PrimitiveType(64, FTypeKind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].push_back(t);
		}


		// float32
		{
			PrimitiveType* t = new PrimitiveType(32, FTypeKind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].push_back(t);
		}
		// float64
		{
			PrimitiveType* t = new PrimitiveType(64, FTypeKind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].push_back(t);
		}

		return tc;
	}






	std::string Type::typeListToString(std::deque<Type*> types)
	{
		// print types
		std::string str = "{ ";
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + " }";
	}

	bool Type::areTypeListsEqual(std::deque<Type*> a, std::deque<Type*> b)
	{
		if(a.size() != b.size()) return false;

		for(size_t i = 0; i < a.size(); i++)
		{
			if(!Type::areTypesEqual(a[i], b[i]))
				return false;
		}

		return true;
	}


	Type* Type::getPointerTo(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		size_t inds = 0;
		if(this->toPointerType())
			inds = this->toPointerType()->indirections;

		PointerType* newType = new PointerType(inds + 1, this);

		// get or create.
		newType = dynamic_cast<PointerType*>(tc->normaliseType(newType));
		iceAssert(newType);
		return newType;
	}

	Type* Type::getPointerElementType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(!this->isPointerType())
			iceAssert(!"type is not a pointer");


		PointerType* ptrthis = dynamic_cast<PointerType*>(this);
		iceAssert(ptrthis);

		Type* newType = ptrthis->baseType;
		newType = tc->normaliseType(newType);

		// iceAssert(newType->indirections == this->indirections - 1);
		// printf("POINTER ELM: %p\n", newType);
		return newType;
	}

	bool Type::areTypesEqual(Type* a, Type* b)
	{
		if(a->typeKind != b->typeKind) return false;

		return a->isTypeEqual(b);
	}



	Type* Type::getIndirectedType(ssize_t times, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		Type* ret = this;
		if(times > 0)
		{
			for(ssize_t i = 0; i < times; i++)
			{
				// auto old = ret;
				ret = ret->getPointerTo();

				// printf("ret: %s // %s\n", old->str().c_str(), ret->str().c_str());
			}
		}
		else if(times < 0)
		{
			for(ssize_t i = 0; i < -times; i++)
				ret = ret->getPointerElementType();
		}

		return ret;
	}


	Type* Type::fromBuiltin(std::string builtin, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		int indirections = 0;
		builtin = Codegen::unwrapPointerType(builtin, &indirections);

		if(!Compiler::getDisableLowercaseBuiltinTypes())
		{
			if(builtin.length() > 0)
			{
				builtin[0] = toupper(builtin[0]);
			}
		}

		Type* real = 0;

		if(builtin == "Int8")			real = PrimitiveType::getInt8(tc);
		else if(builtin == "Int16")		real = PrimitiveType::getInt16(tc);
		else if(builtin == "Int32")		real = PrimitiveType::getInt32(tc);
		else if(builtin == "Int64")		real = PrimitiveType::getInt64(tc);
		else if(builtin == "Int")		real = PrimitiveType::getInt64(tc);

		else if(builtin == "Uint8")		real = PrimitiveType::getUint8(tc);
		else if(builtin == "Uint16")	real = PrimitiveType::getUint16(tc);
		else if(builtin == "Uint32")	real = PrimitiveType::getUint32(tc);
		else if(builtin == "Uint64")	real = PrimitiveType::getUint64(tc);
		else if(builtin == "Uint")		real = PrimitiveType::getUint64(tc);

		else if(builtin == "Float32")	real = PrimitiveType::getFloat32(tc);
		else if(builtin == "Float")		real = PrimitiveType::getFloat32(tc);

		else if(builtin == "Float64")	real = PrimitiveType::getFloat64(tc);
		else if(builtin == "Double")	real = PrimitiveType::getFloat64(tc);

		else if(builtin == "Bool")		real = PrimitiveType::getBool(tc);
		else if(builtin == "Void")		real = PrimitiveType::getVoid(tc);
		else return 0;

		iceAssert(real);

		real = real->getIndirectedType(indirections);
		return real;
	}


	Type* Type::fromLlvmType(llvm::Type* ltype, std::deque<bool> signage)
	{
		// Type* type = 0;

		// // printf("FROM LLVM: %s\n", ((Codegen::CodegenInstance*) 0)->getReadableType(ltype).c_str());

		// if(ltype->isPointerTy())
		// 	return fromLlvmType(ltype->getPointerElementType(), signage)->getPointerTo();


		// if(!type) type = new Type();

		// type->isTypeSigned = false;
		// type->isTypeVoid = false;
		// type->bitWidth = 0;


		// // 1a. int types
		// if(ltype->isIntegerTy())
		// {
		// 	type->typeKind = FTypeKind::Integer;
		// 	type->ptrBaseTypeKind = FTypeKind::Integer;

		// 	type->bitWidth = ltype->getIntegerBitWidth();
		// 	type->isTypeSigned = (signage.size() > 0 ? signage.front() : false);
		// }

		// // 1b. float types
		// else if(ltype->isFloatingPointTy())
		// {
		// 	type->typeKind = FTypeKind::Floating;
		// 	type->ptrBaseTypeKind = FTypeKind::Floating;

		// 	if(ltype->isFloatTy()) type->bitWidth = 32;
		// 	else if(ltype->isDoubleTy()) type->bitWidth = 64;
		// 	else iceAssert(0 && "??? unsupported floating type");
		// }

		// // 2a. named structs
		// else if(ltype->isStructTy() && !llvm::cast<llvm::StructType>(ltype)->isLiteral())
		// {
		// 	if(ltype->getStructName() == "String")
		// 		error("POOOP");

		// 	llvm::StructType* lstype = llvm::cast<llvm::StructType>(ltype);

		// 	type->typeKind = FTypeKind::NamedStruct;
		// 	type->ptrBaseTypeKind = FTypeKind::NamedStruct;

		// 	type->structName = lstype->getName();
		// 	type->isTypeLiteralStruct = false;

		// 	if(signage.size() != lstype->getNumElements())
		// 	{
		// 		printf("expected %d, got %zu\n", lstype->getNumElements(), signage.size());
		// 		iceAssert(!"missing information (not enough signs)");
		// 	}

		// 	size_t i = 0;
		// 	for(auto m : lstype->elements())
		// 		type->structMembers.push_back(Type::fromLlvmType(m, { signage[i] })), i++;
		// }

		// // 2b. literal structs
		// else if(ltype->isStructTy())
		// {
		// 	llvm::StructType* lstype = llvm::cast<llvm::StructType>(ltype);

		// 	type->typeKind = FTypeKind::LiteralStruct;
		// 	type->ptrBaseTypeKind = FTypeKind::LiteralStruct;

		// 	type->structName = "__LITERAL_STRUCT__";
		// 	type->isTypeLiteralStruct = true;

		// 	if(signage.size() != lstype->getNumElements())
		// 	{
		// 		printf("expected %d, got %zu\n", lstype->getNumElements(), signage.size());
		// 		iceAssert(!"missing information (not enough signs)");
		// 	}

		// 	size_t i = 0;
		// 	for(auto m : lstype->elements())
		// 		type->structMembers.push_back(Type::fromLlvmType(m, { signage[i] })), i++;
		// }

		// // 3. array types
		// else if(ltype->isArrayTy())
		// {
		// 	iceAssert(0);
		// }

		// // 4. function types
		// else if(ltype->isFunctionTy())
		// {
		// 	iceAssert(0);
		// }

		// if(type->indirections > 0)
		// 	type->typeKind = FTypeKind::Pointer;

		// type = getDefaultFTContext()->normaliseType(type);	// todo: proper.
		// // printf("RETURNING: %p\n", type);

		// return type;

		// return llvm::Type::getVoidTy(llvm::getGlobalContext());
		return PrimitiveType::getVoid();
	}




	PrimitiveType* Type::toPrimitiveType()
	{
		if(this->typeKind != FTypeKind::Integer && this->typeKind != FTypeKind::Floating) return 0;
		return dynamic_cast<PrimitiveType*>(this);
	}

	FunctionType* Type::toFunctionType()
	{
		if(this->typeKind != FTypeKind::Function) return 0;
		return dynamic_cast<FunctionType*>(this);
	}

	PointerType* Type::toPointerType()
	{
		if(this->typeKind != FTypeKind::Pointer) return 0;
		return dynamic_cast<PointerType*>(this);
	}

	StructType* Type::toStructType()
	{
		if(this->typeKind != FTypeKind::NamedStruct && this->typeKind != FTypeKind::LiteralStruct) return 0;
		return dynamic_cast<StructType*>(this);
	}

	ArrayType* Type::toArrayType()
	{
		if(this->typeKind != FTypeKind::Array) return 0;
		return dynamic_cast<ArrayType*>(this);
	}





















	bool Type::isStructType()
	{
		return this->toStructType() != 0;
	}

	bool Type::isNamedStruct()
	{
		return this->toStructType() != 0
			&& (this->typeKind == FTypeKind::NamedStruct);
	}

	bool Type::isLiteralStruct()
	{
		return this->toStructType() != 0
			&& (this->typeKind == FTypeKind::LiteralStruct);
	}

	bool Type::isPackedStruct()
	{
		return this->toStructType() != 0
			&& (this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct)
			&& (this->toStructType()->isTypePacked);
	}

	bool Type::isArrayType()
	{
		return this->toArrayType() != 0;
	}

	bool Type::isFloatingPointType()
	{
		return this->typeKind == FTypeKind::Floating
			&& this->toPrimitiveType() != 0;
	}

	bool Type::isIntegerType()
	{
		return this->typeKind == FTypeKind::Integer
			&& this->toPrimitiveType() != 0;
	}

	bool Type::isPointerType()
	{
		return this->toPointerType() != 0;
	}
}


















