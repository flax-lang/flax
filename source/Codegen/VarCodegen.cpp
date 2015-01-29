// VarCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t VarRef::codegen(CodegenInstance* cgi)
{
	llvm::Value* val = cgi->getSymInst(this->name);
	if(!val)
		error(this, "Unknown variable name '%s'", this->name.c_str());

	return Result_t(cgi->mainBuilder.CreateLoad(val, this->name), val);
}

Result_t VarDecl::codegen(CodegenInstance* cgi)
{
	if(cgi->isDuplicateSymbol(this->name))
		error(this, "Redefining duplicate symbol '%s'", this->name.c_str());

	llvm::Function* func = cgi->mainBuilder.GetInsertBlock()->getParent();
	llvm::Value* val = nullptr;

	llvm::AllocaInst* ai = cgi->allocateInstanceInBlock(func, this);
	cgi->getSymTab()[this->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this);


	TypePair_t* cmplxtype = cgi->getType(this->type);

	if(this->initVal && !cmplxtype)
	{
		this->initVal = cgi->autoCastType(this, this->initVal);
		val = this->initVal->codegen(cgi).result.first;
	}
	else if(cgi->isBuiltinType(this) || cgi->isArrayType(this))
	{
		val = cgi->getDefaultValue(this);
	}
	else
	{
		// get our type
		if(!cmplxtype)
			error(this, "Invalid type");

		TypePair_t* pair = cmplxtype;
		if(pair->first->isStructTy())
		{
			Struct* str = nullptr;
			assert(pair->second.first);
			assert((str = dynamic_cast<Struct*>(pair->second.first)));
			assert(pair->second.second == ExprType::Struct);

			val = cgi->mainBuilder.CreateCall(cgi->mainModule->getFunction(str->initFunc->getName()), ai);

			if(this->initVal)
			{
				llvm::Value* ival = this->initVal->codegen(cgi).result.first;

				if(ival->getType() == ai->getType()->getPointerElementType())
					return Result_t(cgi->mainBuilder.CreateStore(ival, ai), ai);

				else
					return cgi->callOperatorOnStruct(pair, ai, ArithmeticOp::Assign, ival);
			}
			else
			{
				return Result_t(val, ai);
			}
		}
		else
		{
			error(this, "Unknown type encountered");
		}
	}

	if(val->getType() != ai->getType()->getPointerElementType())
	{
		error("Invalid assignment, type %s cannot be assigned to type %s", cgi->getReadableType(val->getType()).c_str(),
			cgi->getReadableType(ai->getType()->getPointerElementType()).c_str());
	}

	cgi->mainBuilder.CreateStore(val, ai);
	return Result_t(val, ai);
}














