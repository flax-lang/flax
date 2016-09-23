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
	{
		// check for functions
		auto fns = cgi->module->getFunctionsWithName(Identifier(this->name, IdKind::Function));
		std::deque<fir::Function*> cands;

		for(auto fn : fns)
		{
			// check that the function we found wasn't an instantiation -- we can't access those directly,
			// we always have to go through our Codegen::instantiateGenericFunctionUsingParameters() function
			// that one will return an already-generated version if the types match up.

			if(!fn->isGenericInstantiation())
				cands.push_back(fn);
		}

		// if it's a generic function, it won't be in the module
		// check the scope.
		for(auto f : cgi->getCurrentFuncTree()->genericFunctions)
		{
			if(f.first->ident.name == this->name)
			{
				if(!f.first->generatedFunc)
					f.first->codegen(cgi);

				iceAssert(f.first->generatedFunc);
				cands.push_back(f.first->generatedFunc);
			}
		}


		fir::Function* fn = cgi->tryDisambiguateFunctionVariableUsingType(this, this->name, cands, extra);
		if(fn == 0)
		{
			GenError::unknownSymbol(cgi, this, this->name, SymbolType::Variable);
		}

		return Result_t(fn, 0);
	}

	return Result_t(cgi->builder.CreateLoad(val), val);
}

fir::Type* VarRef::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	VarDecl* decl = cgi->getSymDecl(this, this->name);
	if(!decl)
	{
		// check for functions

		auto fns = cgi->module->getFunctionsWithName(Identifier(this->name, IdKind::Function));
		std::deque<fir::Function*> cands;

		for(auto fn : fns)
		{
			if(!fn->isGenericInstantiation())
				cands.push_back(fn);
		}

		// if it's a generic function, it won't be in the module
		// check the scope.
		for(auto f : cgi->getCurrentFuncTree()->genericFunctions)
		{
			if(f.first->ident.name == this->name)
			{
				if(!f.first->generatedFunc)
					f.first->codegen(cgi);

				iceAssert(f.first->generatedFunc);
				cands.push_back(f.first->generatedFunc);
			}
		}

		fir::Function* fn = cgi->tryDisambiguateFunctionVariableUsingType(this, this->name, cands, extra);
		if(fn == 0)
			GenError::unknownSymbol(cgi, this, this->name, SymbolType::Variable);

		return fn->getType();
	}

	return decl->getType(cgi, allowFail);
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


	iceAssert(this->concretisedType);
	if(val != 0)
	{
		// cast.
		val = cgi->autoCastType(this->concretisedType, val, valptr);
	}





	if(this->initVal && !cmplxtype && !cgi->isAnyType(val->getType()) && !val->getType()->isArrayType())
	{
		// ...
		// handled below
	}
	else if(!this->initVal && (cgi->isBuiltinType(this) || cgi->isArrayType(this)
		|| this->getType(cgi)->isTupleType() || this->getType(cgi)->isPointerType()))
	{
		val = cgi->getDefaultValue(this);
		iceAssert(val);
	}
	else
	{
		iceAssert(ai);
		iceAssert(this->concretisedType);
		cmplxtype = cgi->getType(this->concretisedType);

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
			cgi->addSymbol(this->ident.name, ai, this);
			didAddToSymtab = true;
		}


		if(this->initVal && (!cmplxtype || dynamic_cast<StructBase*>(cmplxtype->second.first)->ident.name == "Any"
										|| cgi->isAnyType(val->getType())))
		{
			// this only works if we don't call a constructor

			// todo: make this work better, maybe as a parameter to doBinOpAssign
			// it currently doesn't know (and maybe shouldn't) if we're doing first assign or not
			// if we're an immutable var (ie. val or let), we first set immutable to false so we
			// can store the value, then set it back to whatever it was.

			bool wasImmut = this->immutable;
			this->immutable = false;

			auto vr = new VarRef(this->pin, this->ident.name);
			auto res = Operators::performActualAssignment(cgi, this, vr, this->initVal, ArithmeticOp::Assign, cgi->builder.CreateLoad(ai),
				ai, val, valptr);

			delete vr;

			this->immutable = wasImmut;
			return res.result.first;
		}
		else if(!cmplxtype && !this->initVal)
		{
			if(ai->getType()->getPointerElementType()->isFunctionType())
			{
				// stuff
				// just return 0

				auto n = fir::ConstantValue::getNullValue(ai->getType()->getPointerElementType());
				cgi->builder.CreateStore(n, ai);
				return n;
			}
			else
			{
				iceAssert(val);
				cgi->builder.CreateStore(val, ai);
				return val;
			}
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



	// check if we're a generic function
	if(val->getType()->isFunctionType() && val->getType()->toFunctionType()->isGenericFunction())
	{
		// if we are, we need concrete types to be able to reify the function
		// we cannot have a variable hold a parametric function in the raw form, since calling it
		// later will be very troublesome (different return types, etc.)

		iceAssert(this->concretisedType && this->concretisedType->isFunctionType());
		if(this->concretisedType->toFunctionType()->isGenericFunction())
		{
			error(this, "Unable to infer the instantiation of parametric function (type '%s'); explicit type specifier must be given",
				this->concretisedType->str().c_str());
		}
		else
		{
			// concretised function is *not* generic.
			// hooray.


			fir::Function* oldf = dynamic_cast<fir::Function*>(val);
			iceAssert(oldf);

			fir::Function* fn = 0;

			if(MemberAccess* ma = dynamic_cast<MemberAccess*>(this->initVal))
			{
				auto fp = cgi->resolveAndInstantiateGenericFunctionReference(this, oldf, this->concretisedType->toFunctionType(), ma);
				fn = fp;
			}
			else
			{
				auto fp = cgi->tryResolveGenericFunctionFromCandidatesUsingFunctionType(this,
					cgi->findGenericFunctions(oldf->getName().name), this->concretisedType->toFunctionType());

				fn = fp.firFunc;
			}


			if(fn != 0)
			{
				// rewrite history
				val = fn;
			}
			else
			{
				error(this, "Invalid instantiation of parametric function of type '%s' with type '%s'", oldf->getType()->str().c_str(),
					this->concretisedType->str().c_str());
			}
		}
	}

	if(!didAddToSymtab && shouldAddToSymtab)
		cgi->addSymbol(this->ident.name, ai, this);

	if(val->getType() != ai->getType()->getPointerElementType())
		GenError::invalidAssignment(cgi, this, ai->getType()->getPointerElementType(), val->getType());

	cgi->builder.CreateStore(val, ai);
	return val;
}





