// FlaxType.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <unordered_map>

#include "../include/typechecking.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"

namespace flax
{
	// NOTE: some global state.
	// structs

	class FTContext
	{
		public:
		std::map<std::deque<flax::Type*>, flax::Type*> createdLiteralStructs;
		std::map<std::string, flax::Type*> createdNamedStructs;

		// arrays
		std::map<std::pair<flax::Type*, size_t>, flax::Type*> createdArrays;

		// functions
		// key type: pair<pair<deque<args>, isVarArg>, returnType>
		std::map<std::pair<std::pair<std::deque<flax::Type*>, bool>, flax::Type*>, flax::Type*> createdFunctions;

		// primitives
		// NOTE: multimap is ordered by bit width.
		// floats + ints here too.
		std::unordered_map<size_t, std::vector<flax::Type*>> primitiveTypes;

		// special little thing.
		flax::Type* voidType = 0;

		llvm::LLVMContext* llvmContext = 0;





		struct TypeList
		{
			std::vector<flax::Type*> primitives;
			std::vector<flax::Type*> structs;
			std::vector<flax::Type*> arrays;
			std::vector<flax::Type*> funcs;
		};


		std::unordered_map<size_t, TypeList> typeCache;
		void createOrGetType(flax::Type** type)
		{
			flax::Type*& ty = *type;

			TypeList& list = typeCache[ty->indirections];

			// find in the list.
			if(ty->typeKind == FTypeKind::Integer || ty->typeKind == FTypeKind::Floating)
			{
				for(auto t : list.primitives)
				{
					if(Type::areTypesEqual(t, ty))
					{
						// todo: I really *don't know* how else to do this.
						delete ty;

						*type = t;
						return;
					}
				}

				// not existing.
				list.primitives.push_back(ty);
			}
			else if(ty->typeKind == FTypeKind::NamedStruct || ty->typeKind == FTypeKind::LiteralStruct)
			{
				for(auto t : list.structs)
				{
					if(Type::areTypesEqual(t, ty))
					{
						// todo: I really *don't know* how else to do this.
						delete ty;

						*type = t;
						return;
					}
				}

				// not existing.
				list.structs.push_back(ty);
			}
			else if(ty->typeKind == FTypeKind::Array)
			{
				for(auto t : list.arrays)
				{
					if(Type::areTypesEqual(t, ty))
					{
						// todo: I really *don't know* how else to do this.
						delete ty;

						*type = t;
						return;
					}
				}

				// not existing.
				list.arrays.push_back(ty);
			}
			else if(ty->typeKind == FTypeKind::Function)
			{
				for(auto t : list.funcs)
				{
					if(Type::areTypesEqual(t, ty))
					{
						// todo: I really *don't know* how else to do this.
						delete ty;

						*type = t;
						return;
					}
				}

				// not existing.
				list.funcs.push_back(ty);
			}
			else if(ty->typeKind == FTypeKind::Void)
			{
				if(this->voidType == 0)
				{
					this->voidType = ty;
				}
				else
				{
					delete ty;
					*type = this->voidType;
				}
			}
			else
			{
				iceAssert(0 && "invalid type kind");
			}
		}
	};




	static FTContext* defaultFTContext = 0;
	void setDefaultFTContext(FTContext* tc)
	{
		iceAssert(tc && "Tried to set null type context as default");
		defaultFTContext = tc;
	}

	FTContext* getDefaultFTContext()
	{
		if(defaultFTContext == 0)
			defaultFTContext = createFTContext();

		return defaultFTContext;
	}

