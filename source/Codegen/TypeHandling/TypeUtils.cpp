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

		fir::StructType* bst = baseType->toStructType();

		if(bst && (this->isEnum(baseType) || this->isTypeAlias(baseType)))
		{
			TypePair_t* tp = this->getType(baseType);
			if(!tp)
				error(user, "Invalid type '%s'!", bst->getStructName().str().c_str());

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
		if(expr->type.ftype != 0)
			return expr->type.ftype;

		setInferred = false;
		iceAssert(expr);
		{
			if(VarDecl* decl = dynamic_cast<VarDecl*>(expr))
			{
				if(decl->type.strType == "Inferred")
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

					fir::Type* ret = this->parseAndGetOrInstantiateType(decl, decl->type.strType, allowFail);
					if(setInferred) decl->inferredLType = ret;

					return ret;
				}
			}
			else if(VarRef* ref = dynamic_cast<VarRef*>(expr))
			{
				VarDecl* decl = getSymDecl(ref, ref->name);
				if(!decl)
				{
					error(expr, "(%s:%d) -> Internal check failed: invalid var ref to '%s'", __FILE__, __LINE__, ref->name.c_str());
				}

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
							fir::Function* genericMaybe = this->tryResolveAndInstantiateGenericFunction(fc);
							if(genericMaybe)
								return genericMaybe->getReturnType();

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
				TypePair_t* type = this->getTypeByString(fd->type.strType);
				if(!type)
				{
					fir::Type* ret = this->parseAndGetOrInstantiateType(fd, fd->type.strType, allowFail);
					return ret;
				}

				return type->first;
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

				fir::StructType* st = dynamic_cast<fir::StructType*>(lhs);

				if(!pair && (!st || !st->isLiteralStruct()))
					error(expr, "Invalid type '%s' for dot-operator-access", this->getReadableType(lhs).c_str());


				if((st && st->isLiteralStruct()) || (pair->second.second == TypeKind::Tuple))
				{
					// values are 1, 2, 3 etc.
					// for now, assert this.

					Number* n = dynamic_cast<Number*>(ma->right);
					iceAssert(n);

					fir::Type* ttype = pair ? pair->first : st;
					iceAssert(ttype->isStructType());

					if((size_t) n->ival >= ttype->toStructType()->getElementCount())
					{
						error(expr, "Tuple does not have %d elements, only %zd", (int) n->ival + 1,
							ttype->toStructType()->getElementCount());
					}

					return ttype->toStructType()->getElementN(n->ival);
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
								return this->getExprTypeFromStringType(c, c->type, allowFail);
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
				fir::Type* base = this->getExprTypeFromStringType(alloc, alloc->type.strType);

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
				if(dum->type.isLiteral)
				{
					return this->parseAndGetOrInstantiateType(expr, dum->type.strType);
				}
				else
				{
					return this->getExprType(dum->type.type);
				}
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
				fir::Type* tp = tup->cachedLlvmType;
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
			&& from->isStructType() && from->toStructType()->getStructName().str() == "String")
		{
			return 2;
		}
		else if(from->isPointerType() && from->getPointerElementType() == fir::PrimitiveType::getInt8(this->getContext())
			&& to->isStructType() && to->toStructType()->getStructName().str() == "String")
		{
			return 2;
		}
		else if(to->isFloatingPointType() && from->isIntegerType())
		{
			// int-to-float is 10.
			return 10;
		}
		else if(to->isStructType() && from->isStructType() && to->toStructType()->isABaseTypeOf(from->toStructType()))
		{
			return 20;
		}
		else if(to->isPointerType() && from->isNullPointer())
		{
			return 5;
		}
		else if(to->isPointerType() && from->isPointerType() && from->getPointerElementType()->toStructType()
				&& to->getPointerElementType()->toStructType() && to->getPointerElementType()->toStructType()
						->isABaseTypeOf(from->getPointerElementType()->toStructType()))
		{
			return 20;
		}
		else if(this->isAnyType(to))
		{
			// any cast is 25.
			return 25;
		}
		else if(this->isTupleType(from) && this->isTupleType(to)
			&& from->toStructType()->getElementCount() == to->toStructType()->getElementCount())
		{
			int sum = 0;
			for(size_t i = 0; i < from->toStructType()->getElementCount(); i++)
			{
				int d = this->getAutoCastDistance(from->toStructType()->getElementN(i), to->toStructType()->getElementN(i));
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
				&& from->getType()->isStructType() && from->getType()->toStructType()->getStructName().str() == "String")
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
		else if(target->isStructType() && from->getType()->isStructType()
			&& target->toStructType()->isABaseTypeOf(from->getType()->toStructType()))
		{
			fir::StructType* sto = target->toStructType();
			fir::StructType* sfr = from->getType()->toStructType();

			iceAssert(sto->isABaseTypeOf(sfr));

			// create alloca, which gets us a pointer.
			fir::Value* alloca = this->builder.CreateStackAlloc(sfr);

			// store the value into the pointer.
			this->builder.CreateStore(from, alloca);

			// do a pointer type cast.
			fir::Value* ptr = this->builder.CreatePointerTypeCast(alloca, sto->getPointerTo());

			// load it.
			retval = this->builder.CreateLoad(ptr);
		}
		else if(target->isPointerType() && from->getType()->isNullPointer())
		{
			retval = fir::ConstantValue::getNullValue(target);
			// fprintf(stderr, "void cast, %s (%zu) // %s (%zu)\n", target->str().c_str(), from->id, retval->getType()->str().c_str(), retval->id);
		}
		else if(target->isPointerType() && from->getType()->isPointerType() && target->getPointerElementType()->toStructType()
				&& target->getPointerElementType()->toStructType()->isABaseTypeOf(from->getType()->getPointerElementType()->toStructType()))
		{
			fir::StructType* sfr = from->getType()->getPointerElementType()->toStructType();
			fir::StructType* sto = target->getPointerElementType()->toStructType();

			iceAssert(sfr && sto && sto->isABaseTypeOf(sfr));
			retval = this->builder.CreatePointerTypeCast(from, sto->getPointerTo());
		}
		else if(this->isTupleType(from->getType()) && this->isTupleType(target)
			&& from->getType()->toStructType()->getElementCount() == target->toStructType()->getElementCount())
		{
			// somewhat complicated
			iceAssert(fromPtr);

			fir::Value* tuplePtr = this->getStackAlloc(target);
			// fprintf(stderr, "tuplePtr = %s\n", tuplePtr->getType()->str().c_str());
			// fprintf(stderr, "from = %s, to = %s\n", from->getType()->str().c_str(), target->str().c_str());

			for(size_t i = 0; i < from->getType()->toStructType()->getElementCount(); i++)
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











	std::string unwrapPointerType(std::string type, int* _indirections)
	{
		std::string sptr = "*";
		size_t ptrStrLength = sptr.length();

		int tmp = 0;
		if(!_indirections)
			_indirections = &tmp;

		std::string actualType = type;
		if(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
		{
			int& indirections = *_indirections;

			while(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
				actualType = actualType.substr(0, actualType.length() - ptrStrLength), indirections++;
		}

		return actualType;
	}

	static fir::Type* recursivelyParseTuple(CodegenInstance* cgi, Expr* user, std::string& str, bool allowFail)
	{
		iceAssert(str.length() > 0);
		iceAssert(str[0] == '(');

		str = str.substr(1);
		char front = str.front();
		if(front == ')')
			error(user, "Empty tuples are not supported");

		std::vector<fir::Type*> types;
		while(front != ')')
		{
			std::string cur;
			while(front != ',' && front != '(' && front != ')')
			{
				cur += front;

				str.erase(str.begin());
				front = str.front();
			}

			if(front == ',' || front == ')')
			{
				bool shouldBreak = (front == ')');
				fir::Type* ty = cgi->parseAndGetOrInstantiateType(user, cur, allowFail);
				iceAssert(ty);

				types.push_back(ty);

				str.erase(str.begin());
				front = str.front();

				if(shouldBreak)
					break;
			}
			else if(front == '(')
			{
				iceAssert(str.front() == '(');
				types.push_back(recursivelyParseTuple(cgi, user, str, allowFail));

				if(str.front() == ',')
					str.erase(str.begin());

				front = str.front();
			}
		}

		return fir::StructType::getLiteral(types, cgi->getContext());
	}



	fir::Type* CodegenInstance::parseAndGetOrInstantiateType(Expr* user, std::string type, bool allowFail)
	{
		if(type.length() > 0)
		{
			if(type[0] == '(')
			{
				// parse a tuple.
				fir::Type* parsed = recursivelyParseTuple(this, user, type, allowFail);
				return parsed;
			}
			else
			{
				int indirections = 0;

				std::string actualType = unwrapPointerType(type, &indirections);
				if(actualType.find("[") != std::string::npos)
				{
					size_t k = actualType.find("[");
					std::string base = actualType.substr(0, k);

					std::string arr = actualType.substr(k);
					fir::Type* btype = this->parseAndGetOrInstantiateType(user, base, allowFail);


					std::vector<int> sizes;
					while(arr.length() > 0 && arr.front() == '[')
					{
						arr = arr.substr(1);

						// get the size
						const char* c = arr.c_str();
						char* final = 0;


						if(arr.find("]") == 0)
						{
							// variable array.
							sizes.push_back(0);
							iceAssert(arr.find("]") == 1);

							arr = arr.substr(1);
						}
						else if(arr.find("...") == 0)
						{
							sizes.push_back(-1);
							iceAssert(arr.find("]") == 3);

							arr = arr.substr(3);

							if(arr.length() > 0 && arr.front() == '[')
								error(user, "Variadic array '[...]' must be the last dimension.");
						}
						else
						{
							size_t asize = strtoll(c, &final, 0);
							size_t numlen = final - c;

							arr = arr.substr(numlen);
							sizes.push_back(asize);


							// get the closing.
							iceAssert(arr.length() > 0 && arr.front() == ']');
							arr = arr.substr(1);
						}
					}

					for(auto i : sizes)
					{
						if(i > 0)
						{
							btype = fir::ArrayType::get(btype, i);
						}
						else if(i == -1)
						{
							btype = fir::LLVariableArrayType::get(btype);
						}
						else
						{
							btype = btype->getPointerTo();
						}
					}

					return btype;
				}
				else
				{
					fir::Type* ret = this->getExprTypeFromStringType(user, ExprType(actualType), allowFail);
					if(ret)
					{
						while(indirections > 0)
						{
							ret = ret->getPointerTo();
							indirections--;
						}
					}

					return ret;
				}
			}
		}
		else
		{
			return nullptr;
		}
	}

	fir::Type* CodegenInstance::getExprTypeFromStringType(Ast::Expr* user, ExprType type, bool allowFail)
	{
		if(type.isLiteral)
		{
			fir::Type* ret = this->getExprTypeOfBuiltin(type.strType);
			if(ret) return ret;

			// not so lucky
			std::deque<std::string> ns = this->unwrapNamespacedType(type.strType);
			std::string atype = ns.back();
			ns.pop_back();

			auto pair = this->findTypeInFuncTree(ns, atype);
			TypePair_t* tp = pair.first;
			int indirections = pair.second;

			std::map<std::string, fir::Type*> instantiatedGenericTypes;
			if(indirections == -1)
			{
				// try generic.
				fir::Type* ret = this->resolveGenericType(type.strType);
				if(ret) return ret;

				if(atype.find("<") != std::string::npos)
				{
					error("enotsup (generic structs)");


					#if 0

					size_t k = atype.find("<");
					std::string base = atype.substr(0, k);

					pair = this->findTypeInFuncTree(ns, base);
					tp = pair.first;
					indirections = pair.second;

					if(tp && indirections >= 0)
					{
						// parse that shit.
						StructBase* sb = dynamic_cast<StructBase*>(tp->second.first);
						iceAssert(sb);

						// parse the list of types.
						std::string glist = atype.substr(k);
						{
							iceAssert(glist.size() > 0 && glist[0] == '<');
							glist = glist.substr(1);

							iceAssert(glist.back() == '>');
							glist.pop_back();


							// to allow for nesting generic types in generic types,
							// eg. Foo<Bar<Int>,Qux<Double,Int>>, we need to be smart
							// iterate through the string manually.
							// if we encounter a '<', increase nesting.
							// if we encounter a '>', decrease nesting.
							// if we encounter a ',' while nesting > 0, ignore.
							// if we encounter a ',' while nesting == 0, split the string there.

							int nesting = 0;
							std::deque<std::string> types;

							std::string curtype;
							for(size_t i = 0; i < glist.length(); i++)
							{
								if(glist[i] == '<')
								{
									nesting++;
									curtype += glist[i];
								}
								else if(glist[i] == '>')
								{
									if(nesting == 0) error(user, "mismatched angle brackets in generic type parameter list");
									nesting--;
									curtype += glist[i];
								}
								else if(glist[i] == ',' && nesting == 0)
								{
									types.push_back(curtype);
									curtype = "";
								}
								else
								{
									curtype += glist[i];
								}
							}
							types.push_back(curtype);
						}

						// todo: ew, goto
						goto foundType;
					}
					else
					{
						if(allowFail) return 0;
						else error(user, "Invaild type '%s'", base.c_str());
					}

					#endif
				}

				std::string nsstr;
				for(auto n : ns)
					nsstr += n + ".";

				// if(ns.size() > 0) nsstr = nsstr.substr(1);

				if(allowFail) return 0;
				GenError::unknownSymbol(this, user, atype + " in namespace " + nsstr, SymbolType::Type);
			}

			if(!tp && this->getExprTypeOfBuiltin(atype))
			{
				return this->getExprTypeOfBuiltin(atype);
			}
			else if(tp)
			{
				// foundType:

				StructBase* oldSB = dynamic_cast<StructBase*>(tp->second.first);
				tp = this->findTypeInFuncTree(ns, oldSB->ident.name).first;

				fir::Type* concrete = tp ? tp->first : 0;
				if(!concrete)
				{
					// generate the type.
					iceAssert(oldSB);


					// temporarily hijack the main scope
					auto old = this->namespaceStack;
					this->namespaceStack = ns;

					concrete = oldSB->createType(this, instantiatedGenericTypes);
					// fprintf(stderr, "on-demand codegen of %s\n", oldSB->name.c_str());

					concrete = getExprTypeFromStringType(user, type, allowFail);
					iceAssert(concrete);

					if(instantiatedGenericTypes.size() > 0)
					{
						this->pushGenericTypeStack();
						for(auto t : instantiatedGenericTypes)
							this->pushGenericType(t.first, t.second);
					}

					if(instantiatedGenericTypes.size() > 0)
						this->popGenericTypeStack();

					this->namespaceStack = old;


					if(!tp) tp = this->findTypeInFuncTree(ns, oldSB->ident.name).first;
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
				error(user, "Unknown type '%s'", type.strType.c_str());
			}
			else
			{
				return 0;
			}
		}
		else
		{
			error(user, "enotsup");
		}
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

	bool CodegenInstance::isPtr(Expr* expr)
	{
		fir::Type* ltype = this->getExprType(expr);
		return ltype && ltype->isPointerType();
	}

	bool CodegenInstance::isAnyType(fir::Type* type)
	{
		if(type->isStructType())
		{
			if(!type->toStructType()->isLiteralStruct() && type->toStructType()->getStructName().str() == "Any")
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

	bool CodegenInstance::isEnum(ExprType type)
	{
		if(type.isLiteral)
		{
			TypePair_t* tp = 0;
			if((tp = this->getType(Identifier(type.strType, { }, IdKind::Struct))))
			{
				if(tp->second.second == TypeKind::Enum)
					return true;
			}

			return false;
		}
		else
		{
			error("enotsup");
		}
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

	bool CodegenInstance::isTypeAlias(ExprType type)
	{
		if(type.isLiteral)
		{
			TypePair_t* tp = 0;
			if((tp = this->getType(Identifier(type.strType, { }, IdKind::Struct))))
			{
				if(tp->second.second == TypeKind::TypeAlias)
					return true;
			}

			return false;
		}
		else
		{
			error("enotsup");
		}
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
		return (ltype && (ltype->isIntegerType() || ltype->isFloatingPointType()));
	}

	bool CodegenInstance::isBuiltinType(Expr* expr)
	{
		fir::Type* ltype = this->getExprType(expr);
		return this->isBuiltinType(ltype);
	}

	bool CodegenInstance::isTupleType(fir::Type* type)
	{
		return type->isStructType() && type->isLiteralStruct();
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
			std::string str = "ƒ " + fd->ident.name + "(";
			for(auto p : fd->params)
			{
				str += p->ident.name + ": " + (p->inferredLType ? this->getReadableType(p->inferredLType) : p->type.strType) + ", ";
			}

			if(fd->isCStyleVarArg) str += "..., ";

			if(fd->params.size() > 0)
				str = str.substr(0, str.length() - 2);

			str +=  ") -> " + fd->type.strType;
			return str;
		}
		else if(VarRef* vr = dynamic_cast<VarRef*>(expr))
		{
			return vr->name;
		}
		else if(VarDecl* vd = dynamic_cast<VarDecl*>(expr))
		{
			return (vd->immutable ? ("val ") : ("var ")) + vd->ident.name + ": "
				+ (vd->inferredLType ? this->getReadableType(vd) : vd->type.strType);
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

			return ret + al->type.strType;
		}

		error(expr, "Unknown shit (%s)", typeid(*expr).name());
	}
}