void VarDecl::inferType(CodegenInstance* cgi)
{
	if(this->ptype == pts::InferredType::get())
	{
		if(this->initVal == nullptr)
			error(this, "Type inference requires an initial assignment to infer type");


		fir::Type* vartype = this->initVal->getType(cgi);
		if(vartype == nullptr || vartype->isVoidType())
			GenError::nullValue(cgi, this->initVal);


		if(cgi->isAnyType(vartype))
		{
			// todo: fix this shit
			// but how?
			warn(this, "Assigning a value of type 'Any' using type inference will not unwrap the value");
		}

		this->concretisedType = vartype;
	}
	else
	{
		this->concretisedType = cgi->getTypeFromParserType(this, this->ptype);
		if(!this->concretisedType) error(this, "invalid type %s", this->ptype->str().c_str());

		iceAssert(this->concretisedType);
	}
}






Result_t VarDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(cgi->isDuplicateSymbol(this->ident.name))
		GenError::duplicateSymbol(cgi, this, this->ident.name, SymbolType::Variable);

	this->ident.scope = cgi->getFullScope();

	fir::Value* val = nullptr;
	fir::Value* valptr = nullptr;

	fir::Value* ai = nullptr;

	this->inferType(cgi);

	if(!this->isGlobal)
	{
		ai = cgi->getStackAlloc(this->concretisedType, this->ident.name);
		iceAssert(ai->getType()->getPointerElementType() == this->concretisedType);
	}


	if(this->initVal)
	{
		ValPtr_t r;

		if(isGlobal && (this->concretisedType->isStructType() || this->concretisedType->isClassType()))
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



	// TODO: call global constructors
	if(this->isGlobal)
	{
		ai = cgi->module->createGlobalVariable(this->ident, this->concretisedType, fir::ConstantValue::getNullValue(this->concretisedType),
			this->immutable, (this->attribs & Attr_VisPublic) ? fir::LinkageType::External : fir::LinkageType::Internal);

		fir::Type* ltype = ai->getType()->getPointerElementType();

		if(this->initVal)
		{
			iceAssert(val);
			cgi->addGlobalConstructedValue(ai, val);
		}
		else if(ltype->isStructType() || ltype->isClassType())
		{
			// oopsies. we got to call the struct constructor.
			TypePair_t* tp = cgi->getType(ltype);
			iceAssert(tp);

			StructBase* sb = dynamic_cast<StructBase*>(tp->second.first);
			iceAssert(sb);

			fir::Function* candidate = cgi->getDefaultConstructor(this, ai->getType(), sb);
			cgi->addGlobalConstructor(ai, candidate);
		}
		else if(ltype->isTupleType())
		{
			fir::TupleType* stype = dynamic_cast<fir::TupleType*>(ltype);

			int i = 0;
			for(fir::Type* t : stype->getElements())
			{
				if(t->isTupleType())
				{
					// todo(missing): why?
					error(this, "global nested tuples not supported yet");
				}
				else if(t->isStructType() || t->isClassType())
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

		FunctionTree* ft = cgi->getCurrentFuncTree();
		iceAssert(ft);

		ft->vars[this->ident.name] = { ai, this };
	}
	else
	{
		TypePair_t* cmplxtype = 0;
		if(this->ptype != pts::InferredType::get())
		{
			iceAssert(this->concretisedType);
			cmplxtype = cgi->getType(this->concretisedType);
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

fir::Type* VarDecl::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->ptype == pts::InferredType::get())
	{
		if(!this->concretisedType)		// todo: better error detection for this
		{
			error(this, "Invalid variable declaration for %s!", this->ident.name.c_str());
		}

		iceAssert(this->concretisedType);
		return this->concretisedType;
	}
	else
	{
		// if we already "inferred" the type, don't bother doing it again.
		if(this->concretisedType)
			return this->concretisedType;

		return cgi->getTypeFromParserType(this, this->ptype, allowFail);
	}
}













