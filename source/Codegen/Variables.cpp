// VarCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "codegen.h"
#include "operators.h"

#include "mpark/variant.hpp"

using namespace Ast;
using namespace Codegen;


mpark::variant<fir::Type*, Result_t> CodegenInstance::tryResolveVarRef(VarRef* vr, fir::Type* extratype, bool actual)
{
	if(vr->name == "_")
		error(vr, "'_' is a discarding reference, and cannot be used to refer to values.");

	if(actual)
	{
		if(fir::Value* val = this->getSymInst(vr, vr->name))
			return Result_t(this->irb.CreateLoad(val), val, ValueKind::LValue);
	}
	else
	{
		if(VarDecl* decl = this->getSymDecl(vr, vr->name))
			return decl->getType(this);
	}



	// check the global scope for variables
	{
		auto ft = this->getCurrentFuncTree();
		while(ft)
		{
			for(auto v : ft->vars)
			{
				if(v.first == vr->name)
				{
					if(!actual)
						return v.second.first->getType()->getPointerElementType();

					return Result_t(this->irb.CreateLoad(v.second.first), v.second.first, ValueKind::LValue);
				}
			}

			ft = ft->parent;
		}
	}




	// check for functions
	auto fns = this->resolveFunctionName(vr->name);
	std::vector<fir::Function*> cands;

	for(auto fn : fns)
	{
		// check that the function we found wasn't an instantiation -- we can't access those directly,
		// we always have to go through our Codegen::instantiateGenericFunctionUsingParameters() function
		// that one will return an already-generated version if the types match up.

		if(fn.funcDecl && !fn.firFunc)
		{
			fn.firFunc = dynamic_cast<fir::Function*>(fn.funcDecl->codegen(this).value);
			iceAssert(fn.firFunc);
		}

		if(fn.firFunc && !fn.firFunc->isGenericInstantiation())
			cands.push_back(fn.firFunc);
	}




	// if it's a generic function, it won't be in the module; check the scope.
	{
		auto ft = this->getCurrentFuncTree();
		while(ft)
		{
			for(auto f : ft->genericFunctions)
			{
				if(f.first->ident.name == vr->name)
				{
					if(!f.first->generatedFunc)
						f.first->codegen(this);

					iceAssert(f.first->generatedFunc);
					cands.push_back(f.first->generatedFunc);
				}
			}

			ft = ft->parent;
		}
	}


	fir::Function* fn = this->tryDisambiguateFunctionVariableUsingType(vr, vr->name, cands, extratype);

	if(!actual)
	{
		return fn ? fn->getType() : 0;
	}
	else
	{
		return Result_t(fn, 0);
	}
}






Result_t VarRef::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	auto r = mpark::get<Result_t>(cgi->tryResolveVarRef(this, extratype, true));
	if(r.value == 0)
		GenError::unknownSymbol(cgi, this, this->name, SymbolType::Variable);

	return r;
}

