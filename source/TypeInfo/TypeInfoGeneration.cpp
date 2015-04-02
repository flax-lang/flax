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
	void addNewType(CodegenInstance* cgi, llvm::Type* stype, StructBase* str, ExprKind etype)
	{
		if(dynamic_cast<Enumeration*>(str) && cgi->isEnum(stype) && etype == ExprKind::Enum && str->name == "Type")
			return;


		for(auto k : cgi->rootNode->typeList)
		{
			if(std::get<0>(k) == stype->getStructName())
				return;
		}

		cgi->rootNode->typeList.push_back(std::make_tuple(stype->getStructName(), stype, etype));
	}

	size_t getIndexForType(Codegen::CodegenInstance* cgi, llvm::Type* type)
	{
		size_t i = 0;
		for(auto k : cgi->rootNode->typeList)
		{
			if(std::get<1>(k) == type)
			{
				return i;
			}

			i++;
		}

		return 0;
	}

	void generateTypeInfo(CodegenInstance* cgi)
	{
		Enumeration* enr = new Enumeration(Parser::PosInfo(), "Type");
		enr->isStrong = true;

		Number* num = new Number(Parser::PosInfo(), (int64_t) 1);


		bool done = false;
		for(auto t : cgi->rootNode->typeList)
		{
			if(std::get<0>(t) == "Int8")
			{
				done = true;
				break;
			}
		}

		if(!done)
		{
			cgi->rootNode->typeList.push_back(std::make_tuple("Int8", cgi->getLlvmTypeOfBuiltin("Int8"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Int16", cgi->getLlvmTypeOfBuiltin("Int16"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Int32", cgi->getLlvmTypeOfBuiltin("Int32"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Int64", cgi->getLlvmTypeOfBuiltin("Int64"), ExprKind::BuiltinType));

			cgi->rootNode->typeList.push_back(std::make_tuple("Uint8", cgi->getLlvmTypeOfBuiltin("Uint8"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Uint16", cgi->getLlvmTypeOfBuiltin("Uint16"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Uint32", cgi->getLlvmTypeOfBuiltin("Uint32"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Uint64", cgi->getLlvmTypeOfBuiltin("Uint64"), ExprKind::BuiltinType));

			cgi->rootNode->typeList.push_back(std::make_tuple("Float32", cgi->getLlvmTypeOfBuiltin("Float32"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Float64", cgi->getLlvmTypeOfBuiltin("Float64"), ExprKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Bool", cgi->getLlvmTypeOfBuiltin("Bool"), ExprKind::BuiltinType));
		}


		for(auto tup : cgi->rootNode->typeList)
		{
			enr->cases.push_back(std::make_pair(std::get<0>(tup), num));
			num = new Number(Parser::PosInfo(), num->ival + 1);
		}

		// note: Enumeration does nothing in codegen()
		// fix this if that happens to change in the future.
		enr->createType(cgi);
	}
}







