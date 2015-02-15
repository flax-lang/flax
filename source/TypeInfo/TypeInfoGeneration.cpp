// TypeInfoGeneration.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

namespace TypeInfo
{
	void addNewPrimitiveType(CodegenInstance* cgi, llvm::Type* type)
	{
		// unsupported atm
	}

	static llvm::Value* createFlaxStringName(CodegenInstance* cgi, std::string name)
	{
		llvm::Type* i32 = llvm::Type::getInt32Ty(cgi->getContext());
		llvm::StructType* flaxStringType = llvm::cast<llvm::StructType>(cgi->stringType);

		llvm::Constant* nameLength = llvm::ConstantInt::get(i32, name.length());
		llvm::Value* nameString = cgi->mainBuilder.CreateGlobalStringPtr(name.c_str());
		return llvm::ConstantStruct::get(flaxStringType, nameLength, nameString, NULL);
	}

	void addNewStructType(CodegenInstance* cgi, llvm::Type* stype, StructBase* str)
	{
		std::vector<StructMemberType> members;
		assert(stype->isStructTy());

		int memberNum = 0;
		for(VarDecl* vd : str->members)
		{
			StructMemberType smt;
			llvm::Type* ltype = cgi->getLlvmType(vd);

			if(ltype->isIntegerTy())			smt.kind = TypeKind::Integer;
			else if(ltype->isFloatingPointTy())	smt.kind = TypeKind::FloatingPt;
			else if(ltype->isStructTy())		smt.kind = TypeKind::Struct;
			else if(ltype->isFunctionTy())		smt.kind = TypeKind::Function;

			smt.name = vd->name;

			const llvm::StructLayout* layout = cgi->mainModule->getDataLayout()->getStructLayout(llvm::cast<llvm::StructType>(stype));
			smt.offset = layout->getElementOffset(memberNum);
			members.push_back(smt);

			memberNum++;
		}

		StructType* st = new StructType();
		st->kind = TypeKind::Struct;
		st->name = str->mangledName;
		st->members = members;


		cgi->rootNode->typeInformationTable.push_back(st);
	}

	// this adds the type info for all the basic types
	void initialiseTypeInfo(CodegenInstance* cgi)
	{
		auto* i8 = new IntegerType();	i8->name = "Int8";		i8->kind = TypeKind::Integer;	i8->bits = 8;	i8->isSigned = true;
		auto* i16 = new IntegerType();	i16->name = "Int16";	i16->kind = TypeKind::Integer;	i16->bits = 16;	i16->isSigned = true;
		auto* i32 = new IntegerType();	i32->name = "Int32";	i32->kind = TypeKind::Integer;	i32->bits = 32;	i32->isSigned = true;
		auto* i64 = new IntegerType();	i64->name = "Int64";	i64->kind = TypeKind::Integer;	i64->bits = 64;	i64->isSigned = true;

		auto* u8 = new IntegerType();	u8->name = "Uint8";		u8->kind = TypeKind::Integer;	u8->bits = 8;	u8->isSigned = true;
		auto* u16 = new IntegerType();	u16->name = "Uint16";	u16->kind = TypeKind::Integer;	u16->bits = 16;	u16->isSigned = true;
		auto* u32 = new IntegerType();	u32->name = "Uint32";	u32->kind = TypeKind::Integer;	u32->bits = 32;	u32->isSigned = true;
		auto* u64 = new IntegerType();	u64->name = "Uint64";	u64->kind = TypeKind::Integer;	u64->bits = 64;	u64->isSigned = true;

		auto* f32 = new FloatingPointType();	f32->name = "Float32";	f32->kind = TypeKind::Integer;	f32->isSinglePrecision = true;
		auto* f64 = new FloatingPointType();	f64->name = "Float64";	f64->kind = TypeKind::Integer;	f64->isSinglePrecision = false;

		cgi->rootNode->typeInformationTable.push_back(i8);
		cgi->rootNode->typeInformationTable.push_back(i16);
		cgi->rootNode->typeInformationTable.push_back(i32);
		cgi->rootNode->typeInformationTable.push_back(i64);

		cgi->rootNode->typeInformationTable.push_back(u8);
		cgi->rootNode->typeInformationTable.push_back(u16);
		cgi->rootNode->typeInformationTable.push_back(u32);
		cgi->rootNode->typeInformationTable.push_back(u64);

		cgi->rootNode->typeInformationTable.push_back(f32);
		cgi->rootNode->typeInformationTable.push_back(f64);
	}

	void generateTypeInfo(CodegenInstance* cgi)
	{
		llvm::StructType* flaxStringType = llvm::cast<llvm::StructType>(cgi->stringType);
		llvm::Type* i32 = llvm::Type::getInt32Ty(cgi->getContext());
		llvm::Type* i64 = llvm::Type::getInt64Ty(cgi->getContext());

		static llvm::StructType* memType = llvm::StructType::create("__TypeInfo#structMemberType", flaxStringType, i32, i64, NULL);
		static llvm::StructType* strType = llvm::StructType::create("__TypeInfo#structType", flaxStringType, i32, i64, memType->getPointerTo(), NULL);


		llvm::Value* structKind = llvm::ConstantInt::get(i32, (int) TypeKind::Struct);


		for(Type* t : cgi->rootNode->typeInformationTable)
		{
			StructType* st = dynamic_cast<StructType*>(t);
			if(st)
			{
				// layout:
				// name: FlaxString			(4 + sizeof(void*) bytes)
				// kind: int				(4 bytes)
				// memberCount: uint64_t	(8 bytes)
				// members: Member*			(sizeof(void*) bytes)

				// member:
				// name: FlaxString
				// kind: int
				// offset: uint64_t

				llvm::Value* structName = createFlaxStringName(cgi, st->name);
				llvm::Value* memberCount = llvm::ConstantInt::get(i64, st->members.size());

				std::vector<llvm::Constant*> mems;
				for(StructMemberType smt : st->members)
				{
					llvm::Value* memberName = createFlaxStringName(cgi, smt.name);
					llvm::Value* memberKind = llvm::ConstantInt::get(i32, (int) smt.kind);
					llvm::Value* memberOffs = llvm::ConstantInt::get(i64, smt.offset);

					mems.push_back(llvm::ConstantStruct::get(memType, memberName, memberKind, memberOffs, NULL));
				}

				llvm::Constant* membersArr = llvm::ConstantArray::get(llvm::ArrayType::get(memType, st->members.size()), mems);
				llvm::GlobalVariable* gvMemArr = new llvm::GlobalVariable(*cgi->mainModule, membersArr->getType(), true, llvm::GlobalValue::ExternalLinkage, membersArr);

				llvm::Value* membersPtr = cgi->mainBuilder.CreateConstGEP2_32(gvMemArr, 0, 0);

				llvm::Constant* str = llvm::ConstantStruct::get(strType, structName, structKind, memberCount, membersPtr, NULL);
				new llvm::GlobalVariable(*cgi->mainModule, str->getType(), true, llvm::GlobalValue::ExternalLinkage, str, "__TypeInfo#" + st->name);
			}
		}
	}
}







