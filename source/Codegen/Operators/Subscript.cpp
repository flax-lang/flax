// Subscript.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	static std::pair<ClassDef*, fir::Type*> getClassDef(CodegenInstance* cgi, Expr* user, Expr* subscriptee)
	{
		fir::Type* subscripteeType = cgi->getExprType(subscriptee);

		TypePair_t* tp = cgi->getType(subscripteeType);
		if(!tp || !dynamic_cast<ClassDef*>(tp->second.first))
			error(user, "Cannot subscript on type %s", subscripteeType->str().c_str());

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);

		if(cls->subscriptOverloads.size() == 0)
			error(user, "Class %s has no subscript operators defined, cannot subscript.", subscripteeType->str().c_str());

		return { cls, subscripteeType };
	}


	Result_t operatorAssignToOverloadedSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, Expr* lhs, fir::Value* rhs, Expr* rhsExpr)
	{
		ArrayIndex* ari = dynamic_cast<ArrayIndex*>(lhs);
		iceAssert(ari);

		auto p = getClassDef(cgi, user, ari->arr);
		ClassDef* cls = p.first;
		fir::Type* ftype = p.second;

		std::deque<FuncPair_t> cands;

		for(auto soo : cls->subscriptOverloads)
			cands.push_back({ soo->setterFunc, soo->decl });

		std::string basename = cls->subscriptOverloads[0]->decl->ident.name;

		// todo: MULIPLE SUBSCRIPTS
		std::deque<Expr*> params = { ari->index };
		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, params, false);



		if(!res.resolved)
		{
			auto tup = GenError::getPrettyNoSuchFunctionError(cgi, params, cands);
			std::string argstr = std::get<0>(tup);
			std::string candstr = std::get<1>(tup);
			HighlightOptions ops = std::get<2>(tup);

			error(user, ops, "Class %s has no subscript operator taking parameters (%s)\nPossible candidates (%zu):\n%s",
				ftype->str().c_str(), argstr.c_str(), cands.size(), candstr.c_str());
		}
		else
		{
			if(res.t.first == 0)
			{
				error(user, "Class %s does not have a subscript operator with a setter", ftype->str().c_str());
			}


			std::deque<fir::Value*> fargs;

			// gen the self (note: uses the ArrayIndex AST)
			fir::Value* lhsPtr = ari->arr->codegen(cgi).result.second;
			iceAssert(lhsPtr);

			if(lhsPtr->isImmutable())
				GenError::assignToImmutable(cgi, user, rhsExpr);

			fargs.push_back(lhsPtr);

			fir::Function* fn = cgi->module->getFunction(res.t.first->getName());
			iceAssert(fn);

			// gen args.
			// -2 to exclude the first param, and the rhs param.
			for(size_t i = 0; i < fn->getArgumentCount() - 2; i++)
			{
				fir::Value* arg = params[i]->codegen(cgi).result.first;

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



	// note: only used by getExprType(), we don't use it ourselves here.
	fir::Function* getOperatorSubscriptGetter(Codegen::CodegenInstance* cgi, Expr* user, fir::Type* fcls, std::deque<Ast::Expr*> args)
	{
		iceAssert(args.size() >= 1);

		TypePair_t* tp = cgi->getType(fcls);
		if(!tp) { return 0; }

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		if(!cls) { return 0; }


		std::deque<FuncPair_t> cands;

		for(auto soo : cls->subscriptOverloads)
			cands.push_back({ soo->getterFunc, soo->decl });

		std::string basename = cls->subscriptOverloads[0]->decl->ident.name;

		std::deque<Expr*> params = std::deque<Expr*>(args.begin() + 1, args.end());
		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, params, false);

		if(res.resolved) return res.t.first;
		else return 0;
	}









	Result_t operatorOverloadedSubscript(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		iceAssert(args.size() >= 2);

		auto p = getClassDef(cgi, user, args[0]);
		ClassDef* cls = p.first;
		fir::Type* ftype = p.second;

		std::deque<FuncPair_t> cands;

		for(auto soo : cls->subscriptOverloads)
			cands.push_back({ soo->getterFunc, soo->decl });

		std::string basename = cls->subscriptOverloads[0]->decl->ident.name;

		std::deque<Expr*> params = std::deque<Expr*>(args.begin() + 1, args.end());
		Resolved_t res = cgi->resolveFunctionFromList(user, cands, basename, params, false);

		if(!res.resolved)
		{
			auto tup = GenError::getPrettyNoSuchFunctionError(cgi, params, cands);
			std::string argstr = std::get<0>(tup);
			std::string candstr = std::get<1>(tup);
			HighlightOptions ops = std::get<2>(tup);

			error(user, ops, "Class %s has no subscript operator taking parameters (%s)\nPossible candidates (%zu):\n%s",
				ftype->str().c_str(), argstr.c_str(), cands.size(), candstr.c_str());
		}
		else
		{
			std::deque<fir::Value*> fargs;

			// gen the self.
			fir::Value* lhsPtr = args[0]->codegen(cgi).result.second;
			iceAssert(lhsPtr);

			fargs.push_back(lhsPtr);

			// gen args.
			fir::Function* fn = cgi->module->getFunction(res.t.first->getName());
			iceAssert(fn);

			for(size_t i = 0; i < fn->getArgumentCount() - 1; i++)
			{
				fir::Value* arg = params[i]->codegen(cgi).result.first;

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
		fir::Type* atype = cgi->getExprType(subscriptee);

		if(!atype->isArrayType() && !atype->isPointerType() && !atype->isLLVariableArrayType())
		{
			if(atype->isStructType())
				return operatorOverloadedSubscript(cgi, op, user, args);

			error(user, "Can only index on pointer or array types, got %s", atype->str().c_str());
		}


		Result_t lhsp = subscriptee->codegen(cgi);

		fir::Value* lhs = 0;
		if(lhsp.result.first->getType()->isPointerType())	lhs = lhsp.result.first;
		else												lhs = lhsp.result.second;


		iceAssert(lhs);

		fir::Value* gep = nullptr;
		fir::Value* ind = subscriptIndex->codegen(cgi).result.first;

		if(atype->isStructType() || atype->isArrayType())
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















