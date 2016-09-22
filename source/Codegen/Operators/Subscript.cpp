// Subscript.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;




Result_t ArrayIndex::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Operators::OperatorMap::get().call(ArithmeticOp::Subscript, cgi, this, { this->arr, this->index });
}

fir::Type* ArrayIndex::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	fir::Type* t = this->arr->getType(cgi);
	if(!t->isArrayType() && !t->isPointerType() && !t->isLLVariableArrayType())
	{
		// todo: multiple subscripts
		fir::Function* getter = Operators::getOperatorSubscriptGetter(cgi, this, t, { this, this->index });
		if(!getter)
		{
			error(this, "Invalid subscript on type %s, with index type %s", t->cstr(),
				this->index->getType(cgi)->cstr());
		}

		return getter->getReturnType();
	}
	else
	{
		if(t->isLLVariableArrayType()) return t->toLLVariableArray()->getElementType();
		else if(t->isPointerType()) return t->getPointerElementType();
		else return t->toArrayType()->getElementType();
	}
}






namespace Operators
{
	static std::pair<ClassDef*, fir::Type*> getClassDef(CodegenInstance* cgi, Expr* user, Expr* subscriptee)
	{
		fir::Type* subscripteeType = subscriptee->getType(cgi);

		TypePair_t* tp = cgi->getType(subscripteeType);
		if(!tp || !dynamic_cast<ClassDef*>(tp->second.first))
			error(user, "Cannot subscript on type %s", subscripteeType->cstr());

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		size_t s = cls->subscriptOverloads.size();

		for(auto ext : cgi->getExtensionsForType(cls))
			s += ext->subscriptOverloads.size();

		if(s == 0)
		{
			error(user, "Class %s has no subscript operators defined, cannot subscript.", subscripteeType->cstr());
		}

		return { cls, subscripteeType };
	}


	Result_t operatorAssignToOverloadedSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, Expr* lhs, fir::Value* rhs, Expr* rhsExpr)
	{
		ArrayIndex* ari = dynamic_cast<ArrayIndex*>(lhs);
		iceAssert(ari);

		auto p = getClassDef(cgi, user, ari->arr);
		ClassDef* cls = p.first;
		fir::Type* ftype = p.second;

		std::deque<FuncDefPair> cands;

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
		std::deque<fir::Type*> fparams = { ftype->getPointerTo(), ari->index->getType(cgi) };
		std::deque<Expr*> eparams = { ari->index };

		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, fparams, false);



		if(!res.resolved)
		{
			auto tup = GenError::getPrettyNoSuchFunctionError(cgi, { ari->index }, cands);
			std::string argstr = std::get<0>(tup);
			std::string candstr = std::get<1>(tup);
			HighlightOptions ops = std::get<2>(tup);

			error(user, ops, "Class %s has no subscript operator taking parameters (%s)\nPossible candidates (%zu):\n%s",
				ftype->cstr(), argstr.c_str(), cands.size(), candstr.c_str());
		}
		else
		{
			if(res.t.firFunc == 0)
			{
				error(user, "Class %s does not have a subscript operator with a setter", ftype->cstr());
			}


			std::deque<fir::Value*> fargs;

			// gen the self (note: uses the ArrayIndex AST)
			fir::Value* lhsPtr = ari->arr->codegen(cgi).result.second;
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
				fir::Value* arg = eparams[i]->codegen(cgi).result.first;

				// i + 1 to skip the self
				if(fn->getArguments()[i + 1]->getType() != arg->getType())
					arg = cgi->autoCastType(fn->getArguments()[i + 1]->getType(), arg);

				fargs.push_back(arg);
			}


			rhs = cgi->autoCastType(fn->getArguments().back()->getType(), rhs);
			fargs.push_back(rhs);


			cgi->builder.CreateCall(fn, fargs);

			return Result_t(0, 0);
		}
	}



	fir::Function* getOperatorSubscriptGetter(Codegen::CodegenInstance* cgi, Expr* user, fir::Type* fcls, std::deque<Ast::Expr*> args)
	{
		iceAssert(args.size() >= 1);

		TypePair_t* tp = cgi->getType(fcls);
		if(!tp) { return 0; }

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		if(!cls) { return 0; }

		fir::Type* ftype = cls->createdType;
		if(!ftype) cls->createType(cgi);

		iceAssert(ftype);


		std::deque<FuncDefPair> cands;

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


		std::deque<fir::Type*> fparams = { ftype->getPointerTo() };
		for(auto e : std::deque<Expr*>(args.begin() + 1, args.end()))
			fparams.push_back(e->getType(cgi));

		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, fparams, false);

		if(res.resolved) return res.t.firFunc;
		else return 0;
	}









	Result_t operatorOverloadedSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		iceAssert(args.size() >= 2);

		auto p = getClassDef(cgi, user, args[0]);
		ClassDef* cls = p.first;
		fir::Type* ftype = p.second;

		std::deque<FuncDefPair> cands;

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

		std::deque<Expr*> eparams = std::deque<Expr*>(args.begin() + 1, args.end());
		std::deque<fir::Type*> fparams = { ftype->getPointerTo() };
		for(auto e : std::deque<Expr*>(args.begin() + 1, args.end()))
			fparams.push_back(e->getType(cgi));


		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, fparams, false);

		if(!res.resolved)
		{
			auto tup = GenError::getPrettyNoSuchFunctionError(cgi, eparams, cands);
			std::string argstr = std::get<0>(tup);
			std::string candstr = std::get<1>(tup);
			HighlightOptions ops = std::get<2>(tup);

			error(user, ops, "Class %s has no subscript operator taking parameters (%s)\nPossible candidates (%zu):\n%s",
				ftype->cstr(), argstr.c_str(), cands.size(), candstr.c_str());
		}
		else
		{
			std::deque<fir::Value*> fargs;

			// gen the self.
			fir::Value* lhsPtr = args[0]->codegen(cgi).result.second;
			iceAssert(lhsPtr);

			fargs.push_back(lhsPtr);

			// gen args.
			fir::Function* fn = cgi->module->getFunction(res.t.firFunc->getName());
			iceAssert(fn);

			for(size_t i = 0; i < fn->getArgumentCount() - 1; i++)
			{
				fir::Value* arg = eparams[i]->codegen(cgi).result.first;

				// i + 1 to skip the self
				if(fn->getArguments()[i + 1]->getType() != arg->getType())
					arg = cgi->autoCastType(fn->getArguments()[i + 1]->getType(), arg);

				fargs.push_back(arg);
			}

			fir::Value* val = cgi->builder.CreateCall(fn, fargs);
			fir::Value* ret = cgi->builder.CreateImmutStackAlloc(fn->getReturnType(), val);
			return Result_t(val, ret);
		}
	}





	Result_t operatorSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		// arg[0] is the thing being subscripted
		// the rest are the things within the subscript.

		if(args.size() < 2)
			error(user, "Expected at least one expression in the subscript operator (have %zu)", args.size() - 1);

		Expr* subscriptee = args[0];
		Expr* subscriptIndex = args[1];

		// get our array type
		fir::Type* atype = subscriptee->getType(cgi);

		if(!atype->isArrayType() && !atype->isPointerType() && !atype->isLLVariableArrayType())
		{
			if(atype->isStructType() || atype->isClassType())
				return operatorOverloadedSubscript(cgi, op, user, args);

			error(user, "Can only index on pointer or array types, got %s", atype->cstr());
		}


		Result_t lhsp = subscriptee->codegen(cgi);

		fir::Value* lhs = 0;
		if(lhsp.result.first->getType()->isPointerType())	lhs = lhsp.result.first;
		else												lhs = lhsp.result.second;


		iceAssert(lhs);

		fir::Value* gep = nullptr;
		fir::Value* ind = subscriptIndex->codegen(cgi).result.first;

		if(atype->isStructType() || atype->isClassType() || atype->isArrayType())
		{
			gep = cgi->builder.CreateGEP2(lhs, fir::ConstantInt::getUint64(0), ind);
		}
		else if(atype->isLLVariableArrayType())
		{
			fir::Value* dataPtr = cgi->builder.CreateStructGEP(lhs, 0);
			fir::Value* data = cgi->builder.CreateLoad(dataPtr);

			gep = cgi->builder.CreateGetPointer(data, ind);
		}
		else
		{
			gep = cgi->builder.CreateGetPointer(lhs, ind);
		}

		return Result_t(cgi->builder.CreateLoad(gep), gep);
	}
}