	FTContext* createFTContext()
	{
		FTContext* tc = new FTContext();

		tc->llvmContext = &llvm::getGlobalContext();

		// fill in primitives

		// void.
		tc->voidType = new Type();
		tc->voidType->bitWidth = 0;
		tc->voidType->typeKind = FTypeKind::Void;
		tc->voidType->llvmType = llvm::Type::getVoidTy(*tc->llvmContext);


		// bool
		{
			flax::Type* t = new Type();
			t->bitWidth = 1;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt1Ty(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[1].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}




		// int8
		{
			flax::Type* t = new Type();
			t->bitWidth = 8;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt8Ty(*tc->llvmContext);
			t->isTypeSigned = true;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// int16
		{
			flax::Type* t = new Type();
			t->bitWidth = 16;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt16Ty(*tc->llvmContext);
			t->isTypeSigned = true;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// int32
		{
			flax::Type* t = new Type();
			t->bitWidth = 32;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt32Ty(*tc->llvmContext);
			t->isTypeSigned = true;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// int64
		{
			flax::Type* t = new Type();
			t->bitWidth = 64;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt64Ty(*tc->llvmContext);
			t->isTypeSigned = true;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}




		// uint8
		{
			flax::Type* t = new Type();
			t->bitWidth = 8;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt8Ty(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// uint16
		{
			flax::Type* t = new Type();
			t->bitWidth = 16;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt16Ty(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// uint32
		{
			flax::Type* t = new Type();
			t->bitWidth = 32;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt32Ty(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// uint64
		{
			flax::Type* t = new Type();
			t->bitWidth = 64;
			t->typeKind = FTypeKind::Integer;
			t->llvmType = llvm::Type::getInt64Ty(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}


		// float32
		{
			flax::Type* t = new Type();
			t->bitWidth = 32;
			t->typeKind = FTypeKind::Floating;
			t->llvmType = llvm::Type::getFloatTy(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}
		// float64
		{
			flax::Type* t = new Type();
			t->bitWidth = 64;
			t->typeKind = FTypeKind::Floating;
			t->llvmType = llvm::Type::getDoubleTy(*tc->llvmContext);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache[0].primitives.push_back(t);
		}

		return tc;
	}






	static std::string typeListToString(std::deque<flax::Type*> types)
	{
		// print types
		std::string str = "{ ";
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + "}";
	}

	static bool areTypeListsEqual(std::deque<flax::Type*> a, std::deque<flax::Type*> b)
	{
		if(a.size() != b.size()) return false;

		for(size_t i = 0; i < a.size(); i++)
		{
			if(!Type::areTypesEqual(a[i], b[i]))
				return false;
		}

		return true;
	}

	// structs
	flax::Type* Type::getOrCreateNamedStruct(std::string name, std::deque<flax::Type*> members, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		flax::Type* existing = tc->createdNamedStructs[name];
		if(!existing)
		{
			existing = new flax::Type();

			existing->typeKind = FTypeKind::NamedStruct;

			existing->structName = name;
			existing->structMembers = members;
			existing->isTypeLiteralStruct = false;

			std::vector<llvm::Type*> lmems;
			for(auto m : members)
				lmems.push_back(m->llvmType);

			existing->llvmType = llvm::StructType::create(*tc->llvmContext, lmems, name);
			tc->createdNamedStructs[name] = existing;
		}
		else
		{
			iceAssert(existing->typeKind == FTypeKind::NamedStruct && "wtf??");

			// check members.
			if(!areTypeListsEqual(members, existing->structMembers))
			{
				std::string mstr = typeListToString(members);
				error("Conflicting types for named struct %s:\n%s vs %s", name.c_str(), existing->str().c_str(), mstr.c_str());
			}

			// ok.
		}

		return existing;
	}

	flax::Type* Type::getOrCreateNamedStruct(std::string name, std::vector<flax::Type*> members, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::deque<flax::Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return Type::getOrCreateNamedStruct(name, dmems, tc);
	}




	flax::Type* Type::getLiteralStruct(std::deque<flax::Type*> members, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		for(auto litstr : tc->createdLiteralStructs)
		{
			if(areTypeListsEqual(litstr.first, members))
				return litstr.second;
		}

		// nope. create.
		flax::Type* type = new flax::Type();

		type->typeKind = FTypeKind::LiteralStruct;

		type->structName = "__LITERAL_STRUCT__";
		type->structMembers = members;
		type->isTypeLiteralStruct = true;

		std::vector<llvm::Type*> lmems;
		for(auto m : members)
			lmems.push_back(m->llvmType);

		type->llvmType = llvm::StructType::get(*tc->llvmContext, lmems);

		tc->createdLiteralStructs[type->structMembers] = type;
		return type;
	}

	flax::Type* Type::getLiteralStruct(std::vector<flax::Type*> members, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::deque<flax::Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return Type::getLiteralStruct(dmems, tc);
	}



	// arrays
	flax::Type* Type::getArray(flax::Type* elementType, size_t num, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		for(auto arr : tc->createdArrays)
		{
			if(Type::areTypesEqual(arr.first.first, elementType) && arr.first.second == num)
				return arr.second;
		}

		// create.
		flax::Type* type = new Type();

		type->typeKind = FTypeKind::Array;

		type->arraySize = num;
		type->arrayElementType = elementType;
		type->llvmType = llvm::ArrayType::get(type->arrayElementType->getLlvmType(), type->getArraySize());

		tc->createdArrays[{ elementType, num }] = type;

		return type;
	}



	// functions
	flax::Type* Type::getFunction(std::deque<flax::Type*> args, flax::Type* ret, bool isVarArg, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// key type: pair<pair<deque<args>, isVarArg>, returnType>
		for(auto fn : tc->createdFunctions)
		{
			if(areTypeListsEqual(fn.first.first.first, args) && Type::areTypesEqual(fn.first.second, ret)
				&& (isVarArg == fn.first.first.second))
			{
				return fn.second;
			}
		}

		// create.
		flax::Type* type = new Type();
		type->typeKind = FTypeKind::Function;

		type->isFnVarArg = isVarArg;
		type->functionParams = args;
		type->functionRetType = ret;

		std::vector<llvm::Type*> largs;
		for(auto a : args)
			largs.push_back(a->llvmType);

		type->llvmType = llvm::FunctionType::get(ret->llvmType, largs, isVarArg);

		tc->createdFunctions[{ { args, isVarArg }, ret }] = type;

		return type;
	}


	// primitives
	flax::Type* Type::getBool(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// bitwidth = 1
		std::vector<flax::Type*> bools = tc->primitiveTypes[1];

		// how do we have more than 1?
		iceAssert(bools.size() == 1 && "???? more than 1 bool??");
		iceAssert(bools.front()->bitWidth == 1 && "not bool purporting to be bool???");

		return bools.front();
	}

	flax::Type* Type::getVoid(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		iceAssert(tc->voidType && "FTContext was not initialised, no void type!");
		return tc->voidType;
	}









	flax::Type* Type::getIntWithBitWidthAndSignage(FTContext* tc, size_t bits, bool issigned)
	{
		std::vector<flax::Type*> types = tc->primitiveTypes[bits];

		iceAssert(types.size() > 0 && "no types of this kind??");

		for(auto t : types)
		{
			iceAssert(t->bitWidth == bits);
			if((t->isSigned() == issigned) && !t->isFloatingPointType())
				return t;
		}

		iceAssert(false);
		return 0;
	}

	flax::Type* Type::getFloatWithBitWidth(FTContext* tc, size_t bits)
	{
		std::vector<flax::Type*> types = tc->primitiveTypes[bits];

		iceAssert(types.size() > 0 && "no types of this kind??");

		for(auto t : types)
		{
			iceAssert(t->bitWidth == bits);
			if(t->isFloatingPointType())
				return t;
		}

		iceAssert(false);
		return 0;
	}





	flax::Type* Type::getInt8(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 8, true);
	}

	flax::Type* Type::getInt16(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 16, true);
	}

	flax::Type* Type::getInt32(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 32, true);
	}

	flax::Type* Type::getInt64(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 64, true);
	}





	flax::Type* Type::getUint8(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 8, false);
	}

	flax::Type* Type::getUint16(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 16, false);
	}

	flax::Type* Type::getUint32(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 32, false);
	}

	flax::Type* Type::getUint64(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getIntWithBitWidthAndSignage(tc, 64, false);
	}




	flax::Type* Type::getFloat32(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getFloatWithBitWidth(tc, 32);
	}

	flax::Type* Type::getFloat64(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return Type::getFloatWithBitWidth(tc, 64);
	}







	// various
	std::string Type::str()
	{
		// is primitive.
		std::string ret;
		if(this->isIntegerType())
		{
			if(this->isSigned())	ret = "i";
			else					ret = "u";

			ret += std::to_string(this->getIntegerBitWidth());
		}
		else if(this->isFloatingPointType())
		{
			// todo: bitWidth is applicable to both floats and ints,
			// but getIntegerBitWidth (obviously) works only for ints.
			if(this->bitWidth == 32)
				ret = "f32";

			else if(this->bitWidth == 64)
				ret = "f64";

			else
				iceAssert(!"????");
		}
		else if(this->isArrayType())
		{
			ret = this->arrayElementType->str();
			ret += "[" + std::to_string(this->getArraySize()) + "]";
		}
		else if(this->isNamedStruct())
		{
			ret = this->structName;
		}
		else if(this->isLiteralStruct())
		{
			ret = typeListToString(this->structMembers);
		}
		else
		{
			iceAssert(!"???? no such type");
		}


		for(size_t ind = 0; ind < this->indirections; ind++)
			ret += "*";

		return ret;
	}



















	flax::Type* Type::cloneType(flax::Type* type)
	{
		flax::Type* newType		= new Type();
		newType->typeKind		= type->typeKind;
		newType->indirections	= type->indirections;

		if(type->typeKind == FTypeKind::Integer || type->typeKind == FTypeKind::Floating)
		{
			newType->bitWidth				= type->bitWidth;
			newType->isTypeSigned			= type->isTypeSigned;
		}
		else if(type->typeKind == FTypeKind::Array)
		{
			newType->arrayElementType		= type->arrayElementType;
			newType->arraySize				= type->arraySize;
		}
		else if(type->typeKind == FTypeKind::NamedStruct || type->typeKind == FTypeKind::LiteralStruct)
		{
			newType->structMembers			= type->structMembers;
			newType->isTypeLiteralStruct	= type->isTypeLiteralStruct;

			if(type->typeKind == FTypeKind::NamedStruct)
				newType->structName = type->structName;
		}
		else if(type->typeKind == FTypeKind::Function)
		{
			newType->functionParams			= type->functionParams;
			newType->functionRetType		= type->functionRetType;
			newType->isFnVarArg				= type->isFnVarArg;
		}
		else if(type->typeKind == FTypeKind::Void)
		{
			iceAssert(!"pointers to void not supported");
		}
		else
		{
			iceAssert(!"invalid type");
		}

		return newType;
	}


	flax::Type* Type::getPointerTo(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		flax::Type* newType = Type::cloneType(this);
		newType->indirections += 1;

		// get or create.
		tc->createOrGetType(&newType);

		// todo:
		return newType;
	}

	flax::Type* Type::getPointerElementType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		flax::Type* newType = Type::cloneType(this);
		if(newType->indirections == 0)
			iceAssert(!"type is not a pointer");

		newType->indirections -= 1;

		tc->createOrGetType(&newType);

		// todo:
		return newType;
	}


	bool Type::isTypeEqual(flax::Type* other)
	{
		return Type::areTypesEqual(this, other);
	}

	bool Type::areTypesEqual(flax::Type* a, flax::Type* b)
	{
		if(a->typeKind != b->typeKind) return false;
		if(a->indirections != b->indirections) return false;

		if(a->typeKind == FTypeKind::Integer)
		{
			return a->getIntegerBitWidth() == b->getIntegerBitWidth() && a->isSigned() == b->isSigned();
		}
		else if(a->typeKind == FTypeKind::Floating)
		{
			return a->bitWidth == b->bitWidth;
		}
		else if(a->typeKind == FTypeKind::Array)
		{
			return a->arraySize == b->arraySize && Type::areTypesEqual(a->getArrayElementType(), b->getArrayElementType());
		}
		else if(a->typeKind == FTypeKind::NamedStruct)
		{
			return a->getStructName() == b->getStructName();
		}
		else if(a->typeKind == FTypeKind::LiteralStruct)
		{
			return areTypeListsEqual(a->structMembers, b->structMembers);
		}
		else if(a->typeKind == FTypeKind::Function)
		{
			return areTypeListsEqual(a->functionParams, b->functionParams)
				&& Type::areTypesEqual(a->functionRetType, b->functionRetType) && a->isFnVarArg == b->isFnVarArg;
		}
		else
		{
			iceAssert(!"invalid type");
			return false;
		}
	}


	llvm::Type* Type::getLlvmType()
	{
		return this->llvmType;
	}










	bool Type::isPointerTo(flax::Type* other)
	{
		return false;
	}

	bool Type::isArrayElementOf(flax::Type* other)
	{
		return false;
	}

	bool Type::isPointerElementOf(flax::Type* other)
	{
		return false;
	}


	bool Type::isStructType()
	{
		return this->indirections == 0
			&& (this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct);
	}

	bool Type::isNamedStruct()
	{
		return this->indirections == 0
			&& this->typeKind == FTypeKind::NamedStruct;
	}

	bool Type::isLiteralStruct()
	{
		return this->indirections == 0
			&& this->typeKind == FTypeKind::LiteralStruct;
	}

	bool Type::isArrayType()
	{
		return this->indirections == 0
			&& this->typeKind == FTypeKind::Array;
	}

	bool Type::isFloatingPointType()
	{
		return this->indirections == 0
			&& this->typeKind == FTypeKind::Floating;
	}

	bool Type::isIntegerType()
	{
		return this->indirections == 0
			&& this->typeKind == FTypeKind::Integer;
	}
}


















