// EnumerationCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t EnumDef::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	this->createType(cgi);
	return Result_t(0, 0);
}

fir::Type* EnumDef::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	if(this->createdType == 0)
		return this->createType(cgi);

	else return this->createdType;
}

fir::Type* EnumDef::reifyTypeUsingMapping(CodegenInstance* cgi, std::map<std::string, fir::Type*> tm)
{
	return 0;
}

fir::Type* EnumDef::createType(CodegenInstance* cgi)
{
	// make sure all types are the same
	// todo: remove this limitation maybe?
	if(this->didCreateType)
		return this->createdType;

	// this->ident.scope = cgi->getFullScope();

	if(cgi->isDuplicateType(this->ident))
		GenError::duplicateSymbol(cgi, this, this->ident.name, SymbolType::Type);

	fir::Type* prev = 0;
	if(this->ptype != 0)
		prev = cgi->getTypeFromParserType(this, this->ptype);

	// for(auto pair : this->cases)
	// {
	// 	if(!prev) prev = pair.second->getType(cgi);

	// 	fir::Type* t = pair.second->getType(cgi);
	// 	if(t != prev && cgi->getAutoCastDistance(t, prev) == -1)
	// 	{
	// 		error(pair.second, "Enumeration values must have the same type, have conflicting types '%s' and '%s'",
	// 			t->str().c_str(), prev->str().c_str());
	// 	}
	// }

	std::vector<std::string> fullScope = cgi->getFullScope();


	std::map<std::string, fir::ConstantValue*> casevals;
	for(auto p : this->cases)
	{
		fir::Value* v = p.second->codegen(cgi).value;
		if(!prev) prev = v->getType();

		fir::ConstantValue* cv = dynamic_cast<fir::ConstantValue*>(v);
		if(!cv) error(p.second, "Enumeration cases have to be constant");

		if(cv->getType() != prev && (prev->isIntegerType() || prev->isFloatingPointType()))
			cv = fir::createConstantValueCast(cv, prev);

		else if(cv->getType() != prev)
		{
			error(p.second, "Enumeration values must have the same type, have conflicting types '%s' and '%s'",
				cv->getType()->str().c_str(), prev->str().c_str());
		}

		casevals[p.first] = cv;
	}


	// now that they're all the same type:
	this->didCreateType = true;

	fir::EnumType* type = fir::EnumType::get(this->ident, prev, casevals);
	cgi->addNewType(type, this, TypeKind::Enum);
	this->createdType = type;

	return type;
}
















