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
		for(auto k : cgi->rootNode->typeList)
		{
			if(std::get<0>(k) == stype->getStructName())
				return;
		}

		cgi->rootNode->typeList.push_back(std::make_tuple(stype->getStructName(), stype, etype));
	}

	size_t getIndexForType(Codegen::CodegenInstance* cgi, llvm::Type* type)
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

		return 0;
	}

	void initialiseTypeInfo(CodegenInstance* cgi)
	{
		Enumeration* enr = 0;

		if(cgi->getType("Type") == nullptr)
		{
			enr = new Enumeration(Parser::PosInfo(), "Type");
			enr->isStrong = true;

			Number* num = new Number(Parser::PosInfo(), (int64_t) 1);
			enr->cases.push_back(std::make_pair("Type", num));

			// note: Enumeration does nothing in codegen()
			// fix this if that happens to change in the future.

			enr->createType(cgi);
		}
		else
		{
			auto pair = cgi->getType("Type");
			assert(pair);

			assert(pair->second.second == ExprKind::Enum);

			enr = dynamic_cast<Enumeration*>(pair->second.first);
			assert(enr);
		}


		// create the Any type.
		if(cgi->getType("Any") == nullptr)
		{
			Struct* any = new Struct(Parser::PosInfo(), "Any");
			{
				VarDecl* type = new VarDecl(Parser::PosInfo(), "type", false);
				type->type.strType = "Type";

				VarDecl* data = new VarDecl(Parser::PosInfo(), "data", false);
				data->type.strType = "Int8*";

				any->members.push_back(type);	any->nameMap["type"] = 0;
				any->members.push_back(data);	any->nameMap["data"] = 1;
			}

			any->createType(cgi);
			any->codegen(cgi);
		}
	}

	void generateTypeInfo(CodegenInstance* cgi)
	{
		Enumeration* enr = dynamic_cast<Enumeration*>(cgi->getType("Type")->second.first);
		assert(enr);

		// start at 2, we already have 1
		Number* num = new Number(Parser::PosInfo(), (int64_t) 2);


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
			bool skip = false;
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
			num = new Number(Parser::PosInfo(), num->ival + 1);
		}

		printf("Final type list for module %s\n{\n", cgi->mainModule->getModuleIdentifier().c_str());

		int i = 1;
		for(auto c : enr->cases)
		{
			printf("\t%d: %s\n", i, c.first.c_str());
			i++;
		}
		printf("}\n\n");
	}
}







