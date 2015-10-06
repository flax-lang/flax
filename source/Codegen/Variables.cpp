// VarCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/dependency.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/GLobalVariable.h"

using namespace Ast;
using namespace Codegen;

Result_t VarRef::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	llvm::Value* val = cgi->getSymInst(this, this->name);
	if(!val)
		GenError::unknownSymbol(cgi, this, this->name, SymbolType::Variable);

	return Result_t(cgi->builder.CreateLoad(val, this->name), val);
}









llvm::Value* VarDecl::doInitialValue(Codegen::CodegenInstance* cgi, TypePair_t* cmplxtype, llvm::Value* val, llvm::Value* valptr,
	llvm::Value* storage, bool shouldAddToSymtab)
{
	llvm::Value* ai = storage;
	bool didAddToSymtab = false;

	if(this->initVal && !cmplxtype && this->type.strType != "Inferred" && !cgi->isAnyType(val->getType()) && !val->getType()->isArrayTy())
	{
		// ...
	}
	else if(!this->initVal && (cgi->isBuiltinType(this) || cgi->isArrayType(this) || cgi->isPtr(this)))
	{
		val = cgi->getDefaultValue(this);
		iceAssert(val);
	}
	else
	{
		if(this->inferredLType)
		{
			cmplxtype = cgi->getType(this->inferredLType);
		}
		else
		{
			if(this->type.strType.find("::") != std::string::npos)
				cmplxtype = cgi->getType(cgi->mangleRawNamespace(this->type.strType));

			else
				cmplxtype = cgi->getType(this->type.strType);
		}

		if(!ai)
		{
			iceAssert(cmplxtype);
			iceAssert((ai = cgi->allocateInstanceInBlock(cmplxtype->first)));
		}

		if(cmplxtype)
		{
			// automatically call the init() function
			if(!this->disableAutoInit && !this->initVal)
			{
				llvm::Value* unwrappedAi = cgi->lastMinuteUnwrapType(this, ai);
				if(unwrappedAi != ai)
				{
					cmplxtype = cgi->getType(unwrappedAi->getType()->getPointerElementType());
					iceAssert(cmplxtype);
				}

				if(!cgi->isEnum(ai->getType()->getPointerElementType()))
				{
					std::vector<llvm::Value*> args { unwrappedAi };

					llvm::Function* initfunc = cgi->getStructInitialiser(this, cmplxtype, args);
					iceAssert(initfunc);

					val = cgi->builder.CreateCall(initfunc, args);
				}
			}
		}

		if(shouldAddToSymtab)
		{
			cgi->addSymbol(this->name, ai, this);
			didAddToSymtab = true;
		}


		if(this->initVal && (!cmplxtype || reinterpret_cast<StructBase*>(cmplxtype->second.first)->name == "Any"
			|| cgi->isAnyType(val->getType())))
		{
			// this only works if we don't call a constructor

			// todo: make this work better, maybe as a parameter to doBinOpAssign
			// it currently doesn't know (and maybe shouldn't) if we're doing first assign or not
			// if we're an immutable var (ie. val or let), we first set immutable to false so we
			// can store the value, then set it back to whatever it was.

			bool wasImmut = this->immutable;
			this->immutable = false;
			auto res = cgi->doBinOpAssign(this, /* todo: this varref leaks */ new VarRef(this->posinfo, this->name), this->initVal,
				ArithmeticOp::Assign, cgi->builder.CreateLoad(ai), ai, val, valptr);

			this->immutable = wasImmut;
			return res.result.first;
		}
		else if(!cmplxtype && !this->initVal)
		{
			if(!val)
				val = cgi->getDefaultValue(this);

			cgi->builder.CreateStore(val, ai);
			return val;
		}
		else if(cmplxtype && this->initVal)
		{
			if(ai->getType()->getPointerElementType() != val->getType())
				ai = cgi->lastMinuteUnwrapType(this, ai);


			if(ai->getType()->getPointerElementType() != val->getType())
				GenError::invalidAssignment(cgi, this, ai->getType()->getPointerElementType(), val->getType());

			cgi->builder.CreateStore(val, ai);
			return val;
		}
		else
		{
			if(valptr)
				val = cgi->builder.CreateLoad(valptr);

			else
				return val;
		}
	}

	if(!ai)
	{
		error(this, "ai is null");
	}

	if(!didAddToSymtab && shouldAddToSymtab)
		cgi->addSymbol(this->name, ai, this);


	if(val->getType() != ai->getType()->getPointerElementType())
	{
		Number* n = 0;
		if(val->getType()->isIntegerTy() && (n = dynamic_cast<Number*>(this->initVal)) && n->ival == 0)
		{
			val = llvm::Constant::getNullValue(ai->getType()->getPointerElementType());
		}
		else if(val->getType()->isIntegerTy() && ai->getType()->getPointerElementType()->isIntegerTy())
		{
			Number* n = 0;

			if((n = dynamic_cast<Number*>(this->initVal)))
			{
				uint64_t max = pow(2, ai->getType()->getPointerElementType()->getIntegerBitWidth()) - 1;
				if(max == 0)
					max = UINT64_MAX;

				if((uint64_t) n->ival < max)
				{
					val = cgi->builder.CreateIntCast(val, ai->getType()->getPointerElementType(), false);
				}
				else
				{
					error(this, "Trying to assign value of %s to variable with max value %s (int%d)", std::to_string(n->ival).c_str(),
						std::to_string(max).c_str(), ai->getType()->getPointerElementType()->getIntegerBitWidth());
				}
			}
		}
		else
		{
			GenError::invalidAssignment(cgi, this, ai->getType()->getPointerElementType(), val->getType());
		}
	}

	cgi->builder.CreateStore(val, ai);
	return val;
}





