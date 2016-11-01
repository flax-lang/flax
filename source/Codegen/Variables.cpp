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
		// check the global scope for variables
		auto ft = cgi->getCurrentFuncTree();
		for(auto v : ft->vars)
		{
			if(v.first == this->name)
				return Result_t(cgi->irb.CreateLoad(v.second.first), v.second.first, ValueKind::LValue);
		}



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

	return Result_t(cgi->irb.CreateLoad(val), val, ValueKind::LValue);
}

fir::Type* VarRef::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	VarDecl* decl = cgi->getSymDecl(this, this->name);
	if(!decl)
	{
		// check the global scope for variables
		auto ft = cgi->getCurrentFuncTree();
		for(auto v : ft->vars)
		{
			if(v.first == this->name)
				return v.second.first->getType()->getPointerElementType();
		}






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









fir::Value* VarDecl::doInitialValue(CodegenInstance* cgi, TypePair_t* cmplxtype, fir::Value* val, fir::Value* valptr,
	fir::Value* storage, bool shouldAddToSymtab, ValueKind vk)
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
	else if(!this->initVal && (cgi->isBuiltinType(this) || cgi->isArrayType(this) || this->getType(cgi)->isDynamicArrayType()
		|| this->getType(cgi)->isTupleType() || this->getType(cgi)->isPointerType() || this->getType(cgi)->isCharType()))
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
				if(!ai->getType()->getPointerElementType()->isEnumType())
				{
					std::vector<fir::Value*> args { ai };

					fir::Function* initfunc = cgi->getStructInitialiser(this, cmplxtype, args);
					iceAssert(initfunc);

					val = cgi->irb.CreateCall(initfunc, args);
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
			auto res = Operators::performActualAssignment(cgi, this, vr, this->initVal, ArithmeticOp::Assign, cgi->irb.CreateLoad(ai),
				ai, val, valptr, vk);

			// delete vr;

			this->immutable = wasImmut;
			return res.value;
		}
		else if(!cmplxtype && !this->initVal)
		{
			if(ai->getType()->getPointerElementType()->isFunctionType())
			{
				// stuff
				// just return 0

				auto n = fir::ConstantValue::getNullValue(ai->getType()->getPointerElementType());
				cgi->irb.CreateStore(n, ai);
				return n;
			}
			else
			{
				iceAssert(val);
				cgi->irb.CreateStore(val, ai);
				return val;
			}
		}
		else if(cmplxtype && this->initVal)
		{
			if(ai->getType()->getPointerElementType() != val->getType())
			{
				// info(this, "%s <%d> %s", ai->getType()->getPointerElementType()->str().c_str(),
					// cgi->getAutoCastDistance(val->getType(), this->concretisedType), val->getType()->str().c_str());

				GenError::invalidAssignment(cgi, this, ai->getType()->getPointerElementType(), val->getType());
			}

			cgi->irb.CreateStore(val, ai);
			return val;
		}
		else
		{
			if(valptr)
				val = cgi->irb.CreateLoad(valptr);

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
	{
		// info(this, "%s <%d> %s", val->getType()->str().c_str(),
			// cgi->getAutoCastDistance(val->getType(), this->concretisedType), this->concretisedType->str().c_str());

		GenError::invalidAssignment(cgi, this, ai->getType()->getPointerElementType(), val->getType());
	}



	// strings 'n' stuff
	if(cgi->isRefCountedType(val->getType()))
	{
		// if the right side was a string literal, everything is already done
		// (as an optimisation, the string literal is directly stored into the var)

		if(this->initVal && (!dynamic_cast<StringLiteral*>(this->initVal) || dynamic_cast<StringLiteral*>(this->initVal)->isRaw))
		{
			// we need to store something there first, to initialise the refcount and stuff before we try to decrement it
			cgi->assignRefCountedExpression(this, val, valptr, ai, vk, true);
		}
		else if(!this->initVal)
		{
			// val was the default value
			cgi->irb.CreateStore(val, ai);
		}

		if(shouldAddToSymtab)
		{
			cgi->addRefCountedValue(ai);
		}
	}
	else
	{
		bool wasimmut = ai->isImmutable();

		ai->makeNotImmutable();
		cgi->irb.CreateStore(val, ai);

		if(wasimmut) ai->makeImmutable();
	}






	return cgi->irb.CreateLoad(ai);
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

		if(vartype->isPrimitiveType() && vartype->toPrimitiveType()->isLiteralType())
		{
			// make it the largest, by default
			if(vartype->isIntegerType() && vartype->isSignedIntType())
				vartype = fir::Type::getInt64();

			else if(vartype->isIntegerType())
				vartype = fir::Type::getUint64();

			else
				vartype = fir::Type::getFloat64();
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

	this->inferType(cgi);



	// TODO: call global constructors
	if(this->isGlobal)
	{
		fir::GlobalVariable* glob = cgi->module->createGlobalVariable(this->ident, this->concretisedType,
			fir::ConstantValue::getNullValue(this->concretisedType), this->immutable,
			(this->attribs & Attr_VisPublic) ? fir::LinkageType::External : fir::LinkageType::Internal);

		fir::Type* ltype = glob->getType()->getPointerElementType();

		if(this->initVal)
		{
			auto prev = cgi->irb.getCurrentBlock();


			fir::Function* constr = cgi->procureAnonymousConstructorFunction(glob);

			// set it up so we go straight to writing instructions.
			cgi->irb.setCurrentBlock(constr->getBlockList().front());


			fir::Value* rval = 0;
			fir::Value* rptr = 0;
			ResultType rtype;
			ValueKind rkind;

			std::tie(rval, rptr, rtype, rkind) = this->initVal->codegen(cgi, glob);

			// don't be wasting time calling functions if we're constant.
			if(dynamic_cast<fir::ConstantValue*>(rval))
			{
				// go back to prev
				cgi->irb.setCurrentBlock(prev);

				// delete the function
				cgi->module->removeFunction(constr);

				// ok, now. we can call codegen again, since it's constant. no repercussions
				// we need to call it again, because the old value was created in a deleted function.

				fir::Value* val = this->initVal->codegen(cgi).value;
				if(val->getType() != glob->getType()->getPointerElementType())
				{
					auto vt = val->getType();
					auto gt = glob->getType()->getPointerElementType();

					fir::ConstantInt* ci = dynamic_cast<fir::ConstantInt*>(val);
					iceAssert(ci);

					// ok, decide what kind of cast we want
					if(vt->isIntegerType() && gt->isIntegerType())
					{
						// int to int -- either signedness or size
						// check if we can do a lossless cast
						int d = cgi->getAutoCastDistance(vt, gt);

						if(d == -1)
						{
							// we can't.
							// check if the value is negative, and we're unsigned
							if(vt->isSignedIntType() && !gt->isSignedIntType())
							{
								if(ci->getSignedValue() < 0)
								{
									error(this, "Cannot store negative literal value '%zd' into unsigned type '%s'", ci->getSignedValue(),
										gt->str().c_str());
								}
							}

							// check the sizing
							bool res = false;
							if(vt->isSignedIntType())
								res = fir::checkSignedIntLiteralFitsIntoType(gt->toPrimitiveType(), ci->getSignedValue());

							else
								res = fir::checkUnsignedIntLiteralFitsIntoType(gt->toPrimitiveType(), ci->getUnsignedValue());

							if(res)
							{
								val = fir::ConstantInt::get(gt, ci->getSignedValue());
							}
							else
							{
								error(this, "Cannot cast '%s' to '%s' without loss", vt->str().c_str(), gt->str().c_str());
							}
						}
						else
						{
							// ok, we can.
							// do the cast.
							if(vt->isSignedIntType() != gt->isSignedIntType())
							{
								// change sign first
								val = cgi->irb.CreateIntSignednessCast(val, vt->toPrimitiveType()->getOppositeSignedType());
							}

							// change size
							val = cgi->irb.CreateIntSizeCast(val, gt);
						}
					}
					else if(vt->isIntegerType() && gt->isFloatingPointType())
					{
						// int to float
						iceAssert(0);
					}
					else if(vt->isFloatingPointType() && gt->isIntegerType())
					{
						// float to int
						iceAssert(0);
					}
					else
					{
						error(this, "Unable to cast value of type '%s' to store in global variable of type '%s'", vt->str().c_str(),
							gt->str().c_str());
					}
				}

				fir::ConstantValue* cv = dynamic_cast<fir::ConstantValue*>(val);
				iceAssert(cv);

				glob->setInitialValue(cv);
			}
			else
			{
				TypePair_t* cmplxtype = 0;
				if(this->ptype != pts::InferredType::get())
				{
					iceAssert(this->concretisedType);
					cmplxtype = cgi->getType(this->concretisedType);
				}


				// add it.
				cgi->addGlobalConstructor(glob, constr);

				this->doInitialValue(cgi, cmplxtype, rval, rptr, glob, false, rkind);
				cgi->irb.CreateReturnVoid();

				cgi->irb.setCurrentBlock(prev);
			}
		}
		else if(ltype->isStringType())
		{
			// fk
			cgi->addGlobalConstructedValue(glob, fir::ConstantValue::getNullValue(this->concretisedType));
		}
		else if(ltype->isStructType() || ltype->isClassType())
		{
			// oopsies. we got to call the struct constructor.
			TypePair_t* tp = cgi->getType(ltype);
			iceAssert(tp);

			StructBase* sb = dynamic_cast<StructBase*>(tp->second.first);
			iceAssert(sb);

			fir::Function* candidate = cgi->getDefaultConstructor(this, glob->getType(), sb);
			cgi->addGlobalConstructor(glob, candidate);
		}
		else if(ltype->isTupleType())
		{
			std::function<void(CodegenInstance*, fir::TupleType*, fir::Value*, VarDecl*)> handleTuple
				= [&handleTuple](CodegenInstance* cgi, fir::TupleType* tt, fir::Value* ai, VarDecl* user) -> void
			{
				size_t i = 0;
				for(fir::Type* t : tt->getElements())
				{
					if(t->isTupleType())
					{
						fir::Value* p = cgi->irb.CreateStructGEP(ai, i);
						handleTuple(cgi, t->toTupleType(), p, user);
					}
					else if(t->isStructType() || t->isClassType())
					{
						TypePair_t* tp = cgi->getType(t);
						iceAssert(tp);

						fir::Function* structConstr = cgi->getDefaultConstructor(user, t->getPointerTo(),
							dynamic_cast<StructBase*>(tp->second.first));

						fir::Value* p = cgi->irb.CreateStructGEP(ai, i);
						cgi->irb.CreateCall1(structConstr, p);
					}
					else
					{
						fir::Value* p = cgi->irb.CreateStructGEP(ai, i);
						cgi->irb.CreateStore(fir::ConstantValue::getNullValue(t), p);
					}

					i++;
				}
			};

			auto prev = cgi->irb.getCurrentBlock();

			fir::Function* constr = cgi->procureAnonymousConstructorFunction(glob);
			cgi->addGlobalConstructor(glob, constr);

			// set it up so the lambda goes straight to writing instructions.
			cgi->irb.setCurrentBlock(constr->getBlockList().front());
			handleTuple(cgi, ltype->toTupleType(), glob, this);
			cgi->irb.CreateReturnVoid();

			cgi->irb.setCurrentBlock(prev);
		}

		FunctionTree* ft = cgi->getCurrentFuncTree();
		iceAssert(ft);

		ft->vars[this->ident.name] = { glob, this };

		return Result_t(0, glob);
	}
	else
	{
		fir::Value* val = nullptr;
		fir::Value* valptr = nullptr;

		fir::Value* ai = nullptr;
		ValueKind vk = ValueKind::RValue;


		ai = cgi->getStackAlloc(this->concretisedType, this->ident.name);
		iceAssert(ai->getType()->getPointerElementType() == this->concretisedType);


		if(this->initVal)
		{
			std::tie(val, valptr, vk) = this->initVal->codegen(cgi, ai);
		}


		TypePair_t* cmplxtype = 0;
		if(this->ptype != pts::InferredType::get())
		{
			iceAssert(this->concretisedType);
			cmplxtype = cgi->getType(this->concretisedType);
		}

		this->doInitialValue(cgi, cmplxtype, val, valptr, ai, true, vk);

		if(this->immutable)
			ai->makeImmutable();

		return Result_t(cgi->irb.CreateLoad(ai), ai);
	}
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













