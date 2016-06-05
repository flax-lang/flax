// TupleCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


fir::StructType* Tuple::getType(CodegenInstance* cgi)
{
	// todo: handle named tuples.
	// would probably just be handled as implicit anon structs
	// (randomly generated name or something), with appropriate code to handle
	// assignment to and from.

	if(this->ltypes.size() == 0)
	{
		iceAssert(!this->didCreateType);

		for(Expr* e : this->values)
			this->ltypes.push_back(cgi->getExprType(e));

		this->name = "__anonymoustuple_" + std::to_string(cgi->typeMap.size());
		this->cachedLlvmType = fir::StructType::getLiteral(this->ltypes, cgi->getContext());
		this->didCreateType = true;

		// todo: debate, should we add this?
		cgi->addNewType(this->cachedLlvmType, this, TypeKind::Tuple);
	}

	return this->cachedLlvmType;
}

fir::Type* Tuple::createType(CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes)
{
	(void) cgi;
	return 0;
}

Result_t CodegenInstance::doTupleAccess(fir::Value* selfPtr, Number* num, bool createPtr)
{
	iceAssert(selfPtr);
	iceAssert(num);

	fir::Type* type = selfPtr->getType()->getPointerElementType();
	iceAssert(type->isStructType());

	// quite simple, just get the number (make sure it's a Ast::Number)
	// and do a structgep.

	if((size_t) num->ival >= type->toStructType()->getElementCount())
		error(num, "Tuple does not have %d elements, only %zd", (int) num->ival + 1, type->toStructType()->getElementCount());

	fir::Value* gep = this->builder.CreateStructGEP(selfPtr, num->ival);
	return Result_t(this->builder.CreateLoad(gep), createPtr ? gep : 0);
}

Result_t Tuple::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	(void) rhs;

	fir::Value* gep = 0;
	if(lhsPtr)
	{
		gep = lhsPtr;
	}
	else
	{
		gep = cgi->allocateInstanceInBlock(this->getType(cgi));
	}

	iceAssert(gep);

	fir::StructType* strtype = gep->getType()->getPointerElementType()->toStructType();;
	iceAssert(strtype);

	// set all the values.
	// do the gep for each.

	iceAssert(strtype->getElementCount() == this->values.size());

	for(unsigned int i = 0; i < strtype->getElementCount(); i++)
	{
		fir::Value* member = cgi->builder.CreateStructGEP(gep, i);
		fir::Value* val = this->values[i]->codegen(cgi).result.first;

		// printf("%s -> %s\n", cgi->getReadableType(val).c_str(), cgi->getReadableType(member->getType()->getPointerElementType()).c_str());
		val = cgi->autoCastType(member->getType()->getPointerElementType(), val);

		if(val->getType() != member->getType()->getPointerElementType())
			error(this, "Element %d of tuple is mismatched, expected '%s' but got '%s'", i,
				cgi->getReadableType(member->getType()->getPointerElementType()).c_str(), cgi->getReadableType(val).c_str());

		cgi->builder.CreateStore(val, member);
	}

	return Result_t(cgi->builder.CreateLoad(gep), gep);
}



















