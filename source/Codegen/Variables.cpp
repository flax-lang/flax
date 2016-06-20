// VarCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "operators.h"


using namespace Ast;
using namespace Codegen;

Result_t VarRef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Value* val = cgi->getSymInst(this, this->name);
	if(!val)
		GenError::unknownSymbol(cgi, this, this->name, SymbolType::Variable);

	return Result_t(cgi->builder.CreateLoad(val), val);
}









fir::Value* VarDecl::doInitialValue(Codegen::CodegenInstance* cgi, TypePair_t* cmplxtype, fir::Value* val, fir::Value* valptr,
	fir::Value* storage, bool shouldAddToSymtab)
{
	fir::Value* ai = storage;
	bool didAddToSymtab = false;

	if(this->initVal && !val)
	{
		// means the expression is void
		GenError::nullValue(cgi, this->initVal);
	}


	iceAssert(this->inferredLType);
	if(val != 0)
	{
		// cast.
		val = cgi->autoCastType(this->inferredLType, val, valptr);
	}





	if(this->initVal && !cmplxtype && this->type.strType != "Inferred" && !cgi->isAnyType(val->getType()) && !val->getType()->isArrayType())
	{
		// ...
		// handled below
	}
	else if(!this->initVal && (cgi->isBuiltinType(this) || cgi->isArrayType(this) || cgi->isPtr(this)))
	{
		val = cgi->getDefaultValue(this);
		iceAssert(val);
	}
	else
	{
		iceAssert(ai);
		iceAssert(this->inferredLType);
		cmplxtype = cgi->getType(this->inferredLType);

		if(cmplxtype)
		{
			// automatically call the init() function
			if(!this->disableAutoInit && !this->initVal)
			{
				fir::Value* unwrappedAi = cgi->lastMinuteUnwrapType(this, ai);
				if(unwrappedAi != ai)
				{
					cmplxtype = cgi->getType(unwrappedAi->getType()->getPointerElementType());
					iceAssert(cmplxtype);
				}

				if(!cgi->isEnum(ai->getType()->getPointerElementType()))
				{
					std::vector<fir::Value*> args { unwrappedAi };

					fir::Function* initfunc = cgi->getStructInitialiser(this, cmplxtype, args);
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


		if(this->initVal && (!cmplxtype || dynamic_cast<StructBase*>(cmplxtype->second.first)->name == "Any" || cgi->isAnyType(val->getType())))
		{
			// this only works if we don't call a constructor

			// todo: make this work better, maybe as a parameter to doBinOpAssign
			// it currently doesn't know (and maybe shouldn't) if we're doing first assign or not
			// if we're an immutable var (ie. val or let), we first set immutable to false so we
			// can store the value, then set it back to whatever it was.

			bool wasImmut = this->immutable;
			this->immutable = false;

			auto vr = new VarRef(this->pin, this->name);
			auto res = Operators::performActualAssignment(cgi, this, vr, this->initVal, ArithmeticOp::Assign, cgi->builder.CreateLoad(ai),
				ai, val, valptr);

			delete vr;

			this->immutable = wasImmut;
			return res.result.first;
		}
		else if(!cmplxtype && !this->initVal)
		{
			iceAssert(val);
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


	iceAssert(ai);

	if(!didAddToSymtab && shouldAddToSymtab)
		cgi->addSymbol(this->name, ai, this);

	cgi->builder.CreateStore(val, ai);
	return val;
}





void VarDecl::inferType(CodegenInstance* cgi)
{
	if(this->type.strType == "Inferred")
	{
		if(this->initVal == nullptr)
			error(this, "Type inference requires an initial assignment to infer type");


		fir::Type* vartype = cgi->getExprType(this->initVal);
		if(vartype == nullptr || vartype->isVoidType())
			GenError::nullValue(cgi, this->initVal);


		if(cgi->isAnyType(vartype))
		{
			// todo: fix this shit
			// but how?
			warn(this, "Assigning a value of type 'Any' using type inference will not unwrap the value");
		}

		this->inferredLType = vartype;

		if(cgi->isBuiltinType(this->initVal) && !this->inferredLType->isStructType())
			this->type = cgi->getReadableType(this->initVal);
	}
	else
	{
		// not actually needed??????
		// std::deque<DepNode*> deps = cgi->dependencyGraph->findDependenciesOf(this);

		this->inferredLType = cgi->parseAndGetOrInstantiateType(this, this->type.strType);
		if(!this->inferredLType) error(this, "invalid type %s", this->type.strType.c_str());

		iceAssert(this->inferredLType);
	}
}






Result_t VarDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
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





	fir::Value* val = nullptr;
	fir::Value* valptr = nullptr;

	fir::Value* ai = nullptr;

	if(this->inferredLType == nullptr)
		this->inferType(cgi);


	if(!this->isGlobal)
	{
		ai = cgi->getStackAlloc(this->inferredLType);
		iceAssert(ai->getType()->getPointerElementType() == this->inferredLType);
	}


	if(this->initVal)
	{
		ValPtr_t r;

		if(isGlobal && this->inferredLType->isStructType())
		{
			// can't call directly for globals, since we cannot call the function directly.
			// todo: if it's a struct, and we need to call a constructor...
			error(this, "enotsup");
		}
		else
		{
			r = this->initVal->codegen(cgi, ai).result;
		}

		val = r.first;
		valptr = r.second;
	}


	this->mangledName = cgi->mangleWithNamespace(this->name);

	// TODO: call global constructors
	if(this->isGlobal)
	{
		ai = cgi->module->createGlobalVariable(mangledName, this->inferredLType, fir::ConstantValue::getNullValue(this->inferredLType),
			this->immutable, this->attribs & Attr_VisPublic ? fir::LinkageType::External : fir::LinkageType::Internal);

		fir::Type* ltype = ai->getType()->getPointerElementType();

		if(this->initVal)
		{
			iceAssert(val);
			cgi->addGlobalConstructedValue(ai, val);
		}
		else if(ltype->isStructType() && !cgi->isTupleType(ltype))
		{
			// oopsies. we got to call the struct constructor.
			TypePair_t* tp = cgi->getType(ltype);
			iceAssert(tp);

			StructBase* sb = dynamic_cast<StructBase*>(tp->second.first);
			iceAssert(sb);

			fir::Function* candidate = cgi->getDefaultConstructor(this, ai->getType(), sb);
			cgi->addGlobalConstructor(ai, candidate);
		}
		else if(cgi->isTupleType(ltype))
		{
			fir::StructType* stype = dynamic_cast<fir::StructType*>(ltype);

			int i = 0;
			for(fir::Type* t : stype->getElements())
			{
				if(cgi->isTupleType(t))
				{
					error(this, "global nested tuples not supported yet");
				}
				else if(t->isStructType())
				{
					TypePair_t* tp = cgi->getType(t);
					iceAssert(tp);

					cgi->addGlobalTupleConstructor(ai, i, cgi->getDefaultConstructor(this, t->getPointerTo(),
						dynamic_cast<StructBase*>(tp->second.first)));
				}
				else
				{
					cgi->addGlobalTupleConstructedValue(ai, i, fir::ConstantValue::getNullValue(t));
				}

				i++;
			}
		}


		cgi->addSymbol(mangledName, ai, this);

		FunctionTree* ft = cgi->getCurrentFuncTree();
		iceAssert(ft);

		ft->vars[this->name] = *cgi->getSymPair(this, mangledName);
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

	if(this->immutable)
		ai->makeImmutable();

	if(!this->isGlobal)
		return Result_t(cgi->builder.CreateLoad(ai), ai);

	else
		return Result_t(0, ai);
}














