// TypeUtils.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	fir::Type* CodegenInstance::getExprTypeOfBuiltin(std::string type)
	{
		int indirections = 0;
		type = unwrapPointerType(type, &indirections);

		fir::Type* real = fir::Type::fromBuiltin(type);
		if(!real) return 0;

		iceAssert(real);
		while(indirections > 0)
		{
			real = real->getPointerTo();
			indirections--;
		}

		return real;
	}


	fir::Value* CodegenInstance::lastMinuteUnwrapType(Expr* user, fir::Value* alloca)
	{
		if(!alloca->getType()->isPointerType())
			error("expected pointer, got %s", alloca->getType()->str().c_str());

		iceAssert(alloca->getType()->isPointerType());
		fir::Type* baseType = alloca->getType()->getPointerElementType();

		if((baseType->isClassType() || baseType->isStructType()) && (this->isEnum(baseType) || this->isTypeAlias(baseType)))
		{
			TypePair_t* tp = this->getType(baseType);
			if(!tp)
				error(user, "Invalid type (%s)", baseType->str().c_str());

			iceAssert(tp->second.second == TypeKind::Enum);
			EnumDef* enr = dynamic_cast<EnumDef*>(tp->second.first);

			iceAssert(enr);
			if(enr->isStrong)
			{
				return alloca;		// fail.
			}

			return this->builder.CreateStructGEP(alloca, 0);
		}

		return alloca;
	}

	fir::Type* CodegenInstance::getExprType(Expr* expr, bool allowFail, bool setInferred)
	{
		return this->getExprType(expr, Resolved_t(), allowFail, setInferred);
	}

	fir::Type* CodegenInstance::getExprType(Expr* expr, Resolved_t preResolvedFn, bool allowFail, bool setInferred)
	{
		if(expr->ptype && expr->ptype->resolvedFType)
			return expr->ptype->resolvedFType;

		setInferred = false;
		iceAssert(expr);
		{
			if(VarDecl* decl = dynamic_cast<VarDecl*>(expr))
			{
				if(decl->ptype == pts::InferredType::get())
				{
					if(!decl->inferredLType)		// todo: better error detection for this
					{
						error(expr, "Invalid variable declaration for %s!", decl->ident.name.c_str());
					}

					iceAssert(decl->inferredLType);
					return decl->inferredLType;
				}
				else
				{
					// if we already "inferred" the type, don't bother doing it again.
					if(decl->inferredLType)
						return decl->inferredLType;

					fir::Type* ret = this->getTypeFromParserType(decl, decl->ptype, allowFail);
					if(setInferred) decl->inferredLType = ret;

					return ret;
				}
			}
			else if(VarRef* ref = dynamic_cast<VarRef*>(expr))
			{
				VarDecl* decl = getSymDecl(ref, ref->name);
				if(!decl)
					GenError::unknownSymbol(this, expr, ref->name, SymbolType::Variable);

				auto x = this->getExprType(decl, allowFail);
				return x;
			}
			else if(UnaryOp* uo = dynamic_cast<UnaryOp*>(expr))
			{
				if(uo->op == ArithmeticOp::Deref)
				{
					fir::Type* ltype = this->getExprType(uo->expr);
					if(!ltype->isPointerType())
						error(expr, "Attempted to dereference a non-pointer type '%s'", this->getReadableType(ltype).c_str());

					return this->getExprType(uo->expr)->getPointerElementType();
				}

				else if(uo->op == ArithmeticOp::AddrOf)
					return this->getExprType(uo->expr)->getPointerTo();

				else
					return this->getExprType(uo->expr);
			}
			else if(FuncCall* fc = dynamic_cast<FuncCall*>(expr))
			{
				Resolved_t& res = preResolvedFn;
				if(!res.resolved)
				{
					Resolved_t rt = this->resolveFunction(expr, fc->name, fc->params);
					if(!rt.resolved)
					{
						TypePair_t* tp = this->getTypeByString(fc->name);
						if(tp)
						{
							return tp->first;
						}
						else
						{
							auto genericMaybe = this->tryResolveGenericFunctionCall(fc);
							if(genericMaybe.first)
							{
								fc->cachedResolveTarget = Resolved_t(genericMaybe);
								return genericMaybe.first->getReturnType();
							}

							GenError::prettyNoSuchFunctionError(this, fc, fc->name, fc->params);
						}
					}
					else
					{
						res = rt;
					}
				}

				return getExprType(res.t.second);
			}
			else if(Func* f = dynamic_cast<Func*>(expr))
			{
				return getExprType(f->decl);
			}
			else if(FuncDecl* fd = dynamic_cast<FuncDecl*>(expr))
			{
				fir::Type* t = this->getTypeFromParserType(fd, fd->ptype, allowFail);
				if(t)
					return t;

				else if(!t && allowFail)
					return 0;

				else
					error(fd, "Unknown type '%s'", fd->ptype->str().c_str());


				// TypePair_t* type = this->getType(t);
				// if(!type && allowFail)
				// 	return 0;

				// else if(!type)
				// 	error(fd, "Unknown type '%s'", fd->ptype->str().c_str());

				// return type->first;
			}
			else if(StringLiteral* sl = dynamic_cast<StringLiteral*>(expr))
			{
				if(sl->isRaw)
					return fir::PointerType::getInt8Ptr(this->getContext());

				else
				{
					auto tp = this->getTypeByString("String");
					if(!tp)
						return fir::PointerType::getInt8Ptr(this->getContext());


					return tp->first;
				}
			}
			else if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
			{
				if(ma->matype == MAType::LeftNamespace || ma->matype == MAType::LeftTypename)
					return this->resolveStaticDotOperator(ma, false).first.first;


				// first, get the type of the lhs
				fir::Type* lhs = this->getExprType(ma->left);
				TypePair_t* pair = this->getType(lhs->isPointerType() ? lhs->getPointerElementType() : lhs);

				if(!pair && !lhs->isTupleType())
					error(expr, "Invalid type '%s' for dot-operator-access", this->getReadableType(lhs).c_str());

				if(lhs->isTupleType())
				{
					// values are 1, 2, 3 etc.
					// for now, assert this.

					fir::TupleType* tt = lhs->toTupleType();
					iceAssert(tt);

					Number* n = dynamic_cast<Number*>(ma->right);
					iceAssert(n);

					if((size_t) n->ival >= tt->getElementCount())
					{
						error(expr, "Tuple does not have %d elements, only %zd", (int) n->ival + 1, tt->getElementCount());
					}

					return tt->getElementN(n->ival);
				}
				else if(pair->second.second == TypeKind::Class)
				{
					ClassDef* cls = dynamic_cast<ClassDef*>(pair->second.first);
					iceAssert(cls);

					VarRef* memberVr = dynamic_cast<VarRef*>(ma->right);
					FuncCall* memberFc = dynamic_cast<FuncCall*>(ma->right);

					if(memberVr)
					{
						for(VarDecl* mem : cls->members)
						{
							if(mem->ident.name == memberVr->name)
								return this->getExprType(mem);
						}
						for(ComputedProperty* c : cls->cprops)
						{
							if(c->ident.name == memberVr->name)
								return this->getTypeFromParserType(c, c->ptype, allowFail);
						}

						auto exts = this->getExtensionsForType(cls);
						for(auto ext : exts)
						{
							for(auto cp : ext->cprops)
							{
								if(cp->attribs & Attr_VisPublic || ext->parentRoot == this->rootNode)
								{
									if(cp->ident.name == memberVr->name)
										return this->getExprType(cp);
								}
							}
						}
					}
					else if(memberFc)
					{
						return this->resolveMemberFuncCall(ma, cls, memberFc).second->getReturnType();
					}
				}
				else if(pair->second.second == TypeKind::Struct)
				{
					StructDef* str = dynamic_cast<StructDef*>(pair->second.first);
					iceAssert(str);

					VarRef* memberVr = dynamic_cast<VarRef*>(ma->right);
					FuncCall* memberFc = dynamic_cast<FuncCall*>(ma->right);

					if(memberVr)
					{
						for(VarDecl* mem : str->members)
						{
							if(mem->ident.name == memberVr->name)
								return this->getExprType(mem);
						}

						auto exts = this->getExtensionsForType(str);
						for(auto ext : exts)
						{
							for(auto cp : ext->cprops)
							{
								if(cp->attribs & Attr_VisPublic || ext->parentRoot == this->rootNode)
								{
									if(cp->ident.name == memberVr->name)
										return this->getExprType(cp);
								}
							}
						}
					}
					else if(memberFc)
					{
						error(memberFc, "Tried to call method on struct");
					}
				}
				else if(pair->second.second == TypeKind::Enum)
				{
					EnumDef* enr = dynamic_cast<EnumDef*>(pair->second.first);
					iceAssert(enr);

					VarRef* enrcase = dynamic_cast<VarRef*>(ma->right);
					iceAssert(enrcase);

					for(auto c : enr->cases)
					{
						if(c.first == enrcase->name)
							return this->getExprType(c.second);
					}

					error(expr, "Enum '%s' has no such case '%s'", enr->ident.name.c_str(), enrcase->name.c_str());
				}
				else
				{
					error(expr, "Invalid expr type (%s)", typeid(*pair->second.first).name());
				}
			}
			else if(BinOp* bo = dynamic_cast<BinOp*>(expr))
			{
				fir::Type* ltype = this->getExprType(bo->left);
				fir::Type* rtype = this->getExprType(bo->right);

				if(bo->op == ArithmeticOp::CmpLT || bo->op == ArithmeticOp::CmpGT || bo->op == ArithmeticOp::CmpLEq
				|| bo->op == ArithmeticOp::CmpGEq || bo->op == ArithmeticOp::CmpEq || bo->op == ArithmeticOp::CmpNEq)
				{
					return fir::PrimitiveType::getBool(this->getContext());
				}
				else if(bo->op == ArithmeticOp::Cast || bo->op == ArithmeticOp::ForcedCast)
				{
					return this->getExprType(bo->right);
				}
				else if(bo->op >= ArithmeticOp::UserDefined)
				{
					auto data = this->getBinaryOperatorOverload(bo, bo->op, ltype, rtype);

					if(!data.found)
					{
						error(bo, "No such custom operator '%s' for types '%s' and '%s'", Parser::arithmeticOpToString(this, bo->op).c_str(),
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
					else
					{
						if(ltype->isPointerType() && rtype->isIntegerType())
						{
							// pointer arith??
							return ltype;
						}

						auto data = this->getBinaryOperatorOverload(bo, bo->op, ltype, rtype);
						if(data.found)
						{
							return data.opFunc->getReturnType();
						}
						else
						{
							error(expr, "No such operator overload for operator '%s' accepting types %s and %s.",
								Parser::arithmeticOpToString(this, bo->op).c_str(), this->getReadableType(ltype).c_str(),
								this->getReadableType(rtype).c_str());
						}
					}
				}
			}
			else if(Alloc* alloc = dynamic_cast<Alloc*>(expr))
			{
				fir::Type* base = this->getTypeFromParserType(alloc, alloc->ptype);

				if(alloc->counts.size() == 0) return base->getPointerTo();

				for(size_t i = 0; i < alloc->counts.size(); i++)
					 base = base->getPointerTo();

				return base;
			}
			else if(Number* nm = dynamic_cast<Number*>(expr))
			{
				return nm->codegen(this).result.first->getType();
			}
			else if(dynamic_cast<BoolVal*>(expr))
			{
				return fir::PrimitiveType::getBool(getContext());
			}
			else if(Return* retr = dynamic_cast<Return*>(expr))
			{
				return this->getExprType(retr->val);
			}
			else if(DummyExpr* dum = dynamic_cast<DummyExpr*>(expr))
			{
				return this->getTypeFromParserType(expr, dum->ptype);
			}
			else if(dynamic_cast<IfStmt*>(expr))
			{
				return fir::PrimitiveType::getVoid(getContext());
			}
			else if(dynamic_cast<Typeof*>(expr))
			{
				TypePair_t* tp = this->getTypeByString("Type");
				iceAssert(tp);

				return tp->first;
			}
			else if(Tuple* tup = dynamic_cast<Tuple*>(expr))
			{
				fir::Type* tp = tup->createdType;
				if(!tup->didCreateType)
					tp = tup->getType(this);

				iceAssert(tp);
				return tp;
			}
			else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(expr))
			{
				fir::Type* t = this->getExprType(ai->arr);
				if(!t->isArrayType() && !t->isPointerType() && !t->isLLVariableArrayType())
				{
					// todo: multiple subscripts
					fir::Function* getter = Operators::getOperatorSubscriptGetter(this, expr, t, { expr, ai->index });
					if(!getter)
					{
						error(expr, "Invalid subscript on type %s, with index type %s", t->str().c_str(),
							this->getReadableType(ai->index).c_str());
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
			else if(ArrayLiteral* al = dynamic_cast<ArrayLiteral*>(expr))
			{
				// todo: make this not shit.
				// edit: ???
				return fir::ArrayType::get(this->getExprType(al->values.front()), al->values.size());
			}
			else if(dynamic_cast<NullVal*>(expr))
			{
				return fir::ConstantValue::getNull()->getType();
			}
			else if(dynamic_cast<PostfixUnaryOp*>(expr))
			{
				iceAssert(0);
			}
		}

		error(expr, "(%s:%d) -> Internal check failed: failed to determine type '%s'", __FILE__, __LINE__, typeid(*expr).name());
	}

	fir::Value* CodegenInstance::getStackAlloc(fir::Type* type, std::string name)
	{
		return this->builder.CreateStackAlloc(type, name);
	}

	fir::Value* CodegenInstance::getImmutStackAllocValue(fir::Value* initValue, std::string name)
	{
		return this->builder.CreateImmutStackAlloc(initValue->getType(), initValue, name);
	}


	fir::Value* CodegenInstance::getDefaultValue(Expr* e)
	{
		return fir::ConstantValue::getNullValue(this->getExprType(e));
	}

	fir::Function* CodegenInstance::getDefaultConstructor(Expr* user, fir::Type* ptrType, StructBase* sb)
	{
		// check if we have a default constructor.

		if(ClassDef* cls = dynamic_cast<ClassDef*>(sb))
		{
			fir::Function* candidate = 0;
			for(fir::Function* fn : cls->initFuncs)
			{
				if(fn->getArgumentCount() == 1 && (fn->getArguments()[0]->getType() == ptrType))
				{
					candidate = fn;
					break;
				}
			}

			if(candidate == 0)
				error(user, "Struct %s has no default initialiser taking 0 parameters", cls->ident.name.c_str());

			return candidate;
		}
		else if(StructDef* str = dynamic_cast<StructDef*>(sb))
		{
			// should be the front one.
			iceAssert(str->initFuncs.size() > 0);
			return str->initFuncs[0];
		}
		else
		{
			error(user, "Type '%s' cannot have initialisers", sb->ident.name.c_str());
		}
	}




	std::string CodegenInstance::getReadableType(fir::Type* type)
	{
		if(type == 0)
			return "(null)";

		return type->str();
	}

	std::string CodegenInstance::getReadableType(fir::Value* val)
	{
		if(val == 0) return "(null)";
		return this->getReadableType(val->getType());
	}

	std::string CodegenInstance::getReadableType(Expr* expr)
	{
		return this->getReadableType(this->getExprType(expr));
	}

	int CodegenInstance::getAutoCastDistance(fir::Type* from, fir::Type* to)
	{
		if(!from || !to)
			return -1;

		if(from->isTypeEqual(to))
			return 0;

		int ret = 0;
		if(from->isIntegerType() && to->isIntegerType()
			&& (from->toPrimitiveType()->getIntegerBitWidth() != to->toPrimitiveType()->getIntegerBitWidth()
				|| from->toPrimitiveType()->isSigned() != to->toPrimitiveType()->isSigned()))
		{
			unsigned int ab = from->toPrimitiveType()->getIntegerBitWidth();
			unsigned int bb = to->toPrimitiveType()->getIntegerBitWidth();

			// we only allow promotion, never truncation (implicitly anyway)

			// todo: do we?
			// if(ab > bb) return -1;

			// fk it
			if(ab == 8)
			{
				if(bb == 8)			ret = 0;
				else if(bb == 16)	ret = 1;
				else if(bb == 32)	ret = 2;
				else if(bb == 64)	ret = 3;
			}
			if(ab == 16)
			{
				if(bb == 8)			ret = 1;
				else if(bb == 16)	ret = 0;
				else if(bb == 32)	ret = 1;
				else if(bb == 64)	ret = 2;
			}
			if(ab == 32)
			{
				if(bb == 8)			ret = 2;
				else if(bb == 16)	ret = 1;
				else if(bb == 32)	ret = 0;
				else if(bb == 64)	ret = 1;
			}
			if(ab == 64)
			{
				if(bb == 8)			ret = 3;
				else if(bb == 16)	ret = 2;
				else if(bb == 32)	ret = 1;
				else if(bb == 64)	ret = 0;
			}

			// check for signed-ness cast.
			if(from->toPrimitiveType()->isSigned() != to->toPrimitiveType()->isSigned())
			{
				ret += 1;
			}

			return ret;
		}
		// check for string to int8*
		else if(to->isPointerType() && to->getPointerElementType() == fir::PrimitiveType::getInt8(this->getContext())
			&& from->isClassType() && from->toClassType()->getClassName().str() == "String")
		{
			return 2;
		}
		else if(from->isPointerType() && from->getPointerElementType() == fir::PrimitiveType::getInt8(this->getContext())
			&& to->isClassType() && to->toClassType()->getClassName().str() == "String")
		{
			return 2;
		}
		else if(to->isFloatingPointType() && from->isIntegerType())
		{
			// int-to-float is 10.
			return 10;
		}
		else if(to->isPointerType() && from->isNullPointer())
		{
			return 5;
		}
		else if(this->isAnyType(to))
		{
			// any cast is 25.
			return 25;
		}
		else if(from->isTupleType() && to->isTupleType() && from->toTupleType()->getElementCount() == to->toTupleType()->getElementCount())
		{
			int sum = 0;
			for(size_t i = 0; i < from->toTupleType()->getElementCount(); i++)
			{
				int d = this->getAutoCastDistance(from->toTupleType()->getElementN(i), to->toTupleType()->getElementN(i));
				if(d == -1)
					return -1;		// note: make sure this is the last case

				sum += d;
			}

			return sum;
		}

		return -1;
	}

	fir::Value* CodegenInstance::autoCastType(fir::Type* target, fir::Value* from, fir::Value* fromPtr, int* distance)
	{
		if(!target || !from)
			return from;

		// casting distance for size is determined by the number of "jumps"
		// 8 -> 16 = 1
		// 8 -> 32 = 2
		// 8 -> 64 = 3
		// 16 -> 64 = 2
		// etc.

		fir::Value* retval = from;

		if(from->getType() == target)
		{
			retval = from;
		}
		if(target->isIntegerType() && from->getType()->isIntegerType()
			&& target->toPrimitiveType()->getIntegerBitWidth() != from->getType()->toPrimitiveType()->getIntegerBitWidth())
		{
			unsigned int lBits = target->toPrimitiveType()->getIntegerBitWidth();
			unsigned int rBits = from->getType()->toPrimitiveType()->getIntegerBitWidth();

			bool shouldCast = lBits > rBits;

			// check if the RHS is a constant value
			fir::ConstantInt* constVal = dynamic_cast<fir::ConstantInt*>(from);
			if(constVal)
			{
				// check if the number fits in the LHS type

				if(lBits < 64)	// todo: 64 is the max
				{
					if(constVal->getSignedValue() < 0)
					{
						int64_t max = -1 * powl(2, lBits - 1);
						if(constVal->getSignedValue() >= max)
							shouldCast = true;
					}
					else
					{
						uint64_t max = powl(2, lBits) - 1;
						if(constVal->getUnsignedValue() <= max)
							shouldCast = true;
					}
				}
			}

			if(shouldCast && !constVal)
			{
				// check signed to unsiged first
				// note(behaviour): should this be implicit?
				// if we should cast, then the bitwidth should already be >=

				if(target->toPrimitiveType()->isSigned() != from->getType()->toPrimitiveType()->isSigned())
				{
					if(target->toPrimitiveType()->isSigned())
						from = this->builder.CreateIntSignednessCast(from, fir::PrimitiveType::getIntN(rBits));

					else
						from = this->builder.CreateIntSignednessCast(from, fir::PrimitiveType::getUintN(rBits));
				}

				retval = this->builder.CreateIntSizeCast(from, target);
			}
			else if(shouldCast)
			{
				// return a const, please.
				if(constVal->getType()->isSignedIntType())
					retval = fir::ConstantInt::getSigned(target, constVal->getSignedValue());

				else
					retval = fir::ConstantInt::getUnsigned(target, constVal->getUnsignedValue());
			}
		}

		// signed to unsigned conversion
		else if(target->isIntegerType() && from->getType()->isIntegerType()
			&& (from->getType()->toPrimitiveType()->isSigned() != target->toPrimitiveType()->isSigned()))
		{
			if(!from->getType()->toPrimitiveType()->isSigned() && target->toPrimitiveType()->isSigned())
			{
				// only do it if it preserves all data.
				// we cannot convert signed to unsigned, but unsigned to signed is okay if the
				// signed bitwidth > unsigned bitwidth
				// eg. u32 -> i32 >> not okay
				//     u32 -> i64 >> okay

				// TODO: making this more like C.
				// note(behaviour): check this
				// implicit casting -- signed to unsigned of SAME BITWITH IS ALLOWED.

				if(target->toPrimitiveType()->getIntegerBitWidth() >= from->getType()->toPrimitiveType()->getIntegerBitWidth())
					retval = this->builder.CreateIntSizeCast(from, target);
			}
			else
			{
				// TODO: making this more like C.
				// note(behaviour): check this
				// implicit casting -- signed to unsigned of SAME BITWITH IS ALLOWED.

				if(target->toPrimitiveType()->getIntegerBitWidth() >= from->getType()->toPrimitiveType()->getIntegerBitWidth())
					retval = this->builder.CreateIntSizeCast(from, target);
			}
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(target->isPointerType() && target->getPointerElementType() == fir::PrimitiveType::getInt8(this->getContext())
				&& from->getType()->isClassType() && from->getType()->toClassType()->getClassName().str() == "String")
		{
			// get the struct gep:
			// Layout of string:
			// var data: Int8*
			// var allocated: Uint64

			// cast the RHS to the LHS

			if(!fromPtr)
			{
				fromPtr = this->getImmutStackAllocValue(from);
			}

			iceAssert(fromPtr);
			fir::Value* ret = this->builder.CreateStructGEP(fromPtr, 0);
			retval = this->builder.CreateLoad(ret);
		}
		else if(target->isFloatingPointType() && from->getType()->isIntegerType())
		{
			// int-to-float is 10.
			retval = this->builder.CreateIntToFloatCast(from, target);
		}
		else if(target->isPointerType() && from->getType()->isNullPointer())
		{
			retval = fir::ConstantValue::getNullValue(target);
			// fprintf(stderr, "void cast, %s (%zu) // %s (%zu)\n", target->str().c_str(), from->id, retval->getType()->str().c_str(), retval->id);
		}
		else if(from->getType()->isTupleType() && target->isTupleType()
			&& from->getType()->toTupleType()->getElementCount() == target->toTupleType()->getElementCount())
		{
			// somewhat complicated
			iceAssert(fromPtr);

			fir::Value* tuplePtr = this->getStackAlloc(target);
			// fprintf(stderr, "tuplePtr = %s\n", tuplePtr->getType()->str().c_str());
			// fprintf(stderr, "from = %s, to = %s\n", from->getType()->str().c_str(), target->str().c_str());

			for(size_t i = 0; i < from->getType()->toTupleType()->getElementCount(); i++)
			{
				fir::Value* gep = this->builder.CreateStructGEP(tuplePtr, i);
				fir::Value* fromGep = this->builder.CreateStructGEP(fromPtr, i);

				// fprintf(stderr, "geps: %s, %s\n", gep->getType()->str().c_str(), fromGep->getType()->str().c_str());

				fir::Value* casted = this->autoCastType(gep->getType()->getPointerElementType(), this->builder.CreateLoad(fromGep), fromGep);

				// fprintf(stderr, "casted = %s\n", casted->getType()->str().c_str());

				this->builder.CreateStore(casted, gep);
			}

			retval = this->builder.CreateLoad(fromPtr);
		}


		int dist = this->getAutoCastDistance(from->getType(), target);
		if(distance != 0)
			*distance = dist;

		return retval;
	}


	fir::Value* CodegenInstance::autoCastType(fir::Value* left, fir::Value* right, fir::Value* rhsPtr, int* distance)
	{
		return this->autoCastType(left->getType(), right, rhsPtr, distance);
	}











	static fir::Type* _recursivelyConvertType(CodegenInstance* cgi, bool allowFail, Expr* user, pts::Type* pt)
	{
		if(pt->isPointerType())
		{
			return _recursivelyConvertType(cgi, allowFail, user, pt->toPointerType()->base)->getPointerTo();
		}
		else if(pt->isFixedArrayType())
		{
			fir::Type* base = _recursivelyConvertType(cgi, allowFail, user, pt->toFixedArrayType()->base);
			return fir::ArrayType::get(base, pt->toFixedArrayType()->size);
		}
		else if(pt->isVariadicArrayType())
		{
			fir::Type* base = _recursivelyConvertType(cgi, allowFail, user, pt->toVariadicArrayType()->base);
			return fir::LLVariableArrayType::get(base);
		}
		else if(pt->isDynamicArrayType())
		{
			error("dynamic arrays not supported");
		}
		else if(pt->isTupleType())
		{
			std::deque<fir::Type*> types;
			for(auto t : pt->toTupleType()->types)
				types.push_back(_recursivelyConvertType(cgi, allowFail, user, t));

			return fir::TupleType::get(types);
		}
		else if(pt->isNamedType())
		{
			// the bulk.
			std::string strtype = pt->toNamedType()->name;

			if(strtype == "T")
			{

			}

			fir::Type* ret = cgi->getExprTypeOfBuiltin(strtype);
			if(ret) return ret;

			// not so lucky
			std::deque<std::string> ns = cgi->unwrapNamespacedType(strtype);
			std::string atype = ns.back();
			ns.pop_back();


			if(TypePair_t* test = cgi->getType(Identifier(atype, ns, IdKind::Struct)))
			{
				iceAssert(test->first);
				return test->first;
			}



			auto pair = cgi->findTypeInFuncTree(ns, atype);
			TypePair_t* tp = pair.first;
			int indirections = pair.second;

			iceAssert(indirections == -1 || indirections == 0);

			// std::unordered_map<std::string, fir::Type*> instantiatedGenericTypes;
			if(indirections == -1)
			{
				// try generic.
				fir::Type* ret = cgi->resolveGenericType(atype);
				if(ret) return ret;

				if(atype.find("<") != std::string::npos)
				{
					error("enotsup (generic structs)");
				}

				std::string nsstr;
				for(auto n : ns)
					nsstr += n + ".";

				// if(ns.size() > 0) nsstr = nsstr.substr(1);

				if(allowFail) return 0;
				GenError::unknownSymbol(cgi, user, atype + " in namespace " + nsstr, SymbolType::Type);
			}

			if(!tp && cgi->getExprTypeOfBuiltin(atype))
			{
				return cgi->getExprTypeOfBuiltin(atype);
			}
			else if(tp)
			{
				// foundType:

				StructBase* oldSB = dynamic_cast<StructBase*>(tp->second.first);
				tp = cgi->findTypeInFuncTree(ns, oldSB->ident.name).first;

				fir::Type* concrete = tp ? tp->first : 0;
				if(!concrete)
				{
					// generate the type.
					iceAssert(oldSB);


					// temporarily hijack the main scope
					auto old = cgi->namespaceStack;
					cgi->namespaceStack = ns;

					concrete = oldSB->createType(cgi);
					// fprintf(stderr, "on-demand codegen of %s\n", oldSB->name.c_str());

					// do recursively, to ensure it's found...???
					// concrete = cgi->getExprTypeFromStringType(user, type, allowFail);
					// concrete = cgi->getTypeFromParserType(user, pt, allowFail);

					iceAssert(concrete);

					cgi->namespaceStack = old;

					if(!tp) tp = cgi->findTypeInFuncTree(ns, oldSB->ident.name).first;
					iceAssert(tp);
				}

				fir::Type* ret = tp->first;
				while(indirections > 0)
				{
					ret = ret->getPointerTo();
					indirections--;
				}

				return ret;
			}
			else if(!allowFail)
			{
				error(user, "Unknown type '%s'", strtype.c_str());
			}
			else
			{
				return 0;
			}
		}
		else
		{
			iceAssert(0);
		}
	}

	fir::Type* CodegenInstance::getTypeFromParserType(Expr* user, pts::Type* type, bool allowFail)
	{
		// pts basically did all the dirty work for us, so...
		return _recursivelyConvertType(this, allowFail, user, type);
	}






















	bool CodegenInstance::isArrayType(Expr* e)
	{
		iceAssert(e);
		fir::Type* ltype = this->getExprType(e);
		return ltype && ltype->isArrayType();
	}

	bool CodegenInstance::isIntegerType(Expr* e)
	{
		iceAssert(e);
		fir::Type* ltype = this->getExprType(e);
		return ltype && ltype->isIntegerType();
	}

	bool CodegenInstance::isSignedType(Expr* e)
	{
		return false;	// TODO: something about this
	}

	bool CodegenInstance::isAnyType(fir::Type* type)
	{
		if(type->isStructType())
		{
			if(type->toStructType()->getStructName().str() == "Any")
			{
				return true;
			}

			TypePair_t* pair = this->getTypeByString("Any");
			if(!pair) return false;
			// iceAssert(pair);

			if(pair->first == type)
				return true;
		}

		return false;
	}

	bool CodegenInstance::isEnum(fir::Type* type)
	{
		if(!type) return false;

		bool res = true;
		if(!type->isStructType())								res = false;
		if(res && type->toStructType()->getElementCount() != 1)	res = false;

		if(!res) return false;

		TypePair_t* tp = 0;
		if((tp = this->getType(type)))
			return tp->second.second == TypeKind::Enum;

		return res;
	}

	bool CodegenInstance::isTypeAlias(fir::Type* type)
	{
		if(!type) return false;

		bool res = true;
		if(!type->isStructType())								res = false;
		if(res && type->toStructType()->getElementCount() != 1)	res = false;

		if(!res) return false;

		TypePair_t* tp = 0;
		if((tp = this->getType(type)))
			return tp->second.second == TypeKind::TypeAlias;

		return res;
	}

	bool CodegenInstance::isBuiltinType(fir::Type* ltype)
	{
		bool ret = (ltype && (ltype->isIntegerType() || ltype->isFloatingPointType()));
		if(!ret)
		{
			while(ltype->isPointerType())
				ltype = ltype->getPointerElementType();

			ret = (ltype && (ltype->isIntegerType() || ltype->isFloatingPointType()));
		}

		return ret;
	}

	bool CodegenInstance::isBuiltinType(Expr* expr)
	{
		fir::Type* ltype = this->getExprType(expr);
		return this->isBuiltinType(ltype);
	}




	std::string CodegenInstance::printAst(Expr* expr)
	{
		if(expr == 0) return "(null)";

		if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
		{
			return "(" + this->printAst(ma->left) + "." + this->printAst(ma->right) + ")";
		}
		else if(FuncCall* fc = dynamic_cast<FuncCall*>(expr))
		{
			std::string ret = fc->name + "(";

			for(auto p : fc->params)
				ret += this->printAst(p) + ", ";

			if(fc->params.size() > 0)
				ret = ret.substr(0, ret.length() - 2);


			ret += ")";
			return ret;
		}
		else if(FuncDecl* fd = dynamic_cast<FuncDecl*>(expr))
		{
			std::string str = "func " + fd->ident.name;
			if(fd->genericTypes.size() > 0)
			{
				str += "<";
				for(auto t : fd->genericTypes)
					str += t.first + ", ";

				str = str.substr(0, str.length() - 2);
				str += ">";
			}

			str += "(";
			for(auto p : fd->params)
			{
				str += p->ident.name + ": " + (p->inferredLType ? this->getReadableType(p->inferredLType) : p->ptype->str()) + ", ";
			}

			if(fd->isCStyleVarArg) str += "..., ";

			if(fd->params.size() > 0)
				str = str.substr(0, str.length() - 2);

			str +=  ") -> " + fd->ptype->str();
			return str;
		}
		else if(VarRef* vr = dynamic_cast<VarRef*>(expr))
		{
			return vr->name;
		}
		else if(VarDecl* vd = dynamic_cast<VarDecl*>(expr))
		{
			return (vd->immutable ? ("val ") : ("var ")) + vd->ident.name + ": "
				+ (vd->inferredLType ? this->getReadableType(vd) : vd->ptype->str());
		}
		else if(BinOp* bo = dynamic_cast<BinOp*>(expr))
		{
			return "(" + this->printAst(bo->left) + " " + Parser::arithmeticOpToString(this, bo->op) + " " + this->printAst(bo->right) + ")";
		}
		else if(UnaryOp* uo = dynamic_cast<UnaryOp*>(expr))
		{
			return "(" + Parser::arithmeticOpToString(this, uo->op) + this->printAst(uo->expr) + ")";
		}
		else if(Number* n = dynamic_cast<Number*>(expr))
		{
			return n->decimal ? std::to_string(n->dval) : std::to_string(n->ival);
		}
		else if(ArrayLiteral* al = dynamic_cast<ArrayLiteral*>(expr))
		{
			std::string s = "[ ";
			for(auto v : al->values)
				s += this->printAst(v) + ", ";

			s = s.substr(0, s.length() - 2);
			s += " ]";

			return s;
		}
		else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(expr))
		{
			return this->printAst(ai->arr) + "[" + this->printAst(ai->index) + "]";
		}
		else if(Func* fn = dynamic_cast<Func*>(expr))
		{
			return this->printAst(fn->decl) + "\n" + this->printAst(fn->block);
		}
		else if(BracedBlock* blk = dynamic_cast<BracedBlock*>(expr))
		{
			std::string ret = "{\n";
			for(auto e : blk->statements)
				ret += "\t" + this->printAst(e) + "\n";

			for(auto d : blk->deferredStatements)
				ret += "\tdefer " + this->printAst(d->expr) + "\n";

			ret += "}";
			return ret;
		}
		else if(StringLiteral* sl = dynamic_cast<StringLiteral*>(expr))
		{
			std::string ret = "\"" + sl->str + "\"";
			return ret;
		}
		else if(BoolVal* bv = dynamic_cast<BoolVal*>(expr))
		{
			return bv->val ? "true" : "false";
		}
		else if(Tuple* tp = dynamic_cast<Tuple*>(expr))
		{
			std::string ret = "(";
			for(auto el : tp->values)
				ret += this->printAst(el) + ", ";

			if(tp->values.size() > 0)
				ret = ret.substr(0, ret.size() - 2);

			ret += ")";
			return ret;
		}
		else if(dynamic_cast<DummyExpr*>(expr))
		{
			return "";
		}
		else if(Typeof* to = dynamic_cast<Typeof*>(expr))
		{
			return "typeof(" + this->printAst(to->inside) + ")";
		}
		else if(ForeignFuncDecl* ffi = dynamic_cast<ForeignFuncDecl*>(expr))
		{
			return "ffi " + this->printAst(ffi->decl);
		}
		else if(Import* imp = dynamic_cast<Import*>(expr))
		{
			return "import " + imp->module;
		}
		else if(dynamic_cast<Root*>(expr))
		{
			return "(root)";
		}
		else if(Return* ret = dynamic_cast<Return*>(expr))
		{
			return "return " + this->printAst(ret->val);
		}
		else if(WhileLoop* wl = dynamic_cast<WhileLoop*>(expr))
		{
			if(wl->isDoWhileVariant)
			{
				return "do {\n" + this->printAst(wl->body) + "\n} while(" + this->printAst(wl->cond) + ")";
			}
			else
			{
				return "while(" + this->printAst(wl->cond) + ")\n{\n" + this->printAst(wl->body) + "\n}\n";
			}
		}
		else if(IfStmt* ifst = dynamic_cast<IfStmt*>(expr))
		{
			bool first = false;
			std::string final;
			for(auto c : ifst->cases)
			{
				std::string one;

				if(!first)
					one = "else ";

				first = false;
				one += "if(" + this->printAst(c.first) + ")" + "\n{\n" + this->printAst(c.second) + "\n}\n";

				final += one;
			}

			if(ifst->final)
				final += "else\n{\n" + this->printAst(ifst->final) + " \n}\n";

			return final;
		}
		else if(ClassDef* cls = dynamic_cast<ClassDef*>(expr))
		{
			std::string s;
			s = "class " + cls->ident.name + "\n{\n";

			for(auto m : cls->members)
				s += this->printAst(m) + "\n";

			for(auto f : cls->funcs)
				s += this->printAst(f) + "\n";

			s += "\n}";
			return s;
		}
		else if(StructDef* str = dynamic_cast<StructDef*>(expr))
		{
			std::string s;
			s = "struct " + str->ident.name + "\n{\n";

			for(auto m : str->members)
				s += this->printAst(m) + "\n";

			s += "\n}";
			return s;
		}
		else if(EnumDef* enr = dynamic_cast<EnumDef*>(expr))
		{
			std::string s;
			s = "enum " + enr->ident.name + "\n{\n";

			for(auto m : enr->cases)
				s += m.first + " " + this->printAst(m.second) + "\n";

			s += "\n}";
			return s;
		}
		else if(NamespaceDecl* nd = dynamic_cast<NamespaceDecl*>(expr))
		{
			return "namespace " + nd->name + "\n{\n" + this->printAst(nd->innards) + "\n}";
		}
		else if(Dealloc* da = dynamic_cast<Dealloc*>(expr))
		{
			return "dealloc " + this->printAst(da->expr);
		}
		else if(Alloc* al = dynamic_cast<Alloc*>(expr))
		{
			std::string ret = "alloc";
			for(auto c : al->counts)
				ret += "[" + this->printAst(c) + "]";

			return ret + al->ptype->str();
		}

		error(expr, "Unknown shit (%s)", typeid(*expr).name());
	}
}































