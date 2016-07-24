// TupleCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


fir::TupleType* Tuple::getType(CodegenInstance* cgi)
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

		this->ident.name = "__anonymoustuple_" + std::to_string(cgi->typeMap.size());
		this->createdType = fir::TupleType::get(this->ltypes, cgi->getContext());
		this->didCreateType = true;

		// todo: debate, should we add this?
		// edit: no.
		// cgi->addNewType(this->createdType, this, TypeKind::Tuple);
	}

	return this->createdType;
}

fir::Type* Tuple::createType(CodegenInstance* cgi, std::unordered_map<std::string, fir::Type*> instantiatedGenericTypes)
{
	(void) cgi;
	return 0;
}

Result_t CodegenInstance::doTupleAccess(fir::Value* selfPtr, Number* num, bool createPtr)
{
	iceAssert(selfPtr);
	iceAssert(num);

	fir::Type* type = selfPtr->getType()->getPointerElementType();
	iceAssert(type->isTupleType());

	// quite simple, just get the number (make sure it's a Ast::Number)
	// and do a structgep.

	if((size_t) num->ival >= type->toTupleType()->getElementCount())
		error(num, "Tuple does not have %d elements, only %zd", (int) num->ival + 1, type->toTupleType()->getElementCount());

	fir::Value* gep = this->builder.CreateStructGEP(selfPtr, num->ival);
	return Result_t(this->builder.CreateLoad(gep), createPtr ? gep : 0);
}

Result_t Tuple::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Value* gep = 0;
	if(extra)
	{
		gep = extra;
	}
	else
	{
		gep = cgi->getStackAlloc(this->getType(cgi));
	}

	iceAssert(gep);

	fir::TupleType* tuptype = gep->getType()->getPointerElementType()->toTupleType();
	iceAssert(tuptype);

	// set all the values.
	// do the gep for each.

	iceAssert(tuptype->getElementCount() == this->values.size());

	for(unsigned int i = 0; i < tuptype->getElementCount(); i++)
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



















