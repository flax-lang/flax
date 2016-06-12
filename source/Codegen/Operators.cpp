// Operators.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
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


Result_t BinOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	iceAssert(this->left && this->right);
	return Operators::OperatorMap::get().call(this->op, cgi, this, { this->left, this->right });
}


Result_t UnaryOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Operators::OperatorMap::get().call(this->op, cgi, this, { this->expr });
}

Result_t PostfixUnaryOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->kind == Kind::ArrayIndex)
	{
		ArrayIndex* fake = new ArrayIndex(this->pin, this->expr, this->args.front());
		return fake->codegen(cgi, extra);
	}
	else
	{
		error(this, "enotsup");
	}
}


namespace Codegen
{
	_OpOverloadData CodegenInstance::getBinaryOperatorOverload(Expr* us, ArithmeticOp op, fir::Type* lhs, fir::Type* rhs)
	{
		struct Attribs
		{
			ArithmeticOp op;

			bool isBinOp = 0;
			bool isPrefixUnary = 0;	// assumes isBinOp == false
			bool isCommutative = 0; // assumes isBinOp == true

			bool needsBooleanNOT = 0;
			bool needsSwap = 0;
		};


		std::deque<std::pair<Attribs, fir::Function*>> candidates;



		auto findCandidatesPass1 = [](CodegenInstance* cgi, std::deque<std::pair<Attribs, fir::Function*>>* cands,
			std::deque<OpOverload*> list, ArithmeticOp op)
		{
			for(auto oo : list)
			{
				Attribs attr;

				attr.op				= oo->op;
				attr.isBinOp		= oo->kind == OpOverload::OperatorKind::CommBinary || oo->kind == OpOverload::OperatorKind::NonCommBinary;
				attr.isCommutative	= oo->kind == OpOverload::OperatorKind::CommBinary;
				attr.isPrefixUnary	= oo->kind == OpOverload::OperatorKind::PrefixUnary;

				attr.needsSwap		= false;

				fir::Function* lfunc = oo->lfunc;
				if(!lfunc && !oo->didCodegen)
				{
					// kinda a hack
					// if we have operators that use other operators, they all should be codegened first.
					// unless this leads to a stupid loop...
					// todo: edge case detection/fixing?

					oo->codegen(cgi);
					lfunc = oo->lfunc;
				}

				if(!lfunc) continue;


				if(oo->op == op)
				{
					attr.needsBooleanNOT = false;

					(*cands).push_back({ attr, lfunc });
				}
			}
		};




		// get the functree, starting from here, up.
		{
			auto curDepth = this->namespaceStack;

			std::deque<OpOverload*> list;
			for(size_t i = 0; i <= this->namespaceStack.size(); i++)
			{
				FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
				if(!ft) break;

				for(auto f : ft->operators)
				{
					list.push_back(f);
				}

				if(curDepth.size() > 0)
					curDepth.pop_back();
			}

			// if we're assigning things, we need to get the assignfuncs as well.
			if(this->isArithmeticOpAssignment(op))
			{
				TypePair_t* tp = this->getType(lhs);
				iceAssert(tp);

				ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
				if(cls)
				{
					for(auto aso : cls->assignmentOverloads)
					{
						if(aso->op == op)
						{
							Attribs atr;

							atr.op				= op;
							atr.isBinOp			= true;
							atr.isPrefixUnary	= false;
							atr.isCommutative	= false;
							atr.needsSwap		= false;

							iceAssert(aso->lfunc);
							candidates.push_back({ atr, aso->lfunc });
						}
					}
				}
			}


			findCandidatesPass1(this, &candidates, list, op);
		}


		// pass 1.5: prune duplicates
		auto set = candidates;
		candidates.clear();

		for(auto s : set)
		{
			if(std::find_if(candidates.begin(), candidates.end(), [s](std::pair<Attribs, fir::Function*> other) -> bool {
				return other.second->getName() == s.second->getName(); }) == candidates.end())
			{
				candidates.push_back(s);
			}
		}







		// pass 2: prune based on number of parameters. (binop vs normal)
		set = candidates;
		candidates.clear();

		for(auto cand : set)
		{
			if(cand.first.isBinOp && cand.second->getArgumentCount() == 2)
				candidates.push_back(cand);

			else if(!cand.first.isBinOp && cand.second->getArgumentCount() == 1)
				candidates.push_back(cand);
		}



		// pass 3: prune based on operand type
		set = candidates;
		candidates.clear();

		bool hasSelf = (this->isArithmeticOpAssignment(op) || op == ArithmeticOp::Subscript);
		if(hasSelf) lhs = lhs->getPointerTo();

		for(auto cand : set)
		{
			fir::Type* targL = cand.second->getArguments()[0]->getType();
			fir::Type* targR = cand.second->getArguments()[1]->getType();


			// if unary op, only LHS is used.
			if(cand.first.isBinOp)
			{
				if(targL == lhs && targR == rhs)
				{
					candidates.push_back(cand);
				}
				else if(cand.first.isCommutative && targR == lhs && targL == rhs)
				{
					cand.first.needsSwap = true;
					candidates.push_back(cand);
				}
			}
			else
			{
				iceAssert(0 && "unary op overloads not implemented");
			}
		}


		// eliminate more.
		set = candidates;
		candidates.clear();


		// deque [pair [<attr, operator func>, assign func]]
		std::deque<std::pair<std::pair<Attribs, fir::Function*>, fir::Function*>> finals;
		for(std::pair<Attribs, fir::Function*> c : set)
		{
			// see if the appropriate assign exists.
			finals.push_back({ { c.first, c.second }, 0 });
		}

		// final step: disambiguate using the more specific op.
		if(finals.size() > 1)
		{
			auto fset = finals;
			finals.clear();

			for(auto f : fset)
			{
				if(f.first.first.op == op)
				{
					for(auto fs : fset)
					{
						if(fs.first.first.op != op)
						{
							fset.clear();
							fset.push_back(f);
							break;
						}
					}
				}
			}

			finals = fset;
		}




		if(finals.size() > 1)
			error(us, "More than one possible operator overload candidate in this expression");

		else if(finals.size() == 0)
		{
			_OpOverloadData ret;
			ret.found = false;

			return ret;
		}

		auto cand = finals.front();

		_OpOverloadData ret;
		ret.found = true;

		ret.isBinOp = cand.first.first.isBinOp;
		ret.isPrefix = cand.first.first.isPrefixUnary;
		ret.needsSwap = cand.first.first.needsSwap;
		ret.needsNot = cand.first.first.needsBooleanNOT;

		ret.opFunc = cand.first.second;

		return ret;
	}





