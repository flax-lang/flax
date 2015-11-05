// TypeUtils.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	fir::Type* CodegenInstance::getExprTypeOfBuiltin(std::string type)
	{
		int indirections = 0;
		type = unwrapPointerType(type, &indirections);

		if(!Compiler::getDisableLowercaseBuiltinTypes())
		{
			if(type.length() > 0)
			{
				type[0] = toupper(type[0]);
			}
		}

		fir::Type* real = 0;

		if(type == "Int8")			real = fir::PrimitiveType::getInt8(this->getContext());
		else if(type == "Int16")	real = fir::PrimitiveType::getInt16(this->getContext());
		else if(type == "Int32")	real = fir::PrimitiveType::getInt32(this->getContext());
		else if(type == "Int64")	real = fir::PrimitiveType::getInt64(this->getContext());
		else if(type == "Int")		real = fir::PrimitiveType::getInt64(this->getContext());

		else if(type == "Uint8")	real = fir::PrimitiveType::getUint8(this->getContext());
		else if(type == "Uint16")	real = fir::PrimitiveType::getUint16(this->getContext());
		else if(type == "Uint32")	real = fir::PrimitiveType::getUint32(this->getContext());
		else if(type == "Uint64")	real = fir::PrimitiveType::getUint64(this->getContext());
		else if(type == "Uint")		real = fir::PrimitiveType::getUint64(this->getContext());

		else if(type == "Float32")	real = fir::PrimitiveType::getFloat32(this->getContext());
		else if(type == "Float")	real = fir::PrimitiveType::getFloat32(this->getContext());

		else if(type == "Float64")	real = fir::PrimitiveType::getFloat64(this->getContext());
		else if(type == "Double")	real = fir::PrimitiveType::getFloat64(this->getContext());

		else if(type == "Bool")		real = fir::PrimitiveType::getBool(this->getContext());
		else if(type == "Void")		real = fir::PrimitiveType::getVoid(this->getContext());
		else return 0;

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
				error(user, "Invalid type '%s'!", bst->getStructName().c_str());

			iceAssert(tp->second.second == TypeKind::Enum);
			Enumeration* enr = dynamic_cast<Enumeration*>(tp->second.first);

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
		setInferred = false;
		iceAssert(expr);
		{
			if(VarDecl* decl = dynamic_cast<VarDecl*>(expr))
			{
				if(decl->type.strType == "Inferred")
				{
					if(!decl->inferredLType)		// todo: better error detection for this
					{
						error(expr, "Invalid variable declaration for %s!", decl->name.c_str());
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
						TypePair_t* tp = this->getType(fc->name);
						if(tp)
						{
							return tp->first;
						}
						else
						{
							fir::Function* genericMaybe = this->tryResolveAndInstantiateGenericFunction(fc);
							if(genericMaybe)
								return genericMaybe->getReturnType();

							GenError::unknownSymbol(this, expr, fc->name.c_str(), SymbolType::Function);
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
				TypePair_t* type = this->getType(fd->type.strType);
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
					auto tp = this->getType("String");
					if(!tp)
						return fir::PointerType::getInt8Ptr(this->getContext());


					return tp->first;
				}
			}
			else if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
			{
				if(ma->matype == MAType::LeftNamespace || ma->matype == MAType::LeftTypename)
					return this->resolveStaticDotOperator(ma, false).first;


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
					Class* cls = dynamic_cast<Class*>(pair->second.first);
					iceAssert(cls);

					VarRef* memberVr = dynamic_cast<VarRef*>(ma->right);
					FuncCall* memberFc = dynamic_cast<FuncCall*>(ma->right);

					if(memberVr)
					{
						for(VarDecl* mem : cls->members)
						{
							if(mem->name == memberVr->name)
								return this->getExprType(mem);
						}
						for(ComputedProperty* c : cls->cprops)
						{
							if(c->name == memberVr->name)
								return this->getExprTypeFromStringType(c, c->type, allowFail);
						}
					}
					else if(memberFc)
					{
						return this->getExprType(this->getFunctionFromMemberFuncCall(cls, memberFc));
					}
				}
				else if(pair->second.second == TypeKind::Struct)
				{
					Struct* str = dynamic_cast<Struct*>(pair->second.first);
					iceAssert(str);

					VarRef* memberVr = dynamic_cast<VarRef*>(ma->right);
					FuncCall* memberFc = dynamic_cast<FuncCall*>(ma->right);

					if(memberVr)
					{
						for(VarDecl* mem : str->members)
						{
							if(mem->name == memberVr->name)
								return this->getExprType(mem);
						}
					}
					else if(memberFc)
					{
						error(memberFc, "Tried to call method on struct");
					}
				}
				else if(pair->second.second == TypeKind::Enum)
				{
					Enumeration* enr = dynamic_cast<Enumeration*>(pair->second.first);
					iceAssert(enr);

					VarRef* enrcase = dynamic_cast<VarRef*>(ma->right);
					iceAssert(enrcase);

					for(auto c : enr->cases)
					{
						if(c.first == enrcase->name)
							return this->getExprType(c.second);
					}

					error(expr, "Enum '%s' has no such case '%s'", enr->name.c_str(), enrcase->name.c_str());
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
					return std::get<6>(this->getOperatorOverload(bo, bo->op, ltype, rtype))->getReturnType();
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

						auto opfn = std::get<6>(this->getOperatorOverload(bo, bo->op, ltype, rtype));
						if(opfn) return opfn->getReturnType();

						error(expr, "??? // (%s %s %s)", this->getReadableType(ltype).c_str(),
							Parser::arithmeticOpToString(this, bo->op).c_str(), this->getReadableType(rtype).c_str());
					}
				}
			}
			else if(Alloc* alloc = dynamic_cast<Alloc*>(expr))
			{
				TypePair_t* type = getType(alloc->type.strType);
				if(!type)
				{
					// check if it ends with pointer, and if we have a type that's un-pointered
					if(alloc->type.strType.find("::") != std::string::npos)
					{
						alloc->type.strType = this->mangleRawNamespace(alloc->type.strType);
						return this->getExprTypeFromStringType(alloc, alloc->type, allowFail)->getPointerTo();
					}

					return this->parseAndGetOrInstantiateType(alloc, alloc->type.strType)->getPointerTo();
				}

				fir::Type* ret = type->first;
				if(alloc->counts.size() == 0) return ret->getPointerTo();

				for(size_t i = 0; i < alloc->counts.size(); i++)
					 ret = ret->getPointerTo();

				return ret;
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
				TypePair_t* tp = this->getType("Type");
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
				if(!t->isArrayType() && !t->isPointerType())
					error(expr, "Not array or pointer type: %s", t->str().c_str());

				if(t->isPointerType()) return t->getPointerElementType();
				else return t->toArrayType()->getElementType();
			}
			else if(ArrayLiteral* al = dynamic_cast<ArrayLiteral*>(expr))
			{
				// todo: make this not shit.
				return fir::ArrayType::get(this->getExprType(al->values.front()), al->values.size());
			}
			else if(PostfixUnaryOp* puo = dynamic_cast<PostfixUnaryOp*>(expr))
			{
				fir::Type* targtype = this->getExprType(puo->expr);
				iceAssert(targtype);

				if(puo->kind == PostfixUnaryOp::Kind::ArrayIndex)
				{
					if(targtype->isPointerType())
						return targtype->getPointerElementType();

					else if(targtype->isArrayType())
						return targtype->toArrayType()->getElementType();

					else
						error(expr, "Invalid???");
				}
				else
				{
					iceAssert(0);
				}
			}
		}

		error(expr, "(%s:%d) -> Internal check failed: failed to determine type '%s'", __FILE__, __LINE__, typeid(*expr).name());
	}

	fir::Value* CodegenInstance::allocateInstanceInBlock(fir::Type* type, std::string name)
	{
		return this->builder.CreateStackAlloc(type, name);
	}

	fir::Value* CodegenInstance::allocateInstanceInBlock(VarDecl* var)
	{
		return allocateInstanceInBlock(this->getExprType(var), var->name);
	}


	fir::Value* CodegenInstance::getDefaultValue(Expr* e)
	{
		return fir::ConstantValue::getNullValue(this->getExprType(e));
	}

	fir::Function* CodegenInstance::getDefaultConstructor(Expr* user, fir::Type* ptrType, StructBase* sb)
	{
		// check if we have a default constructor.

		if(Class* cls = dynamic_cast<Class*>(sb))
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
				error(user, "Struct %s has no default initialiser taking 0 parameters", cls->name.c_str());

			return candidate;
		}
		else if(Struct* str = dynamic_cast<Struct*>(sb))
		{
			// should be the front one.
			iceAssert(str->initFuncs.size() > 0);
			return str->initFuncs[0];
		}
		else
		{
			error(user, "Type '%s' cannot have initialisers", sb->name.c_str());
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
			if(ab > bb) return -1;

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
			&& from->isStructType() && from->toStructType()->getStructName() == this->mangleWithNamespace("String", { }, false))
		{
			return 2;
		}
		else if(from->isPointerType() && from->getPointerElementType() == fir::PrimitiveType::getInt8(this->getContext())
			&& to->isStructType() && to->toStructType()->getStructName() == this->mangleWithNamespace("String", { }, false))
		{
			return 2;
		}
		else if(to->isFloatingPointType() && from->isIntegerType())
		{
			// int-to-float is 10.
			return 10;
		}
		else if(to->isStructType() && from->isStructType())
		{
			fir::StructType* sto = to->toStructType();
			fir::StructType* sfr = from->toStructType();

			if(sto->isABaseTypeOf(sfr))
			{
				return 20;
			}
		}
		else if(to->isPointerType() && from->isPointerType())
		{
			fir::StructType* sfr = from->getPointerElementType()->toStructType();
			fir::StructType* sto = to->getPointerElementType()->toStructType();

			if(sfr && sto && sto->isABaseTypeOf(sfr))
			{
				return 20;
			}
		}

		return -1;
	}

	fir::Value* CodegenInstance::autoCastType(fir::Type* target, fir::Value* right, fir::Value* rhsPtr, int* distance)
	{
		if(!target || !right)
			return right;

		// casting distance for size is determined by the number of "jumps"
		// 8 -> 16 = 1
		// 8 -> 32 = 2
		// 8 -> 64 = 3
		// 16 -> 64 = 2
		// etc.

		fir::Value* retval = right;

		int dist = -1;
		if(target->isIntegerType() && right->getType()->isIntegerType()
			&& target->toPrimitiveType()->getIntegerBitWidth() != right->getType()->toPrimitiveType()->getIntegerBitWidth())
		{
			unsigned int lBits = target->toPrimitiveType()->getIntegerBitWidth();
			unsigned int rBits = right->getType()->toPrimitiveType()->getIntegerBitWidth();

			bool shouldCast = lBits > rBits;
			// check if the RHS is a constant value
			fir::ConstantInt* constVal = dynamic_cast<fir::ConstantInt*>(right);
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
				retval = this->builder.CreateIntSizeCast(right, target);
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
		else if(target->isIntegerType() && right->getType()->isIntegerType()
			&& (right->getType()->toPrimitiveType()->isSigned() != target->toPrimitiveType()->isSigned()))
		{
			if(!right->getType()->toPrimitiveType()->isSigned() && target->toPrimitiveType()->isSigned())
			{
				// only do it if it preserves all data.
				// we cannot convert signed to unsigned, but unsigned to signed is okay if the
				// signed bitwidth > unsigned bitwidth
				// eg. u32 -> i32 >> not okay
				//     u32 -> i64 >> okay

				if(target->toPrimitiveType()->getIntegerBitWidth() > right->getType()->toPrimitiveType()->getIntegerBitWidth())
					retval = this->builder.CreateIntSizeCast(right, target);
			}
			else
			{
				// we can't "normally" do it without losing data
				// but if the rhs is a constant, maybe we can.

				if(fir::ConstantInt* cright = dynamic_cast<fir::ConstantInt*>(right))
				{
					// only if not negative, can we convert to unsigned.
					if(cright->getSignedValue() >= 0)
					{
						// get bits of lhs
						size_t lbits = target->toPrimitiveType()->getIntegerBitWidth();

						// get max value.
						size_t maxL = pow(2, lbits) - 1;
						if(lbits == 64) maxL = UINT64_MAX;

						if(cright->getUnsignedValue() <= maxL)
						{
							retval = this->builder.CreateIntSizeCast(right, target);
						}
					}
				}
			}
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(target->isPointerType() && target->getPointerElementType() == fir::PrimitiveType::getInt8(this->getContext()))
		{
			fir::Type* rtype = right->getType();
			if(rtype->isStructType()
				&& rtype->toStructType()->getStructName() == this->mangleWithNamespace("String", std::deque<std::string>()))
			{
				// get the struct gep:
				// Layout of string:
				// var data: Int8*
				// var allocated: Uint64

				// cast the RHS to the LHS
				iceAssert(rhsPtr);
				fir::Value* ret = this->builder.CreateStructGEP(rhsPtr, 0);
				retval = this->builder.CreateLoad(ret);
			}
		}
		else if(target->isFloatingPointType() && right->getType()->isIntegerType())
		{
			// int-to-float is 10.
			retval = this->builder.CreateIntToFloatCast(right, target);
		}
		else if(target->isStructType() && right->getType()->isStructType())
		{
			fir::StructType* sto = target->toStructType();
			fir::StructType* sfr = right->getType()->toStructType();

			if(sto->isABaseTypeOf(sfr))
			{
				// create alloca, which gets us a pointer.
				fir::Value* alloca = this->builder.CreateStackAlloc(sfr);

				// store the value into the pointer.
				this->builder.CreateStore(right, alloca);

				// do a pointer type cast.
				fir::Value* ptr = this->builder.CreatePointerTypeCast(alloca, sto->getPointerTo());

				// load it.
				retval = this->builder.CreateLoad(ptr);
			}
		}
		else if(target->isPointerType() && right->getType()->isPointerType())
		{
			fir::StructType* sfr = right->getType()->getPointerElementType()->toStructType();
			fir::StructType* sto = target->getPointerElementType()->toStructType();

			if(sfr && sto && sto->isABaseTypeOf(sfr))
			{
				retval = this->builder.CreatePointerTypeCast(right, sto->getPointerTo());
			}
		}


		dist = this->getAutoCastDistance(right->getType(), target);
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
		std::string sptr = std::string("*");
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

	static fir::Type* recursivelyParseArray(CodegenInstance* cgi, Expr* user, std::string& type, bool allowFail)
	{
		iceAssert(type.size() > 0);

		fir::Type* ret = 0;
		if(type[0] != '[')
		{
			std::string t = "";
			while(type[0] != ']')
			{
				t += type[0];
				type.erase(type.begin());
			}

			ret = cgi->parseAndGetOrInstantiateType(user, t, allowFail);
		}
		else
		{
			type = type.substr(1);
			ret = recursivelyParseArray(cgi, user, type, allowFail);

			// todo: FIXME -- arrays, not pointers.
			ret = ret->getPointerTo();

			if(type[0] != ']')
				error(user, "Expected closing '['");
		}

		return ret;
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
			else if(type[0] == '[')
			{
				// array.
				std::string tp = type;
				fir::Type* parsed = recursivelyParseArray(this, user, tp, allowFail);

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

						size_t asize = strtoll(c, &final, 0);
						size_t numlen = final - c;

						arr = arr.substr(numlen);
						sizes.push_back(asize);


						// get the closing.
						iceAssert(arr.length() > 0 && arr.front() == ']');
						arr = arr.substr(1);
					}

					for(auto i : sizes)
					{
						btype = fir::ArrayType::get(btype, i);
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

						// todo: need to mangle name of struct, then find. currently fucks up.
						// todo: need some way to give less shitty error messages

						if(sb->genericTypes.size() == 0)
							error(user, "Type %s does not have type parameters, is not generic", sb->name.c_str());


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


							if(types.size() != sb->genericTypes.size())
								error(user, "Expected %zu type parameters, got %zu", sb->genericTypes.size(), types.size());

							// ok.
							for(size_t i = 0; i < types.size(); i++)
							{
								fir::Type* inst = this->parseAndGetOrInstantiateType(user, types[i]);
								instantiatedGenericTypes[sb->genericTypes[i]] = inst;
							}
						}

						// todo: ew, goto
						goto foundType;
					}
					else
					{
						if(allowFail) return 0;
						else error(user, "Invaild type '%s'", base.c_str());
					}
				}

				std::string nsstr;
				for(auto n : ns)
					nsstr += n + ".";

				if(ns.size() > 0) nsstr = nsstr.substr(1);

				if(allowFail) return 0;
				GenError::unknownSymbol(this, user, atype + " in namespace " + nsstr, SymbolType::Type);
			}


			if(!tp && this->getExprTypeOfBuiltin(atype))
			{
				return this->getExprTypeOfBuiltin(atype);
			}
			else if(tp)
			{
				foundType:

				StructBase* oldSB = dynamic_cast<StructBase*>(tp->second.first);
				std::string mangledGeneric = oldSB->name;

				// mangle properly, and find it.
				{
					iceAssert(tp);
					iceAssert(tp->second.first);

					for(auto pair : instantiatedGenericTypes)
						mangledGeneric += "_" + pair.first + ":" + pair.second->str();

					tp = this->findTypeInFuncTree(ns, mangledGeneric).first;
					// fprintf(stderr, "mangled: %s // tp: %p\n", mangledGeneric.c_str(), tp);
				}


				fir::Type* concrete = tp ? tp->first : 0;
				if(!concrete)
				{
					// generate the type.
					iceAssert(oldSB);

					if(oldSB->genericTypes.size() > 0 && instantiatedGenericTypes.size() == 0)
					{
						error(user, "Type '%s' needs %zu type parameter%s, but none were provided",
							oldSB->name.c_str(), oldSB->genericTypes.size(), oldSB->genericTypes.size() == 1 ? "" : "s");
					}


					// temporarily hijack the main scope
					auto old = this->namespaceStack;
					this->namespaceStack = ns;

					concrete = oldSB->createType(this, instantiatedGenericTypes);

					if(instantiatedGenericTypes.size() > 0)
					{
						this->pushGenericTypeStack();
						for(auto t : instantiatedGenericTypes)
							this->pushGenericType(t.first, t.second);
					}

					// note: codegen() uses str->createdType. since we only *recently* called
					// createType, it should be set properly.

					// set codegen = 0
					oldSB->didCodegen = false;
					oldSB->codegen(this); // this sets it to true

					if(instantiatedGenericTypes.size() > 0)
						this->popGenericTypeStack();

					this->namespaceStack = old;

					if(!concrete) error(user, "!!!");
					iceAssert(concrete);

					if(!tp) tp = this->findTypeInFuncTree(ns, mangledGeneric).first;
					iceAssert(tp);
				}
				// info(user, "concrete: %s // %s\n", type.strType.c_str(), concrete->str().c_str());

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
			if(!type->toStructType()->isLiteralStruct() && type->toStructType()->getStructName() == "Any")
			{
				return true;
			}

			TypePair_t* pair = this->getType("Any");
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
			if((tp = this->getType(this->mangleWithNamespace(type.strType))))
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
			if((tp = this->getType(this->mangleWithNamespace(type.strType))))
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
			std::string str = "ƒ " + fd->name + "(";
			for(auto p : fd->params)
			{
				str += p->name + ": " + (p->inferredLType ? this->getReadableType(p->inferredLType) : p->type.strType) + ", ";
				// str += this->printAst(p).substr(4) + ", "; // remove the leading 'val' or 'var'.
			}

			if(fd->hasVarArg) str += "..., ";

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
			return (vd->immutable ? ("val ") : ("var ")) + vd->name + ": "
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
		else if(Class* cls = dynamic_cast<Class*>(expr))
		{
			std::string s;
			s = "class " + cls->name + "\n{\n";

			for(auto m : cls->members)
				s += this->printAst(m) + "\n";

			for(auto f : cls->funcs)
				s += this->printAst(f) + "\n";

			s += "\n}";
			return s;
		}
		else if(Struct* str = dynamic_cast<Struct*>(expr))
		{
			std::string s;
			s = "struct " + str->name + "\n{\n";

			for(auto m : str->members)
				s += this->printAst(m) + "\n";

			s += "\n}";
			return s;
		}
		else if(Enumeration* enr = dynamic_cast<Enumeration*>(expr))
		{
			std::string s;
			s = "enum " + enr->name + "\n{\n";

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































