// Overloaded.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;




Result_t BinOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	iceAssert(this->left && this->right);
	return Operators::OperatorMap::get().call(this->op, cgi, this, { this->left, this->right });
}

fir::Type* BinOp::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	fir::Type* ltype = this->left->getType(cgi);
	fir::Type* rtype = this->right->getType(cgi);

	if(this->op == ArithmeticOp::CmpLT || this->op == ArithmeticOp::CmpGT || this->op == ArithmeticOp::CmpLEq
	|| this->op == ArithmeticOp::CmpGEq || this->op == ArithmeticOp::CmpEq || this->op == ArithmeticOp::CmpNEq)
	{
		return fir::Type::getBool(cgi->getContext());
	}
	else if(this->op == ArithmeticOp::Cast || this->op == ArithmeticOp::ForcedCast)
	{
		// assume that the cast is valid
		// we'll check the validity of this claim later, during codegen.
		return this->right->getType(cgi);
	}
	else if(this->op >= ArithmeticOp::UserDefined)
	{
		auto data = cgi->getBinaryOperatorOverload(this, this->op, ltype, rtype);

		if(!data.found)
		{
			error(this, "No such custom operator '%s' for types '%s' and '%s'", Parser::arithmeticOpToString(cgi, this->op).c_str(),
				ltype->str().c_str(), rtype->str().c_str());
		}

		iceAssert(data.found);
		return data.opFunc->getReturnType();
	}
	else
	{
		// check if both are integers
		if(ltype->isIntegerType() && rtype->isIntegerType())
		{
			if(ltype->toPrimitiveType()->getIntegerBitWidth() > rtype->toPrimitiveType()->getIntegerBitWidth())
				return ltype;

			return rtype;
		}
		else if(ltype->isIntegerType() && rtype->isFloatingPointType())
		{
			return rtype;
		}
		else if(ltype->isFloatingPointType() && rtype->isIntegerType())
		{
			return ltype;
		}
		else if(ltype->isFloatingPointType() && rtype->isFloatingPointType())
		{
			if(ltype->toPrimitiveType()->getFloatingPointBitWidth() > rtype->toPrimitiveType()->getFloatingPointBitWidth())
				return ltype;

			return rtype;
		}
		else if(ltype->isStringType() && rtype->isStringType())
		{
			return ltype;
		}
		else if(ltype->isDynamicArrayType() && rtype->isDynamicArrayType()
			&& ltype->toDynamicArrayType()->getElementType() == rtype->toDynamicArrayType()->getElementType())
		{
			return ltype;
		}
		else if(ltype->isDynamicArrayType() && ltype->toDynamicArrayType()->getElementType() == rtype)
		{
			return ltype;
		}
		else if(ltype->isEnumType() && ltype == rtype && (op == ArithmeticOp::BitwiseAnd || op == ArithmeticOp::BitwiseOr || op == ArithmeticOp::BitwiseXor || op == ArithmeticOp::BitwiseNot))
		{
			return ltype;
		}
		else
		{
			if((ltype->isPointerType() && rtype->isIntegerType()) || (rtype->isPointerType() && ltype->isIntegerType()))
			{
				// pointer arith??
				return ltype->isPointerType() ? ltype : rtype;
			}

			auto data = cgi->getBinaryOperatorOverload(this, this->op, ltype, rtype);
			if(data.found)
			{
				return data.opFunc->getReturnType();
			}
			else
			{
				error(this, "No such operator overload for operator '%s' accepting types '%s' and '%s'.",
					Parser::arithmeticOpToString(cgi, this->op).c_str(), ltype->str().c_str(), rtype->str().c_str());
			}
		}
	}
}