	Result_t CodegenInstance::callBinaryOperatorOverload(_OpOverloadData data, fir::Value* lhs, fir::Value* lref, fir::Value* rhs,
		fir::Value* rref, ArithmeticOp op)
	{
		// check if we have a ref.
		if(lref == 0)
		{
			// we don't have a pointer-type ref, which is required for operators to work.
			// create one.

			iceAssert(lhs);

			fir::Value* ptr = this->builder.CreateStackAlloc(lhs->getType());
			this->builder.CreateStore(lhs, ptr);

			lref = ptr;
		}
		if(rref == 0)
		{
			iceAssert(rhs);

			fir::Value* ptr = this->builder.CreateStackAlloc(rhs->getType());
			this->builder.CreateStore(rhs, ptr);

			rref = ptr;
		}

		bool isBinOp	= data.isBinOp;
		bool needsSwap	= data.needsSwap;
		bool needsNot	= data.needsNot;


		fir::Function* opFunc = data.opFunc ? this->module->getFunction(data.opFunc->getName()) : 0;

		if(!opFunc)
			error("wtf??");

		fir::Value* ret = 0;

		if(isBinOp)
		{
			fir::Value* larg = 0;
			fir::Value* rarg = 0;

			if(this->isArithmeticOpAssignment(op))
			{
				if(needsSwap)
				{
					larg = rref;
					rarg = lhs;
				}
				else
				{
					larg = lref;
					rarg = rhs;
				}
			}
			else
			{
				if(needsSwap)
				{
					larg = rhs;
					rarg = lhs;
				}
				else
				{
					larg = lhs;
					rarg = rhs;
				}
			}


			ret = this->builder.CreateCall2(opFunc, larg, rarg);
		}
		else
		{
			iceAssert(0 && "not sup unary");
		}



		if(needsNot)
		{
			ret = this->builder.CreateICmpEQ(ret, fir::ConstantInt::getNullValue(ret->getType()));
		}

		return Result_t(ret, 0);
	}
}



