void VarDecl::inferType(CodegenInstance* cgi)
{
	if(this->inferredLType != 0)
		return;

	if(this->type.strType == "Inferred")
	{
		if(this->initVal == nullptr)
			error(this, "Type inference requires an initial assignment to infer type");


		llvm::Type* vartype = cgi->getLlvmType(this->initVal);
		if(vartype == nullptr || vartype->isVoidTy())
			GenError::nullValue(cgi, this->initVal);


		if(cgi->isAnyType(vartype))
		{
			// todo: fix this shit
			// but how?
			warn(this, "Assigning a value of type 'Any' using type inference will not unwrap the value");
		}

		this->inferredLType = cgi->getLlvmType(this->initVal);

		if(cgi->isBuiltinType(this->initVal) && !this->inferredLType->isStructTy())
			this->type = cgi->getReadableType(this->initVal);
	}
	else
	{
		// not actually needed??????
		// std::deque<DepNode*> deps = cgi->dependencyGraph->findDependenciesOf(this);

		this->inferredLType = cgi->parseAndGetOrInstantiateType(this, this->type.strType);
		iceAssert(this->inferredLType);
	}
}






Result_t VarDecl::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* _rhs)
{
	if(cgi->isDuplicateSymbol(this->name))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Variable);

	if(FunctionTree* ft = cgi->getCurrentFuncTree())
	{
		for(auto sub : ft->subs)
		{
			if(sub->nsName == this->name)
			{
				error(this, "Declaration of variable %s conflicts with namespace declaration within scope %s",
					this->name.c_str(), ft->nsName.c_str());
			}
		}
	}

	if(Func* fn = cgi->getCurrentFunctionScope())
	{
		if(fn->decl->parentClass != 0 && !fn->decl->isStatic)
		{
			// check.
			if(this->name == "self")
				error(this, "Cannot have a parameter named 'self' in a method declaration");

			else if(this->name == "super")
				error(this, "Cannot have a parameter named 'super' in a method declaration");
		}
	}





	llvm::Value* val = nullptr;
	llvm::Value* valptr = nullptr;

	llvm::Value* ai = nullptr;

	if(this->inferredLType == nullptr)
		this->inferType(cgi);


	if(!this->isGlobal)
	{
		ai = cgi->allocateInstanceInBlock(this);
		iceAssert(ai->getType()->getPointerElementType() == this->inferredLType);
	}

	if(this->initVal)
	{
		auto r = this->initVal->codegen(cgi, ai).result;

		val = r.first;
		valptr = r.second;
	}




	// TODO: call global constructors
	if(this->isGlobal)
	{
		if(this->attribs & Attr_VisPublic)
		{
			// hmm.
			error(this, "Public global variables are currently not supported.");
		}
		else
		{
			ai = new llvm::GlobalVariable(*cgi->module, this->inferredLType, this->immutable, llvm::GlobalValue::InternalLinkage,
				llvm::Constant::getNullValue(this->inferredLType), this->name);
		}

		llvm::Type* ltype = ai->getType()->getPointerElementType();

		if(this->initVal)
		{
			iceAssert(val);

			if(llvm::cast<llvm::Constant>(val))
			{
				cgi->autoCastType(ai->getType()->getPointerElementType(), val, valptr);
				llvm::cast<llvm::GlobalVariable>(ai)->setInitializer(llvm::cast<llvm::Constant>(val));
			}
			else
			{
				cgi->addGlobalConstructedValue(ai, val);
			}
		}
		else if(ltype->isStructTy() && !cgi->isTupleType(ltype))
		{
			// oopsies. we got to call the struct constructor.
			TypePair_t* tp = cgi->getType(ltype);
			iceAssert(tp);

			StructBase* sb = dynamic_cast<StructBase*>(tp->second.first);
			iceAssert(sb);

			llvm::Function* candidate = cgi->getDefaultConstructor(this, ai->getType(), sb);
			cgi->addGlobalConstructor(ai, candidate);
		}
		else if(cgi->isTupleType(ltype))
		{
			llvm::StructType* stype = llvm::cast<llvm::StructType>(ltype);

			int i = 0;
			for(llvm::Type* t : stype->elements())
			{
				if(cgi->isTupleType(t))
				{
					error(this, "global nested tuples not supported yet");
				}
				else if(t->isStructTy())
				{
					TypePair_t* tp = cgi->getType(t);
					iceAssert(tp);

					cgi->addGlobalTupleConstructor(ai, i, cgi->getDefaultConstructor(this, t->getPointerTo(),
						dynamic_cast<StructBase*>(tp->second.first)));
				}
				else
				{
					cgi->addGlobalTupleConstructedValue(ai, i, llvm::Constant::getNullValue(t));
				}

				i++;
			}
		}


		cgi->addSymbol(this->name, ai, this);

		FunctionTree* ft = cgi->getCurrentFuncTree();
		iceAssert(ft);

		ft->vars[this->name] = *cgi->getSymPair(this, this->name);
	}
	else
	{
		TypePair_t* cmplxtype = 0;
		if(this->type.strType != "Inferred")
		{
			iceAssert(this->inferredLType);
			cmplxtype = cgi->getType(this->inferredLType);
		}

		this->doInitialValue(cgi, cmplxtype, val, valptr, ai, true);
	}

	return Result_t(val, ai);
}