fir::Type* VarRef::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	auto t = mpark::get<fir::Type*>(cgi->tryResolveVarRef(this, extratype, false));
	if(t == 0)
		GenError::unknownSymbol(cgi, this, this->name, SymbolType::Variable);

	return t;
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

	// special override: if we're assigning a non-any to an any
	if(this->concretisedType->isAnyType() && !val->getType()->isAnyType())
	{
		// just.. store it, and fuck off.
		if(shouldAddToSymtab)
		{
			cgi->addSymbol(this->ident.name, ai, this);
			didAddToSymtab = true;
		}

		cgi->assignValueToAny(this, ai, val, valptr, vk);
		return cgi->irb.CreateLoad(ai);
	}


	// check if we're a generic function
	if(val && val->getType()->isFunctionType() && val->getType()->toFunctionType()->isGenericFunction())
	{
		// if we are, we need concrete types to be able to reify the function
		// we cannot have a variable hold a parametric function in the raw form, since calling it
		// later will be very troublesome (different return types, etc.)

		fir::Function* oldf = dynamic_cast<fir::Function*>(val);

		// oldf can be null
		fir::Function* fn = cgi->instantiateGenericFunctionUsingValueAndType(this, oldf, val->getType()->toFunctionType(),
			this->concretisedType->toFunctionType(), dynamic_cast<MemberAccess*>(this->initVal));

		iceAssert(fn);

		// rewrite history
		// just store it
		cgi->irb.CreateStore(fn, ai);
	}
	else
	{
		if(this->initVal)
		{
			// simple store.
			Operators::performActualAssignment(cgi, this, this, this->initVal, ArithmeticOp::Assign, cgi->irb.CreateLoad(ai),
				ai, ValueKind::LValue, val, valptr, vk, true);
		}
		else
		{
			if(!cmplxtype)
			{
				// get the default value
				val = cgi->getDefaultValue(this->concretisedType);
				iceAssert(val);

				// store it, and go away
				cgi->irb.CreateStore(val, ai);
			}
			else
			{
				// call init()
				if(!this->disableAutoInit && cmplxtype->second.second != TypeKind::Enum)
				{
					std::vector<fir::Value*> args { ai };

					fir::Function* initfunc = cgi->getStructInitialiser(this, cmplxtype, args);
					iceAssert(initfunc);

					val = cgi->irb.CreateCall(initfunc, args);
				}
			}
		}
	}




	if(shouldAddToSymtab)
	{
		cgi->addSymbol(this->ident.name, ai, this);
		didAddToSymtab = true;

		if(cgi->isRefCountedType(this->concretisedType))
			cgi->addRefCountedValue(ai);
	}


	if(this->immutable)
		ai->makeImmutable();


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


		if(vartype->isAnyType())
		{
			// todo: fix this shit
			// but how?
			warn(this, "Assigning a value of type 'any' using type inference will not unwrap the value");
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






Result_t VarDecl::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	if(cgi->isDuplicateSymbol(this->ident.name))
		GenError::duplicateSymbol(cgi, this, this->ident.name, SymbolType::Variable);

	this->ident.scope = cgi->getFullScope();

	this->inferType(cgi);



	// TODO: call global constructors
	if(this->isGlobal)
	{
		// check if we already have a global with this name
		if(auto _gv = cgi->module->tryGetGlobalVariable(this->ident))
		{
			error(this, "Global '%s' already exists (with type '%s'), cannot redeclare",
				this->ident.str().c_str(), _gv->getType()->str().c_str());
		}

		fir::GlobalVariable* glob = cgi->module->createGlobalVariable(this->ident, this->concretisedType,
			fir::ConstantValue::getZeroValue(this->concretisedType), false,
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

			std::tie(rval, rptr, rtype, rkind) = this->initVal->codegen(cgi, ltype, glob);

			// don't be wasting time calling functions if we're constant.
			if(dynamic_cast<fir::ConstantValue*>(rval))
			{
				// go back to prev
				cgi->irb.setCurrentBlock(prev);

				// delete the function
				cgi->module->removeFunction(constr);

				// ok, now. we can call codegen again, since it's constant. no repercussions
				// we need to call it again, because the old value was created in a deleted function.

				fir::Value* val = this->initVal->codegen(cgi, glob).value;
				if(val->getType() != glob->getType()->getPointerElementType())
				{
					auto vt = val->getType();
					auto gt = glob->getType()->getPointerElementType();


					// ok, decide what kind of cast we want
					if(vt->isIntegerType() && gt->isIntegerType())
					{
						fir::ConstantInt* ci = dynamic_cast<fir::ConstantInt*>(val);
						iceAssert(ci);

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
			cgi->addGlobalConstructedValue(glob, fir::ConstantValue::getZeroValue(this->concretisedType));
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
						cgi->irb.CreateStore(fir::ConstantValue::getZeroValue(t), p);
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

		if(this->immutable)
			glob->makeImmutable();

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
			std::tie(val, valptr, vk) = this->initVal->codegen(cgi, this->concretisedType, ai);
		}


		TypePair_t* cmplxtype = 0;
		if(this->ptype != pts::InferredType::get())
		{
			iceAssert(this->concretisedType);
			cmplxtype = cgi->getType(this->concretisedType);
		}

		this->doInitialValue(cgi, cmplxtype, val, valptr, ai, this->ident.name != "_", vk);

		if(this->immutable)
			ai->makeImmutable();

		return Result_t(cgi->irb.CreateLoad(ai), ai);
	}
}

fir::Type* VarDecl::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
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













