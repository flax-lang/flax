// EnumerationCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

#if 0
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

	this->irb.CreateStore(res.value, gep);
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
#endif

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
	if(this->ptype != 0)
		prev = cgi->getTypeFromParserType(this, this->ptype);

	for(auto pair : this->cases)
	{
		if(!prev) prev = pair.second->getType(cgi);

		fir::Type* t = pair.second->getType(cgi);
		if(t != prev && cgi->getAutoCastDistance(t, prev) == -1)
		{
			error(pair.second, "Enumeration values must have the same type, have conflicting types '%s' and '%s'",
				t->str().c_str(), prev->str().c_str());
		}
	}

	if(prev->isPrimitiveType() && prev->toPrimitiveType()->isLiteralType())
		prev = prev->toPrimitiveType()->getUnliteralType();

	std::deque<std::string> fullScope = cgi->getFullScope();


	std::map<std::string, fir::ConstantValue*> casevals;
	for(auto p : this->cases)
	{
		fir::Value* v = p.second->codegen(cgi).value;
		fir::ConstantValue* cv = dynamic_cast<fir::ConstantValue*>(v);
		if(!cv) error(p.second, "Enumeration cases have to be constant");

		if(cv->getType() != prev && (prev->isIntegerType() || prev->isFloatingPointType()))
			cv = fir::createConstantValueCast(cv, prev);

		casevals[p.first] = cv;
	}


	// now that they're all the same type:
	this->didCreateType = true;

	fir::EnumType* type = fir::EnumType::get(this->ident, prev, casevals);
	cgi->addNewType(type, this, TypeKind::Enum);
	this->createdType = type;

	return type;
}
















