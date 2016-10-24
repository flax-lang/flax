// TypeInfoGeneration.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "typeinfo.h"
#include "pts.h"

using namespace Ast;
using namespace Codegen;

namespace TypeInfo
{
	void addNewType(CodegenInstance* cgi, fir::Type* stype, StructBase* str, TypeKind etype)
	{
		if(stype == 0) return;

		for(auto k : cgi->rootNode->typeList)
		{
			if(stype->isStructType())
			{
				fir::StructType* strt = dynamic_cast<fir::StructType*>(stype);
				iceAssert(strt);

				if(std::get<0>(k) == strt->getStructName().str())
					return;
			}
			else if(stype->isClassType())
			{
				fir::ClassType* clst = dynamic_cast<fir::ClassType*>(stype);
				iceAssert(clst);

				if(std::get<0>(k) == clst->getClassName().str())
					return;
			}
		}

		std::string id;
		if(stype->isStructType()) id = stype->toStructType()->getStructName().str();
		else if(stype->isClassType()) id = stype->toClassType()->getClassName().str();

		cgi->rootNode->typeList.push_back(std::make_tuple(id, stype, etype));
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

		if(cgi->getTypeByString("Type") == nullptr)
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
			auto pair = cgi->getTypeByString("Type");
			if(!pair) return;

			iceAssert(pair);

			iceAssert(pair->second.second == TypeKind::Enum);

			enr = dynamic_cast<EnumDef*>(pair->second.first);
			iceAssert(enr);
		}


		// create the Any type.
		#if 1
		if(cgi->getTypeByString("Any") == 0)
		{
			StructDef* any = new StructDef(Parser::Pin(), "Any");
			{
				VarDecl* type = new VarDecl(Parser::Pin(), "type", false);
				type->ptype = pts::NamedType::create("Type");

				VarDecl* data = new VarDecl(Parser::Pin(), "value", false);
				data->ptype = new pts::PointerType(pts::NamedType::create(INT8_TYPE_STRING));

				any->members.push_back(type);
				any->members.push_back(data);
			}

			any->codegen(cgi);
		}
		#endif
	}

	void generateTypeInfo(CodegenInstance* cgi)
	{
		EnumDef* enr = dynamic_cast<EnumDef*>(cgi->getTypeByString("Type")->second.first);
		iceAssert(enr);

		// start at 2, we already have 1
		Number* num = new Number(Parser::Pin(), (int64_t) 2);


		bool done = false;
		for(auto t : cgi->rootNode->typeList)
		{
			if(std::get<0>(t) == INT8_TYPE_STRING)
			{
				done = true;
				break;
			}
		}

		if(!done)
		{
			auto kind = TypeKind::BuiltinType;

			cgi->rootNode->typeList.push_back(std::make_tuple(INT8_TYPE_STRING, cgi->getExprTypeOfBuiltin(INT8_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(INT16_TYPE_STRING, cgi->getExprTypeOfBuiltin(INT16_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(INT32_TYPE_STRING, cgi->getExprTypeOfBuiltin(INT32_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(INT64_TYPE_STRING, cgi->getExprTypeOfBuiltin(INT64_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(INT128_TYPE_STRING, cgi->getExprTypeOfBuiltin(INT128_TYPE_STRING), kind));

			cgi->rootNode->typeList.push_back(std::make_tuple(UINT8_TYPE_STRING, cgi->getExprTypeOfBuiltin(UINT8_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(UINT16_TYPE_STRING, cgi->getExprTypeOfBuiltin(UINT16_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(UINT32_TYPE_STRING, cgi->getExprTypeOfBuiltin(UINT32_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(UINT64_TYPE_STRING, cgi->getExprTypeOfBuiltin(UINT64_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(UINT128_TYPE_STRING, cgi->getExprTypeOfBuiltin(UINT128_TYPE_STRING), kind));

			cgi->rootNode->typeList.push_back(std::make_tuple(FLOAT32_TYPE_STRING, cgi->getExprTypeOfBuiltin(FLOAT32_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(FLOAT64_TYPE_STRING, cgi->getExprTypeOfBuiltin(FLOAT64_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(FLOAT80_TYPE_STRING, cgi->getExprTypeOfBuiltin(FLOAT80_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(FLOAT128_TYPE_STRING, cgi->getExprTypeOfBuiltin(FLOAT128_TYPE_STRING), kind));

			cgi->rootNode->typeList.push_back(std::make_tuple(BOOL_TYPE_STRING, cgi->getExprTypeOfBuiltin(BOOL_TYPE_STRING), kind));

			cgi->rootNode->typeList.push_back(std::make_tuple(CHARACTER_TYPE_STRING, cgi->getExprTypeOfBuiltin(CHARACTER_TYPE_STRING), kind));
			cgi->rootNode->typeList.push_back(std::make_tuple(STRING_TYPE_STRING, cgi->getExprTypeOfBuiltin(STRING_TYPE_STRING), kind));

			cgi->rootNode->typeList.push_back(std::make_tuple(UNICODE_CHARACTER_TYPE_STRING,
				cgi->getExprTypeOfBuiltin(UNICODE_CHARACTER_TYPE_STRING), kind));

			cgi->rootNode->typeList.push_back(std::make_tuple(UNICODE_STRING_TYPE_STRING,
				cgi->getExprTypeOfBuiltin(UNICODE_STRING_TYPE_STRING), kind));
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