namespace Codegen
{
	bool CodegenInstance::isValidOperatorForBuiltinTypes(ArithmeticOp op, fir::Type* lhs, fir::Type* rhs)
	{
		auto fn = &CodegenInstance::isValidOperatorForBuiltinTypes;

		switch(op)
		{
			case ArithmeticOp::Add:
			{
				if(lhs->isPrimitiveType() && rhs->isPrimitiveType())	return true;
				else if(lhs->isStringType() && rhs->isStringType())		return true;
				else if(lhs->isPointerType() && rhs->isIntegerType())	return true;
				else if(lhs->isIntegerType() && rhs->isPointerType())	return true;

				return false;
			}

			case ArithmeticOp::Subtract:
			{
				// no strings, and no int - ptr (only ptr - int is valid)
				if(lhs->isPrimitiveType() && rhs->isPrimitiveType())	return true;
				else if(lhs->isPointerType() && rhs->isIntegerType())	return true;

				return false;
			}

			case ArithmeticOp::Multiply:		// fallthrough
			case ArithmeticOp::Divide:			// fallthrough
			case ArithmeticOp::Modulo:			// fallthrough
			case ArithmeticOp::ShiftLeft:		// fallthrough
			case ArithmeticOp::ShiftRight:		return (lhs->isPrimitiveType() && rhs->isPrimitiveType());

			case ArithmeticOp::Assign:
			{
				if(lhs->isPrimitiveType() && rhs->isPrimitiveType())	return true;
				else if(lhs->isStringType() && rhs->isStringType())		return true;
				else if(lhs->isPointerType() && rhs->isPointerType()
					&& lhs->getPointerElementType() == rhs->getPointerElementType())	return true;

				return false;
			}

			case ArithmeticOp::CmpLT:
			case ArithmeticOp::CmpGT:
			case ArithmeticOp::CmpLEq:
			case ArithmeticOp::CmpGEq:
			case ArithmeticOp::CmpEq:
			case ArithmeticOp::CmpNEq:
			{
				if(lhs->isPrimitiveType() && rhs->isPrimitiveType())	return true;
				else if(lhs->isStringType() && rhs->isStringType())		return true;
				else if(lhs->isPointerType() && rhs->isPointerType()
					&& lhs->getPointerElementType() == rhs->getPointerElementType())	return true;

				return false;
			}

			case ArithmeticOp::BitwiseAnd:
			case ArithmeticOp::BitwiseOr:
			case ArithmeticOp::BitwiseXor:
			{
				return (lhs->isPrimitiveType() && rhs->isPrimitiveType());
			}

			case ArithmeticOp::LogicalOr:
			case ArithmeticOp::LogicalAnd:
			{
				return lhs->isPrimitiveType() && lhs->toPrimitiveType()->getIntegerBitWidth() == 1 &&  rhs->isPrimitiveType() && rhs->toPrimitiveType()->getIntegerBitWidth() == 1;
			}


			// lol. mfw member function pointer syntax
			case ArithmeticOp::PlusEquals:
				return (this->*fn)(ArithmeticOp::Add, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::MinusEquals:
				return (this->*fn)(ArithmeticOp::Subtract, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::MultiplyEquals:
				return (this->*fn)(ArithmeticOp::Multiply, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::DivideEquals:
				return (this->*fn)(ArithmeticOp::Divide, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::ModEquals:
				return (this->*fn)(ArithmeticOp::Modulo, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::ShiftLeftEquals:
				return (this->*fn)(ArithmeticOp::ShiftLeft, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::ShiftRightEquals:
				return (this->*fn)(ArithmeticOp::ShiftRight, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::BitwiseAndEquals:
				return (this->*fn)(ArithmeticOp::BitwiseAnd, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::BitwiseOrEquals:
				return (this->*fn)(ArithmeticOp::BitwiseOr, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);
			case ArithmeticOp::BitwiseXorEquals:
				return (this->*fn)(ArithmeticOp::BitwiseXor, lhs, rhs) && (this->*fn)(ArithmeticOp::Assign, lhs, rhs);

			// case ArithmeticOp::Subscript:
			// {
			// 	return lhs->isStringType();
			// }

			default:
				return false;
		}
	}



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

			bool isMember = 0;


			int castedDist = 0;
		};


		if(this->isValidOperatorForBuiltinTypes(op, lhs, rhs))
		{
			_OpOverloadData ret;

			ret.found				= true;
			ret.isPrefix			= false;
			ret.needsSwap			= false;
			ret.needsNot			= false;
			ret.isMember			= false;
			ret.isBuiltin			= true;

			ret.opFunc				= 0;

			return ret;
		}


		std::deque<std::pair<Attribs, fir::Function*>> candidates;



		auto findCandidatesPass1 = [lhs, rhs](CodegenInstance* cgi, std::deque<std::pair<Attribs, fir::Function*>>* cands,
			std::deque<OpOverload*> list, ArithmeticOp op, bool skipGeneric)
		{
			for(auto oo : list)
			{
				Attribs attr;

				attr.op				= oo->op;
				attr.isBinOp		= oo->kind == OpOverload::OperatorKind::CommBinary || oo->kind == OpOverload::OperatorKind::NonCommBinary;
				attr.isCommutative	= oo->kind == OpOverload::OperatorKind::CommBinary;
				attr.isPrefixUnary	= oo->kind == OpOverload::OperatorKind::PrefixUnary;
				attr.isMember		= oo->func->decl->parentClass != 0;

				attr.needsSwap		= false;

				fir::Function* lfunc = oo->lfunc;

				// skip the generic ones first.
				if((!lfunc || !oo->didCodegen) && (oo->op == op || (oo->op == ArithmeticOp::CmpEq && op == ArithmeticOp::CmpNEq)
					|| (oo->op == ArithmeticOp::CmpNEq && op == ArithmeticOp::CmpEq)))
				{
					// kinda a hack
					// if we have operators that use other operators, they all should be codegened first.
					// unless this leads to a stupid loop...
					// todo: edge case detection/fixing?

					if(oo->func->decl->genericTypes.size() == 0 || !skipGeneric)
					{
						// info(oo->func->decl, "generating");
						lfunc = dynamic_cast<fir::Function*>(oo->codegen(cgi, { lhs, rhs }).value);
					}
				}

				if(!lfunc) continue;


				if(oo->op == op)
				{
					attr.needsBooleanNOT = false;
					(*cands).push_back({ attr, lfunc });
				}
				else if((oo->op == ArithmeticOp::CmpEq && op == ArithmeticOp::CmpNEq)
					|| (oo->op == ArithmeticOp::CmpNEq && op == ArithmeticOp::CmpEq))
				{
					attr.needsBooleanNOT = true;
					(*cands).push_back({ attr, lfunc });
				}
			}
		};




		// get the functree, starting from here, up.
		std::deque<OpOverload*> list;
		{
			auto curDepth = this->namespaceStack;

			for(size_t i = 0; i <= this->namespaceStack.size(); i++)
			{
				FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
				if(!ft) break;

				for(auto f : ft->operators)
					list.push_back(f);

				if(curDepth.size() > 0)
					curDepth.pop_back();
			}

			// if we're assigning things, we need to get the assignfuncs as well.
			// if(this->isArithmeticOpAssignment(op))

			if(lhs->isPointerType() && (lhs->getPointerElementType()->isStructType() || lhs->getPointerElementType()->isClassType()))
				lhs = lhs->getPointerElementType();

			if(TypePair_t* tp = this->getType(lhs))
			{
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
							atr.isMember		= true;

							// iceAssert(aso->lfunc);
							candidates.push_back({ atr, aso->lfunc });
						}
					}

					for(auto ext : this->getExtensionsForType(cls))
					{
						for(auto aso : ext->assignmentOverloads)
						{
							if(aso->op == op)
							{
								Attribs atr;

								atr.op				= op;
								atr.isBinOp			= true;
								atr.isPrefixUnary	= false;
								atr.isCommutative	= false;
								atr.needsSwap		= false;
								atr.isMember		= true;

								// iceAssert(aso->lfunc);
								candidates.push_back({ atr, aso->lfunc });
							}
						}
					}


					for(auto ovl : cls->operatorOverloads)
						list.push_back(ovl);

					for(auto ext : this->getExtensionsForType(cls))
					{
						for(auto ovl : ext->operatorOverloads)
							list.push_back(ovl);
					}
				}
			}


			findCandidatesPass1(this, &candidates, list, op, true);
		}


		bool didDoGenerics = false;
		resetForGenerics:


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

		for(auto cand : set)
		{
			fir::Type* targL = cand.second->getArguments()[0]->getType();
			fir::Type* targR = cand.second->getArguments()[1]->getType();

			fir::Type* thelhs = (cand.first.isMember ? lhs->getPointerTo() : lhs);

			// if unary op, only LHS is used.
			if(cand.first.isBinOp)
			{
				int d1 = this->getAutoCastDistance(thelhs, targL);
				int d2 = this->getAutoCastDistance(rhs, targR);


				if(targL == thelhs && targR == rhs)
				{
					candidates.push_back(cand);
				}
				else if(cand.first.isCommutative && targR == thelhs && targL == rhs)
				{
					cand.first.needsSwap = true;
					candidates.push_back(cand);
				}
				else if(d1 >= 0 && d2 >= 0)
				{
					// check for non-exact matching

					cand.first.castedDist += (d1 + d2);
					candidates.push_back(cand);
				}
				else if(cand.first.isCommutative && this->getAutoCastDistance(thelhs, targR) >= 0 && this->getAutoCastDistance(rhs, targL) >= 0)
				{
					cand.first.needsSwap = true;

					cand.first.castedDist += (this->getAutoCastDistance(thelhs, targR) + this->getAutoCastDistance(rhs, targL));
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


		// final final step: if we still have more than 1, disambiguate using casting distance.
		if(finals.size() > 1)
		{
			int best = 9999999;

			auto fset = finals;
			finals.clear();

			for(auto f : fset)
			{
				if(f.first.first.castedDist == best)
					finals.push_back(f);

				else if(f.first.first.castedDist < best)
					finals.clear(), finals.push_back(f), best = f.first.first.castedDist;
			}
		}



		if(finals.size() > 1)
		{
			exitless_error(us, "More than one possible operator overload candidate in this expression");
			for(auto c : finals)
				info("<%d> %s: %s", c.first.first.castedDist, c.first.second->getName().str().c_str(), c.first.second->getType()->str().c_str());

			doTheExit();
		}
		else if(finals.size() == 0)
		{
			if(didDoGenerics)
			{
				_OpOverloadData ret;
				ret.found = false;

				return ret;
			}
			else
			{
				// seriously? goto?
				// todo(goto): get rid.

				// the point of this is to try "specific" operators before generic ones.
				// so some general op == ($T, $T) won't be preferred over a more specific op == (type1, type2).

				didDoGenerics = true;
				candidates.clear();
				findCandidatesPass1(this, &candidates, list, op, false);

				goto resetForGenerics;
			}
		}

		auto cand = finals.front();

		_OpOverloadData ret;
		ret.found = true;

		ret.isBinOp = cand.first.first.isBinOp;
		ret.isPrefix = cand.first.first.isPrefixUnary;
		ret.needsSwap = cand.first.first.needsSwap;
		ret.needsNot = cand.first.first.needsBooleanNOT;
		ret.isMember = cand.first.first.isMember;

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

			fir::Value* ptr = this->irb.CreateStackAlloc(lhs->getType());
			this->irb.CreateStore(lhs, ptr);

			lref = ptr;
		}
		if(rref == 0)
		{
			iceAssert(rhs);

			fir::Value* ptr = this->irb.CreateStackAlloc(rhs->getType());
			this->irb.CreateStore(rhs, ptr);

			rref = ptr;
		}

		bool isBinOp	= data.isBinOp;
		bool needsSwap	= data.needsSwap;
		bool needsNot	= data.needsNot;
		bool isMember	= data.isMember;

		fir::Function* opFunc = data.opFunc ? this->module->getFunction(data.opFunc->getName()) : 0;

		if(!opFunc)
			error("wtf??");

		fir::Value* ret = 0;

		if(isBinOp)
		{
			fir::Value* larg = 0;
			fir::Value* rarg = 0;

			if(isMember)
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


			// if(larg->getType() != opFunc->getArguments()[0]->getType())
			// 	larg = this->autoCastType(opFunc->getArguments()[0]->getType(), larg);

			// if(larg->getType() != opFunc->getArguments()[0]->getType())
			// 	error("mismatched operator argument 0");

			// if(rarg->getType() != opFunc->getArguments()[1]->getType())
			// 	rarg = this->autoCastType(opFunc->getArguments()[1]->getType(), rarg);

			// if(rarg->getType() != opFunc->getArguments()[1]->getType())
			// 	error("mismatched operator argument 1");



			ret = this->irb.CreateCall2(opFunc, larg, rarg);
		}
		else
		{
			iceAssert(0 && "not sup unary");
		}



		if(needsNot)
		{
			ret = this->irb.CreateICmpEQ(ret, fir::ConstantInt::getNullValue(ret->getType()));
		}

		return Result_t(ret, 0);
	}
}



































