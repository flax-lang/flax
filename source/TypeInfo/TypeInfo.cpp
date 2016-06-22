// TypeInfoGeneration.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "typeinfo.h"

using namespace Ast;
using namespace Codegen;

namespace TypeInfo
{
	void addNewType(CodegenInstance* cgi, fir::Type* stype, StructBase* str, TypeKind etype)
	{
		for(auto k : cgi->rootNode->typeList)
		{
			if(stype->isStructType())
			{
				fir::StructType* strt = dynamic_cast<fir::StructType*>(stype);
				iceAssert(strt);

				if(strt->isNamedStruct() && std::get<0>(k) == strt->toStructType()->getStructName())
					return;
			}
		}

		cgi->rootNode->typeList.push_back(std::make_tuple(stype->isLiteralStruct() ? "" : stype->toStructType()->getStructName(),
			stype, etype));
	}

	size_t getIndexForType(Codegen::CodegenInstance* cgi, fir::Type* type)
	{
		size_t i = 1;
		for(auto k : cgi->rootNode->typeList)
		{
			if(std::get<1>(k) == type)
			{
				return i;
			}

			i++;
		}

		std::string name = type->str();
		cgi->rootNode->typeList.push_back(std::make_tuple(name, type, TypeKind::BuiltinType));

		return getIndexForType(cgi, type);
	}

	void initialiseTypeInfo(CodegenInstance* cgi)
	{
		EnumDef* enr = 0;

		if(cgi->getType("Type") == nullptr)
		{
			enr = new EnumDef(Parser::Pin(), "Type");
			enr->isStrong = true;

			Number* num = new Number(Parser::Pin(), (int64_t) 1);
			enr->cases.push_back(std::make_pair("Type", num));

			// codegen() calls createType()
			enr->codegen(cgi);
		}
		else
		{
			auto pair = cgi->getType("Type");
			if(!pair) return;

			iceAssert(pair);

			iceAssert(pair->second.second == TypeKind::Enum);

			enr = dynamic_cast<EnumDef*>(pair->second.first);
			iceAssert(enr);
		}


		// create the Any type.
		#if 1
		if(cgi->getType("Any") == 0)
		{
			StructDef* any = new StructDef(Parser::Pin(), "Any");
			{
				VarDecl* type = new VarDecl(Parser::Pin(), "type", false);
				type->type.strType = "Type";

				VarDecl* data = new VarDecl(Parser::Pin(), "value", false);
				data->type.strType = "Int8*";

				any->members.push_back(type);		any->nameMap["type"] = 0;
				any->members.push_back(data);		any->nameMap["value"] = 1;
			}

			any->codegen(cgi);
		}
		#endif
	}

	void generateTypeInfo(CodegenInstance* cgi)
	{
		EnumDef* enr = dynamic_cast<EnumDef*>(cgi->getType("Type")->second.first);
		iceAssert(enr);

		// start at 2, we already have 1
		Number* num = new Number(Parser::Pin(), (int64_t) 2);


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
			cgi->rootNode->typeList.push_back(std::make_tuple("Int8", cgi->getExprTypeOfBuiltin("Int8"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Int16", cgi->getExprTypeOfBuiltin("Int16"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Int32", cgi->getExprTypeOfBuiltin("Int32"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Int64", cgi->getExprTypeOfBuiltin("Int64"), TypeKind::BuiltinType));

			cgi->rootNode->typeList.push_back(std::make_tuple("Uint8", cgi->getExprTypeOfBuiltin("Uint8"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Uint16", cgi->getExprTypeOfBuiltin("Uint16"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Uint32", cgi->getExprTypeOfBuiltin("Uint32"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Uint64", cgi->getExprTypeOfBuiltin("Uint64"), TypeKind::BuiltinType));

			cgi->rootNode->typeList.push_back(std::make_tuple("Float32", cgi->getExprTypeOfBuiltin("Float32"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Float64", cgi->getExprTypeOfBuiltin("Float64"), TypeKind::BuiltinType));
			cgi->rootNode->typeList.push_back(std::make_tuple("Bool", cgi->getExprTypeOfBuiltin("Bool"), TypeKind::BuiltinType));
		}



		for(auto tup : cgi->rootNode->typeList)
		{
			bool skip = false;

			// check for duplicates.
			// slightly inefficient.
			// todo: hashmap or something
			for(auto c : enr->cases)
			{
				if(c.first == std::get<0>(tup))
				{
					skip = true;
					break;
				}
			}

			if(skip) continue;

			enr->cases.push_back(std::make_pair(std::get<0>(tup), num));
			num = new Number(Parser::Pin(), num->ival + 1);
		}

		#if 0
		printf("Final type list for module %s\n{\n", cgi->module->getModuleName().c_str());

		int i = 1;
		for(auto c : enr->cases)
		{
			printf("\t%d: %s\n", i, c.first.c_str());
			i++;
		}
		printf("}\n\n");
		#endif
	}
}







