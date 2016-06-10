// ArrayCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


static Result_t handleSubscriptOperatorOverload(CodegenInstance* cgi, Expr* e, Expr* index, fir::Value* rhs)
{
	#if 0
	fir::Type* lhsType = cgi->getExprType(e);
	iceAssert(lhsType->isStructType());

	TypePair_t* tp = cgi->getType(lhsType);
	if(!tp) iceAssert(0 && "what?");

	iceAssert(tp->second.second == TypeKind::Class);
	Class* cls = dynamic_cast<Class*>(tp->second.first);


	// todo: do we want to handle top-level subscript overloads?
	std::deque<SubscriptOpOverload*> candidates;
	for(auto opo : cls->opOverloads)
	{
		if(opo->op == ArithmeticOp::Subscript)
			candidates.push_back(dynamic_cast<SubscriptOpOverload*>(opo));
	}


	if(candidates.size() == 0)
		error(e, "No subscript operator overloads for type %s", cgi->getReadableType(e).c_str());


	fir::Value* selfPtr = e->codegen(cgi).result.second;
	iceAssert(selfPtr);

	fir::Value* indexVal = index->codegen(cgi).result.first;
	iceAssert(indexVal);


	std::deque<std::tuple<int, fir::Function*, fir::Function*>> c2s;
	for(auto cand : candidates)
	{
		// todo: HANDLE MULTIPLE SUBSCRIPTS
		// check getter
		std::tuple<int, fir::Function*, fir::Function*> final;
		{
			fir::Function* gf = cgi->module->getFunction(cand->cprop->getterFunc->mangledName);
			iceAssert(gf);	// should always have getter


			// should match...
			iceAssert(gf->getArguments()[0]->getType() == selfPtr->getType());


			// check second argument
			// note: HANDLE MULTIPLE FUCKING SUBSCRIPTS
			if(gf->getArguments()[1]->getType() == indexVal->getType())
			{
				std::get<0>(final) = 0;
				std::get<1>(final) = gf;
			}
			else
			{
				if(int d = cgi->getAutoCastDistance(indexVal->getType(), gf->getArguments()[1]->getType()) >= 0)
				{
					std::get<0>(final) = d;
					std::get<1>(final) = gf;
				}
			}
		}


		// check setter
		// but don't bother if we have no getter
		if(std::get<1>(final))
		{
			fir::Function* sf = cgi->module->getFunction(cand->cprop->setterFunc->mangledName);
			if(sf)
			{
				// should match...
				iceAssert(sf->getArguments()[0]->getType() == selfPtr->getType());


				// check second argument
				// note: HANDLE MULTIPLE FUCKING SUBSCRIPTS
				if(sf->getArguments()[1]->getType() == indexVal->getType())
				{
					std::get<2>(final) = sf;
				}
				else
				{
					if(cgi->getAutoCastDistance(indexVal->getType(), sf->getArguments()[1]->getType()) >= 0)
					{
						std::get<2>(final) = sf;
					}
				}
			}
		}

		if(std::get<1>(final))
			c2s.push_back(final);
	}



	int best = 10000000;
	std::pair<fir::Function*, fir::Function*> fin;
	for(auto c2 : c2s)
	{
		if(std::get<0>(c2) < best)
		{
			fin = { std::get<1>(c2), std::get<2>(c2) };
			best = std::get<0>(c2);
		}
	}

	if(rhs)
	{
		// call setter
		fir::Function* set = std::get<1>(fin);
		cgi->builder.CreateCall(set, { selfPtr, indexVal, rhs });

		return Result_t(0, 0);
	}
	else
	{
		fir::Function* get = std::get<0>(fin);
		fir::Value* ret = cgi->builder.CreateCall(get, { selfPtr, indexVal });

		return Result_t(ret, 0);
	}
	#endif

	return Result_t(0, 0);
}















































