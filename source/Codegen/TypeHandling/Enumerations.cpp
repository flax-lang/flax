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
				return Result_t(fir::ConstantValue::getNullValue(p.second->getType(this)), 0);
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
	fir::Value* gep = this->irb.CreateStructGEP(alloca, 0);

	this->irb.CreateStore(res.result.first, gep);
	return Result_t(this->irb.CreateLoad(alloca), alloca);
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

fir::Type* EnumDef::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->createdType == 0)
		return this->createType(cgi);

	else return this->createdType;
}

fir::Type* EnumDef::createType(CodegenInstance* cgi)
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
			prev = pair.second->getType(cgi);


		fir::Type* t = pair.second->getType(cgi);
		if(t != prev)
			error(pair.second, "Enumeration values must have the same type, have %s and %s", t->str().c_str(), prev->str().c_str());


		prev = t;
	}



	std::deque<std::string> fullScope = cgi->getFullScope();

	fir::StructType* wrapper = fir::StructType::create(this->ident, { { "case", prev } }, cgi->getContext());

	// now that they're all the same type:
	cgi->addNewType(wrapper, this, TypeKind::Enum);
	this->didCreateType = true;

	this->createdType = wrapper;
	return wrapper;
}
















