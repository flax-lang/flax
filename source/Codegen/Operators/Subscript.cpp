// Subscript.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"
#include "runtimefuncs.h"

using namespace Ast;
using namespace Codegen;




Result_t ArrayIndex::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	return Operators::OperatorMap::get().call(ArithmeticOp::Subscript, cgi, this, { this->arr, this->index });
}

fir::Type* ArrayIndex::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	fir::Type* t = this->arr->getType(cgi);
	if(t->isDynamicArrayType())
	{
		return t->toDynamicArrayType()->getElementType();
	}
	else if(t->isArraySliceType())
	{
		return t->toArraySliceType()->getElementType();
	}
	else if(t->isPointerType())
	{
		return t->getPointerElementType();
	}
	else if(t->isArrayType())
	{
		return t->toArrayType()->getElementType();
	}
	else if(t->isStringType())
	{
		return fir::Type::getCharType();
	}
	else
	{
		// todo: multiple subscripts
		fir::Function* getter = Operators::getOperatorSubscriptGetter(cgi, this, t, { this, this->index });
		if(!getter)
		{
			error(this, "Invalid subscript on type '%s', with index type '%s'", t->str().c_str(),
				this->index->getType(cgi)->str().c_str());
		}

		return getter->getReturnType();
	}
}








namespace Operators
{
	static std::pair<ClassDef*, fir::Type*> getClassDef(CodegenInstance* cgi, Expr* user, Expr* subscriptee)
	{
		fir::Type* subscripteeType = subscriptee->getType(cgi);

		TypePair_t* tp = cgi->getType(subscripteeType);
		if(!tp || !dynamic_cast<ClassDef*>(tp->second.first))
			error(user, "Cannot subscript on type '%s'", subscripteeType->str().c_str());

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		size_t s = cls->subscriptOverloads.size();

		for(auto ext : cgi->getExtensionsForType(cls))
			s += ext->subscriptOverloads.size();

		if(s == 0)
		{
			error(user, "Class '%s' has no subscript operators defined, cannot subscript.", subscripteeType->str().c_str());
		}

		return { cls, subscripteeType };
	}


	Result_t operatorAssignToOverloadedSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, Expr* lhs, fir::Value* rhs, Expr* rhsExpr)
	{
		ArrayIndex* ari = dynamic_cast<ArrayIndex*>(lhs);
		iceAssert(ari);

		ClassDef* cls = 0; fir::Type* ftype = 0;
		std::tie(cls, ftype) = getClassDef(cgi, user, ari->arr);

		std::vector<FuncDefPair> cands;

		for(auto soo : cls->subscriptOverloads)
			cands.push_back(FuncDefPair(soo->setterFunc, soo->setterFn->decl, soo->setterFn));

		for(auto ext : cgi->getExtensionsForType(cls))
		{
			for(auto f : ext->subscriptOverloads)
				cands.push_back(FuncDefPair(f->setterFunc, f->setterFn->decl, f->setterFn));
		}

		std::string basename;
		if(cands.size() > 0)
			basename = cands.front().funcDecl->ident.name;


		// todo: MULIPLE SUBSCRIPTS
		std::vector<fir::Type*> fparams = { ftype->getPointerTo(), ari->index->getType(cgi) };
		std::vector<Expr*> eparams = { ari->index };

		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, fparams, false);



		if(!res.resolved)
		{
			std::string argstr; std::string candstr; HighlightOptions opts;
			std::tie(argstr, candstr, opts) = GenError::getPrettyNoSuchFunctionError(cgi, { ari->index }, cands);

			error(user, opts, "Class %s has no subscript operator taking parameters (%s)\nPossible candidates (%zu):\n%s",
				ftype->str().c_str(), argstr.c_str(), cands.size(), candstr.c_str());
		}
		else
		{
			if(res.t.firFunc == 0)
			{
				error(user, "Class %s does not have a subscript operator with a setter", ftype->str().c_str());
			}


			std::vector<fir::Value*> fargs;

			// gen the self (note: uses the ArrayIndex AST)
			fir::Value* lhsPtr = ari->arr->codegen(cgi).pointer;
			iceAssert(lhsPtr);

			if(lhsPtr->isImmutable())
				GenError::assignToImmutable(cgi, user, rhsExpr);

			fargs.push_back(lhsPtr);

			fir::Function* fn = cgi->module->getFunction(res.t.firFunc->getName());
			iceAssert(fn);

			// gen args.
			// -2 to exclude the first param, and the rhs param.
			for(size_t i = 0; i < fn->getArgumentCount() - 2; i++)
			{
				fir::Value* arg = eparams[i]->codegen(cgi).value;

				// i + 1 to skip the self
				if(fn->getArguments()[i + 1]->getType() != arg->getType())
					arg = cgi->autoCastType(fn->getArguments()[i + 1]->getType(), arg);

				fargs.push_back(arg);
			}


			rhs = cgi->autoCastType(fn->getArguments().back()->getType(), rhs);
			fargs.push_back(rhs);


			cgi->irb.CreateCall(fn, fargs);

			return Result_t(0, 0);
		}
	}



	fir::Function* getOperatorSubscriptGetter(Codegen::CodegenInstance* cgi, Expr* user, fir::Type* fcls, std::vector<Ast::Expr*> args)
	{
		iceAssert(args.size() >= 1);

		TypePair_t* tp = cgi->getType(fcls);
		if(!tp) { return 0; }

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		if(!cls) { return 0; }

		fir::Type* ftype = cls->createdType;
		if(!ftype) cls->createType(cgi);

		iceAssert(ftype);


		std::vector<FuncDefPair> cands;

		for(auto soo : cls->subscriptOverloads)
			cands.push_back(FuncDefPair(soo->getterFunc, soo->getterFn->decl, soo->getterFn));

		for(auto ext : cgi->getExtensionsForType(cls))
		{
			for(auto f : ext->subscriptOverloads)
				cands.push_back(FuncDefPair(f->getterFunc, f->getterFn->decl, f->getterFn));
		}

		std::string basename;
		if(cands.size() > 0)
			basename = cands.front().funcDecl->ident.name;


		std::vector<fir::Type*> fparams = { ftype->getPointerTo() };
		for(auto e : std::vector<Expr*>(args.begin() + 1, args.end()))
			fparams.push_back(e->getType(cgi));

		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, fparams, false);

		if(res.resolved) return res.t.firFunc;
		else return 0;
	}









	Result_t operatorOverloadedSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		iceAssert(args.size() >= 2);

		ClassDef* cls = 0; fir::Type* ftype = 0;
		std::tie(cls, ftype) = getClassDef(cgi, user, args[0]);

		std::vector<FuncDefPair> cands;

		for(auto soo : cls->subscriptOverloads)
			cands.push_back(FuncDefPair(soo->getterFunc, soo->getterFn->decl, soo->getterFn));

		for(auto ext : cgi->getExtensionsForType(cls))
		{
			for(auto f : ext->subscriptOverloads)
				cands.push_back(FuncDefPair(f->getterFunc, f->getterFn->decl, f->getterFn));
		}


		std::string basename;
		if(cands.size() > 0)
			basename = cands.front().funcDecl->ident.name;

		std::vector<Expr*> eparams = std::vector<Expr*>(args.begin() + 1, args.end());
		std::vector<fir::Type*> fparams = { ftype->getPointerTo() };
		for(auto e : std::vector<Expr*>(args.begin() + 1, args.end()))
			fparams.push_back(e->getType(cgi));


		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, fparams, false);

		if(!res.resolved)
		{
			std::string argstr; std::string candstr; HighlightOptions opts;
			std::tie(argstr, candstr, opts) = GenError::getPrettyNoSuchFunctionError(cgi, eparams, cands);

			error(user, opts, "Class %s has no subscript operator taking parameters (%s)\nPossible candidates (%zu):\n%s",
				ftype->str().c_str(), argstr.c_str(), cands.size(), candstr.c_str());
		}
		else
		{
			std::vector<fir::Value*> fargs;

			// gen the self.
			fir::Value* lhsPtr = args[0]->codegen(cgi).pointer;
			iceAssert(lhsPtr);

			fargs.push_back(lhsPtr);

			// gen args.
			fir::Function* fn = cgi->module->getFunction(res.t.firFunc->getName());
			iceAssert(fn);

			for(size_t i = 0; i < fn->getArgumentCount() - 1; i++)
			{
				fir::Value* arg = eparams[i]->codegen(cgi).value;

				// i + 1 to skip the self
				if(fn->getArguments()[i + 1]->getType() != arg->getType())
					arg = cgi->autoCastType(fn->getArguments()[i + 1]->getType(), arg);

				fargs.push_back(arg);
			}

			fir::Value* val = cgi->irb.CreateCall(fn, fargs);
			fir::Value* ret = cgi->irb.CreateImmutStackAlloc(fn->getReturnType(), val);
			return Result_t(val, ret);
		}
	}





	Result_t operatorSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		// arg[0] is the thing being subscripted
		// the rest are the things within the subscript.

		if(args.size() < 2)
			error(user, "Expected at least one expression in the subscript operator (have %zu)", args.size() - 1);

		Expr* subscriptee = args[0];
		Expr* subscriptIndex = args[1];

		// get our array type
		fir::Type* atype = subscriptee->getType(cgi);

		if(!atype->isArrayType() && !atype->isPointerType() && !atype->isDynamicArrayType() && !atype->isArraySliceType()
			&& !atype->isStringType())
		{
			if(atype->isStructType() || atype->isClassType())
				return operatorOverloadedSubscript(cgi, op, user, args);

			error(user, "Can only index on pointer or array types, got type '%s'", atype->str().c_str());
		}


		Result_t lhsp = subscriptee->codegen(cgi);

		fir::Value* lhs = 0;
		if(lhsp.pointer && lhsp.pointer->getType()->isPointerType())
		{
			lhs = lhsp.value;
		}
		else if(lhsp.pointer)
		{
			lhs = lhsp.pointer;
		}
		else
		{
			lhs = lhsp.value;
		}

		iceAssert(lhs);

		fir::Value* gep = nullptr;
		fir::Value* ind = cgi->irb.CreateIntSizeCast(subscriptIndex->codegen(cgi).value, fir::Type::getInt64());


		if(atype->isArrayType())
		{
			if(!lhsp.pointer) { lhsp.pointer = cgi->irb.CreateImmutStackAlloc(lhsp.value->getType(), lhsp.value); }
			gep = cgi->irb.CreateGEP2(lhsp.pointer, fir::ConstantInt::getUint64(0), ind);
		}
		else if(atype->isDynamicArrayType())
		{
			fir::Function* checkf = RuntimeFuncs::Array::getBoundsCheckFunction(cgi);
			iceAssert(checkf);

			fir::Value* max = cgi->irb.CreateGetDynamicArrayLength(lhsp.pointer);
			cgi->irb.CreateCall3(checkf, max, ind, fir::ConstantString::get(Parser::pinToString(user->pin)));

			fir::Value* data = cgi->irb.CreateGetDynamicArrayData(lhsp.pointer);
			gep = cgi->irb.CreateGetPointer(data, ind);

			if(lhsp.pointer->isImmutable())
				gep->makeImmutable();
		}
		else if(atype->isArraySliceType())
		{
			fir::Function* checkf = RuntimeFuncs::Array::getBoundsCheckFunction(cgi);
			iceAssert(checkf);

			fir::Value* max = cgi->irb.CreateGetArraySliceLength(lhsp.pointer);
			cgi->irb.CreateCall3(checkf, max, ind, fir::ConstantString::get(Parser::pinToString(user->pin)));

			fir::Value* data = cgi->irb.CreateGetArraySliceData(lhsp.pointer);
			gep = cgi->irb.CreateGetPointer(data, ind);

			if(lhsp.pointer->isImmutable())
				gep->makeImmutable();
		}
		else if(atype->isStringType())
		{
			cgi->irb.CreateCall3(RuntimeFuncs::String::getBoundsCheckFunction(cgi), lhsp.value, ind,
				fir::ConstantString::get(Parser::pinToString(user->pin)));

			fir::Value* dp = cgi->irb.CreateGetStringData(lhsp.value);
			gep = cgi->irb.CreateGetPointer(dp, ind);
			gep = cgi->irb.CreatePointerTypeCast(gep, fir::Type::getCharType()->getPointerTo());
		}
		else if(atype->isPointerType())
		{
			gep = cgi->irb.CreateGetPointer(lhs, ind);
		}
		else
		{
			error(user, "???");
		}

		return Result_t(cgi->irb.CreateLoad(gep), gep, ValueKind::LValue);
	}
}















