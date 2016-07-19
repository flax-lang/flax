// EnumerationCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t CodegenInstance::getEnumerationCaseValue(Expr* user, TypePair_t* tp, std::string caseName, bool actual)
{
	EnumDef* enr = dynamic_cast<EnumDef*>(tp->second.first);
	if(!enr) printf("wtf?? %s\n", typeid(*tp->second.first).name());
	iceAssert(enr);

	Result_t res(0, 0);
	bool found = false;
	for(auto p : enr->cases)
	{
		if(p.first == caseName)
		{
			if(actual)
			{
				res = p.second->codegen(this);
				found = true;
				break;
			}
			else
			{
				return Result_t(fir::ConstantValue::getNullValue(this->getExprType(p.second)), 0);
			}
		}
	}


	if(!found)
		error(user, "Enum '%s' has no such case '%s'", enr->ident.name.c_str(), caseName.c_str());

	if(!enr->isStrong)
		return res;


	// strong enum.
	// create a temp alloca, then use GEP to set the value, then return.
	fir::Value* alloca = this->getStackAlloc(tp->first);
	fir::Value* gep = this->builder.CreateStructGEP(alloca, 0);

	this->builder.CreateStore(res.result.first, gep);
	return Result_t(this->builder.CreateLoad(alloca), alloca);
}


Result_t CodegenInstance::getEnumerationCaseValue(Expr* lhs, Expr* rhs, bool actual)
{
	VarRef* enumName = dynamic_cast<VarRef*>(lhs);
	VarRef* caseName = dynamic_cast<VarRef*>(rhs);
	iceAssert(enumName);

	if(!caseName)
		error(rhs, "Expected identifier after enumeration access");

	TypePair_t* tp = this->getTypeByString(enumName->name);
	iceAssert(tp);
	iceAssert(tp->second.second == TypeKind::Enum);

	return this->getEnumerationCaseValue(rhs, tp, caseName->name);
}

Result_t EnumDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	this->createType(cgi);
	return Result_t(0, 0);
}

fir::Type* EnumDef::createType(CodegenInstance* cgi, std::unordered_map<std::string, fir::Type*> instantiatedGenericTypes)
{
	// make sure all types are the same
	// todo: remove this limitation maybe?
	if(this->didCreateType)
		return this->createdType;


	this->ident.scope = cgi->getFullScope();

	if(cgi->isDuplicateType(this->ident))
		GenError::duplicateSymbol(cgi, this, this->ident.name, SymbolType::Type);


	fir::Type* prev = 0;
	for(auto pair : this->cases)
	{
		if(!prev)
			prev = cgi->getExprType(pair.second);


		fir::Type* t = cgi->getExprType(pair.second);
		if(t != prev)
			error(pair.second, "Enumeration values must have the same type, have %s and %s", cgi->getReadableType(pair.second).c_str(),
				cgi->getReadableType(prev).c_str());


		prev = t;
	}



	std::deque<std::string> fullScope = cgi->getFullScope();

	fir::StructType* wrapper = fir::StructType::createNamed(this->ident, { prev }, cgi->getContext());
	// wrapper->setBody(std::vector<fir::Type*>({ prev }));

	// now that they're all the same type:
	cgi->addNewType(wrapper, this, TypeKind::Enum);
	this->didCreateType = true;

	this->createdType = wrapper;
	return wrapper;
}
















