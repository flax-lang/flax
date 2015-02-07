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
	llvm::Value* val = cgi->getSymInst(this, this->name);
	if(!val)
		GenError::unknownSymbol(this, this->name, SymbolType::Variable);

	return Result_t(cgi->mainBuilder.CreateLoad(val, this->name), val);
}

Result_t VarDecl::codegen(CodegenInstance* cgi)
{
	if(cgi->isDuplicateSymbol(this->name))
		GenError::duplicateSymbol(this, this->name, SymbolType::Variable);

	llvm::Function* func = cgi->mainBuilder.GetInsertBlock()->getParent();
	llvm::Value* val = nullptr;

	TypePair_t* cmplxtype = cgi->getType(this->type);
	llvm::AllocaInst* ai = nullptr;

	if(this->type == "Inferred")
	{
		if(!this->initVal)
			error(this, "Type inference requires an initial assignment to infer type");

		assert(!cmplxtype);
		val = this->initVal->codegen(cgi).result.first;

		if(cgi->isBuiltinType(this->initVal))
		{
			this->varType = cgi->determineVarType(this->initVal);
			this->type = Parser::getVarTypeString(this->varType);
		}
		else
		{
			// it's not a builtin type
			ai = cgi->allocateInstanceInBlock(func, val->getType(), this->name);
			this->inferredLType = val->getType();
		}
	}
	else if(this->initVal)
	{
		this->initVal = cgi->autoCastType(this, this->initVal);
		val = this->initVal->codegen(cgi).result.first;
	}



	if(!ai)	ai = cgi->allocateInstanceInBlock(func, this);
	if(this->initVal && !cmplxtype && this->type != "Inferred")
	{
		// ...
	}
	else if(!this->initVal && (cgi->isBuiltinType(this) || cgi->isArrayType(this) || cgi->isPtr(this)))
	{
		val = cgi->getDefaultValue(this);
	}
	else
	{
		for(TypeMap_t* tm : cgi->visibleTypes)
		{
			for(auto pair : *tm)
			{
				llvm::Type* ltype = pair.second.first;
				if(ltype == this->inferredLType)
				{
					cmplxtype = &pair.second;
					break;
				}
			}
		}


		if(cmplxtype)
		{
			// automatically call the init() function
			if(!this->disableAutoInit && !this->initVal)
			{
				// TODO: constructor args
				std::vector<llvm::Value*> args;
				args.push_back(ai);

				llvm::Function* initfunc = cgi->getStructInitialiser(this, cmplxtype, args);
				val = cgi->mainBuilder.CreateCall(initfunc, args);
			}
		}

		cgi->addSymbol(this->name, ai, this);

		if(this->initVal && !cmplxtype)
		{
			// this only works if we don't call a constructor
			return cgi->doBinOpAssign(this, new VarRef(this->posinfo, this->name), this->initVal, ArithmeticOp::Assign, val, ai, val);
		}
		else
		{
			cgi->mainBuilder.CreateStore(val, ai);
			return Result_t(val, ai);
		}
	}


	if(val->getType() != ai->getType()->getPointerElementType())
	{
		Number* n = 0;
		if(val->getType()->isIntegerTy() && (n = dynamic_cast<Number*>(this->initVal)) && n->ival == 0)
		{
			val = llvm::Constant::getNullValue(ai->getType()->getPointerElementType());
		}
		else
		{
			GenError::invalidAssignment(this, val->getType(), ai->getType()->getPointerElementType());
		}
	}

	cgi->addSymbol(this->name, ai, this);
	cgi->mainBuilder.CreateStore(val, ai);
	return Result_t(val, ai);
}














