// TypeUtils.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	llvm::Type* CodegenInstance::getLlvmTypeOfBuiltin(std::string type)
	{
		if(!Compiler::getDisableLowercaseBuiltinTypes())
		{
			if(type.length() > 0)
			{
				type[0] = toupper(type[0]);
			}
		}

		if(type == "Int8")			return llvm::Type::getInt8Ty(this->getContext());
		else if(type == "Int16")	return llvm::Type::getInt16Ty(this->getContext());
		else if(type == "Int32")	return llvm::Type::getInt32Ty(this->getContext());
		else if(type == "Int64")	return llvm::Type::getInt64Ty(this->getContext());
		else if(type == "Int")		return llvm::Type::getInt64Ty(this->getContext());

		else if(type == "Uint8")	return llvm::Type::getInt8Ty(this->getContext());
		else if(type == "Uint16")	return llvm::Type::getInt16Ty(this->getContext());
		else if(type == "Uint32")	return llvm::Type::getInt32Ty(this->getContext());
		else if(type == "Uint64")	return llvm::Type::getInt64Ty(this->getContext());
		else if(type == "Uint")		return llvm::Type::getInt64Ty(this->getContext());

		else if(type == "Float32")	return llvm::Type::getFloatTy(this->getContext());
		else if(type == "Float")	return llvm::Type::getFloatTy(this->getContext());

		else if(type == "Float64")	return llvm::Type::getDoubleTy(this->getContext());
		else if(type == "Double")	return llvm::Type::getDoubleTy(this->getContext());

		else if(type == "Bool")		return llvm::Type::getInt1Ty(this->getContext());
		else if(type == "Void")		return llvm::Type::getVoidTy(this->getContext());
		else return nullptr;
	}

	llvm::Type* CodegenInstance::getLlvmTypeFromString(Ast::Expr* user, ExprType type, bool allowFail)
	{
		if(type.isLiteral)
		{
			llvm::Type* ret = this->getLlvmTypeOfBuiltin(type.strType);
			if(ret) return ret;

			// not so lucky
			TypePair_t* tp = this->getType(type.strType);
			if(!tp)
				tp = this->getType(type.strType + "E");		// nested types. hack.

			if(!tp && allowFail)
				return 0;

			else if(!tp)
				GenError::unknownSymbol(this, user, type.strType, SymbolType::Type);

			return tp->first;
		}
		else
		{
			error(this, user, "enosup");
		}
	}

	llvm::Value* CodegenInstance::lastMinuteUnwrapType(Expr* user, llvm::Value* alloca)
	{
		iceAssert(alloca->getType()->isPointerTy());
		llvm::Type* baseType = alloca->getType()->getPointerElementType();

		if(this->isEnum(baseType) || this->isTypeAlias(baseType))
		{
			TypePair_t* tp = this->getType(baseType);
			if(!tp)
				error(this, user, "Invalid type '%s'!", baseType->getStructName().str().c_str());

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

	llvm::Type* CodegenInstance::getLlvmType(Expr* expr, bool allowFail, bool setInferred)
	{
		return this->getLlvmType(expr, Resolved_t(), allowFail, setInferred);
	}

	llvm::Type* CodegenInstance::getLlvmType(Expr* expr, Resolved_t preResolvedFn, bool allowFail, bool setInferred)
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
						error(this, expr, "Invalid variable declaration for %s!", decl->name.c_str());
						// return llvm::Type::getVoidTy(this->getContext());
					}

					iceAssert(decl->inferredLType);
					return decl->inferredLType;
				}
				else
				{
					// if we already "inferred" the type, don't bother doing it again.
					if(decl->inferredLType)
						return decl->inferredLType;

					TypePair_t* type = this->getType(decl->type.strType);
					if(!type)
					{
						// check if it ends with pointer, and if we have a type that's un-pointered
						if(decl->type.strType.find("::") != std::string::npos)
						{
							decl->type.strType = this->mangleRawNamespace(decl->type.strType);

							if(setInferred)
								decl->inferredLType = this->getLlvmType(decl, allowFail);

							return setInferred ? decl->inferredLType : this->getLlvmType(decl, allowFail);
						}

						if(setInferred)
							decl->inferredLType = this->parseTypeFromString(decl, decl->type.strType, allowFail);

						return setInferred ? decl->inferredLType : this->parseTypeFromString(decl, decl->type.strType, allowFail);
					}

					if(setInferred)
						decl->inferredLType = type->first;

					return type->first;
				}
			}
			else if(VarRef* ref = dynamic_cast<VarRef*>(expr))
			{
				VarDecl* decl = getSymDecl(ref, ref->name);
				if(!decl)
				{
					error(this, expr, "(%s:%d) -> Internal check failed: invalid var ref to '%s'", __FILE__, __LINE__, ref->name.c_str());
				}

				auto x = this->getLlvmType(decl, allowFail);
				return x;
			}
			else if(UnaryOp* uo = dynamic_cast<UnaryOp*>(expr))
			{
				if(uo->op == ArithmeticOp::Deref)
				{
					llvm::Type* ltype = this->getLlvmType(uo->expr);
					if(!ltype->isPointerTy())
						error(this, expr, "Attempted to dereference a non-pointer type '%s'", this->getReadableType(ltype).c_str());

					return this->getLlvmType(uo->expr)->getPointerElementType();
				}

				else if(uo->op == ArithmeticOp::AddrOf)
					return this->getLlvmType(uo->expr)->getPointerTo();

				else
					return this->getLlvmType(uo->expr);
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
							llvm::Function* genericMaybe = this->tryResolveAndInstantiateGenericFunction(fc);
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

				return getLlvmType(res.t.second);
			}
			else if(Func* f = dynamic_cast<Func*>(expr))
			{
				return getLlvmType(f->decl);
			}
			else if(FuncDecl* fd = dynamic_cast<FuncDecl*>(expr))
			{
				TypePair_t* type = this->getType(fd->type.strType);
				if(!type)
				{
					llvm::Type* ret = this->parseTypeFromString(fd, fd->type.strType, allowFail);
					return ret;
				}

				return type->first;
			}
			else if(StringLiteral* sl = dynamic_cast<StringLiteral*>(expr))
			{
				if(sl->isRaw)
					return llvm::Type::getInt8PtrTy(this->getContext());

				else
				{
					auto tp = this->getType("String");
					if(!tp)
						return llvm::Type::getInt8PtrTy(this->getContext());


					return tp->first;
				}
			}
			else if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
			{
				// hmm.
				VarRef* _vr = 0;
				MemberAccess* _ma = ma;
				do
				{
					_vr = dynamic_cast<VarRef*>(_ma->left);
				}
				while((_ma = dynamic_cast<MemberAccess*>(_ma->left)));


				// VarRef* _vr = dynamic_cast<VarRef*>(ma->left);
				if(_vr)
				{
					// check for type function access (static)
					TypePair_t* tp = 0;
					if((tp = this->getType(this->mangleWithNamespace(_vr->name))))
					{
						if(tp->second.second == TypeKind::Enum)
						{
							iceAssert(tp->first->isStructTy());
							return tp->first;
						}
						else if(tp->second.second == TypeKind::Struct)
						{
							return std::get<0>(this->resolveDotOperator(ma));
						}
					}

					std::deque<NamespaceDecl*> nses = this->resolveNamespace(_vr->name);
					if(nses.size() > 0)
					{
						return std::get<0>(this->resolveDotOperator(ma));
					}
					else if(this->getSymDecl(expr, _vr->name) == 0)
					{
						error(this, expr, "Expression '%s' is neither a namespace nor a variable, "
							"and cannot be accessed with the dot-operator", _vr->name.c_str());
					}
				}

				// first, get the type of the lhs
				llvm::Type* lhs = this->getLlvmType(ma->left);
				TypePair_t* pair = this->getType(lhs->isPointerTy() ? lhs->getPointerElementType() : lhs);

				llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(lhs);

				if(!pair && (!st || (st && !st->isLiteral())))
					error(this, expr, "Invalid type '%s' for dot-operator-access", this->getReadableType(lhs).c_str());



				if((st && st->isLiteral()) || (pair->second.second == TypeKind::Tuple))
				{
					// values are 1, 2, 3 etc.
					// for now, assert this.

					Number* n = dynamic_cast<Number*>(ma->right);
					iceAssert(n);

					llvm::Type* ttype = pair ? pair->first : st;
					iceAssert(ttype->isStructTy());

					if(n->ival >= ttype->getStructNumElements())
						error(this, expr, "Tuple does not have %d elements, only %d", (int) n->ival + 1, ttype->getStructNumElements());

					return ttype->getStructElementType(n->ival);
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
								return this->getLlvmType(mem);
						}
						for(ComputedProperty* c : str->cprops)
						{
							if(c->name == memberVr->name)
								return this->getLlvmTypeFromString(c, c->type, allowFail);
						}
					}
					else if(memberFc)
					{
						return this->getLlvmType(this->getFunctionFromStructFuncCall(str, memberFc));
					}

					return std::get<0>(this->resolveDotOperator(ma));
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
							return this->getLlvmType(c.second);
					}

					error(this, expr, "Enum '%s' has no such case '%s'", enr->name.c_str(), enrcase->name.c_str());
				}
				else
				{
					error(this, expr, "Invalid expr type (%s)", typeid(*pair->second.first).name());
				}
			}
			else if(BinOp* bo = dynamic_cast<BinOp*>(expr))
			{
				if(bo->op == ArithmeticOp::CmpLT || bo->op == ArithmeticOp::CmpGT || bo->op == ArithmeticOp::CmpLEq
				|| bo->op == ArithmeticOp::CmpGEq || bo->op == ArithmeticOp::CmpEq || bo->op == ArithmeticOp::CmpNEq)
				{
					return llvm::IntegerType::getInt1Ty(this->getContext());
				}
				else if(bo->op == ArithmeticOp::Cast || bo->op == ArithmeticOp::ForcedCast)
				{
					return this->getLlvmType(bo->right);
				}
				else
				{
					// check if both are integers
					llvm::Type* ltype = this->getLlvmType(bo->left);
					llvm::Type* rtype = this->getLlvmType(bo->right);

					if(ltype->isIntegerTy() && rtype->isIntegerTy())
					{
						if(ltype->getIntegerBitWidth() > rtype->getIntegerBitWidth())
							return ltype;

						return rtype;
					}
					else
					{
						if(ltype->isPointerTy() && rtype->isIntegerTy())
						{
							// pointer arith?
							return ltype;
						}

						return rtype;
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
						return this->getLlvmTypeFromString(alloc, alloc->type, allowFail)->getPointerTo();
					}

					return this->parseTypeFromString(alloc, alloc->type.strType)->getPointerTo();
				}

				return type->first->getPointerTo();
			}
			else if(Number* nm = dynamic_cast<Number*>(expr))
			{
				return nm->codegen(this).result.first->getType();
			}
			else if(dynamic_cast<BoolVal*>(expr))
			{
				return llvm::Type::getInt1Ty(getContext());
			}
			else if(Return* retr = dynamic_cast<Return*>(expr))
			{
				return this->getLlvmType(retr->val);
			}
			else if(DummyExpr* dum = dynamic_cast<DummyExpr*>(expr))
			{
				if(dum->type.isLiteral)
				{
					return this->parseTypeFromString(expr, dum->type.strType);
				}
				else
				{
					return this->getLlvmType(dum->type.type);
				}
			}
			else if(dynamic_cast<If*>(expr))
			{
				return llvm::Type::getVoidTy(getContext());
			}
			else if(dynamic_cast<Typeof*>(expr))
			{
				TypePair_t* tp = this->getType("Type");
				iceAssert(tp);

				return tp->first;
			}
			else if(Tuple* tup = dynamic_cast<Tuple*>(expr))
			{
				llvm::Type* tp = tup->cachedLlvmType;
				if(!tup->didCreateType)
					tp = tup->getType(this);


				iceAssert(tp);
				return tp;
			}
			else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(expr))
			{
				return this->getLlvmType(ai->arr)->getPointerElementType();
			}
			else if(ArrayLiteral* al = dynamic_cast<ArrayLiteral*>(expr))
			{
				// todo: make this not shit.
				return llvm::ArrayType::get(this->getLlvmType(al->values.front()), al->values.size());
			}
			else if(PostfixUnaryOp* puo = dynamic_cast<PostfixUnaryOp*>(expr))
			{
				llvm::Type* targtype = this->getLlvmType(puo->expr);
				iceAssert(targtype);

				if(puo->kind == PostfixUnaryOp::Kind::ArrayIndex)
				{
					if(targtype->isPointerTy())
						return targtype->getPointerElementType();

					else if(targtype->isArrayTy())
						return targtype->getArrayElementType();

					else
						error(this, expr, "Invalid???");
				}
				else
				{
					iceAssert(0);
				}
			}
		}

		error(this, expr, "(%s:%d) -> Internal check failed: failed to determine type '%s'", __FILE__, __LINE__, typeid(*expr).name());
	}

	llvm::AllocaInst* CodegenInstance::allocateInstanceInBlock(llvm::Type* type, std::string name)
	{
		return this->builder.CreateAlloca(type, 0, name == "" ? "" : name);
	}

	llvm::AllocaInst* CodegenInstance::allocateInstanceInBlock(VarDecl* var)
	{
		return allocateInstanceInBlock(this->getLlvmType(var), var->name);
	}


	llvm::Value* CodegenInstance::getDefaultValue(Expr* e)
	{
		return llvm::Constant::getNullValue(getLlvmType(e));
	}






	static void StringReplace(std::string& str, const std::string& from, const std::string& to)
	{
		size_t start_pos = 0;
		while((start_pos = str.find(from, start_pos)) != std::string::npos)
		{
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
		}
	}

	std::string CodegenInstance::getReadableType(llvm::Type* type)
	{
		if(type == 0)
			return "(null)";

		std::string thing;
		llvm::raw_string_ostream rso(thing);

		type->print(rso);


		// turn it into Flax types.
		std::string ret = rso.str();

		StringReplace(ret, "void", "Void");
		StringReplace(ret, "i8", "Int8");
		StringReplace(ret, "i16", "Int16");
		StringReplace(ret, "i32", "Int32");
		StringReplace(ret, "i64", "Int64");
		StringReplace(ret, "float", "Float32");
		StringReplace(ret, "double", "Float64");

		StringReplace(ret, "i1", "Bool");

		if(ret.length() > 0 && ret[0] == '%')
			ret = ret.substr(1);


		if(ret.length() > 0 && ret.find("=") != (size_t) -1)
		{
			ret = ret.substr(0, ret.find("=") - 1);
		}

		return ret;
	}

	std::string CodegenInstance::getReadableType(llvm::Value* val)
	{
		return this->getReadableType(val->getType());
	}

	std::string CodegenInstance::getReadableType(Expr* expr)
	{
		return this->getReadableType(this->getLlvmType(expr));
	}

	int CodegenInstance::getAutoCastDistance(llvm::Type* from, llvm::Type* to)
	{
		if(!from || !to)
			return -1;

		if(from->isIntegerTy() && to->isIntegerTy() && from->getIntegerBitWidth() != to->getIntegerBitWidth())
		{
			unsigned int ab = from->getIntegerBitWidth();
			unsigned int bb = to->getIntegerBitWidth();

			// we only allow promotion, never truncation (implicitly anyway)
			if(ab > bb) return -1;

			// fk it
			if(ab == 8)
			{
				if(bb == 8)			return 0;
				else if(bb == 16)	return 1;
				else if(bb == 32)	return 2;
				else if(bb == 64)	return 3;
			}
			if(ab == 16)
			{
				if(bb == 8)			return 1;
				else if(bb == 16)	return 0;
				else if(bb == 32)	return 1;
				else if(bb == 64)	return 2;
			}
			if(ab == 32)
			{
				if(bb == 8)			return 2;
				else if(bb == 16)	return 1;
				else if(bb == 32)	return 0;
				else if(bb == 64)	return 1;
			}
			if(ab == 64)
			{
				if(bb == 8)			return 3;
				else if(bb == 16)	return 2;
				else if(bb == 32)	return 1;
				else if(bb == 64)	return 0;
			}
		}
		// check for string to int8*
		else if(to->isPointerTy() && to->getPointerElementType() == llvm::Type::getInt8Ty(this->getContext())
			&& from->isStructTy() && from->getStructName() == this->mangleWithNamespace("String", { }, false))
		{
			return 2;
		}
		else if(from->isPointerTy() && from->getPointerElementType() == llvm::Type::getInt8Ty(this->getContext())
			&& to->isStructTy() && to->getStructName() == this->mangleWithNamespace("String", { }, false))
		{
			return 2;
		}
		else if(to->isFloatingPointTy() && from->isIntegerTy())
		{
			// int-to-float is 10.
			return 10;
		}

		return -1;
	}

	int CodegenInstance::autoCastType(llvm::Type* target, llvm::Value*& right, llvm::Value* rhsPtr)
	{
		if(!target || !right)
			return -1;

		// casting distance for size is determined by the number of "jumps"
		// 8 -> 16 = 1
		// 8 -> 32 = 2
		// 8 -> 64 = 3
		// 16 -> 64 = 2
		// etc.

		int dist = -1;
		if(target->isIntegerTy() && right->getType()->isIntegerTy()
			&& target->getIntegerBitWidth() != right->getType()->getIntegerBitWidth())
		{
			unsigned int lBits = target->getIntegerBitWidth();
			unsigned int rBits = right->getType()->getIntegerBitWidth();

			bool shouldCast = lBits > rBits;
			// check if the RHS is a constant value
			llvm::ConstantInt* constVal = llvm::dyn_cast<llvm::ConstantInt>(right);
			if(constVal)
			{
				// check if the number fits in the LHS type
				if(lBits < 64)	// 64 is the max
				{
					if(constVal->getSExtValue() < 0)
					{
						int64_t max = -1 * powl(2, lBits - 1);
						if(constVal->getSExtValue() > max)
							shouldCast = true;
					}
					else
					{
						uint64_t max = powl(2, lBits) - 1;
						if(constVal->getZExtValue() <= max)
							shouldCast = true;
					}
				}
			}

			if(shouldCast)
			{
				dist = this->getAutoCastDistance(right->getType(), target);
				right = this->builder.CreateIntCast(right, target, false);
			}
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(target->isPointerTy() && target->getPointerElementType() == llvm::Type::getInt8Ty(this->getContext()))
		{
			llvm::Type* rtype = right->getType();
			if(rtype->isStructTy() && rtype->getStructName()== this->mangleWithNamespace("String", std::deque<std::string>()))
			{
				// get the struct gep:
				// Layout of string:
				// var data: Int8*
				// var allocated: Uint64

				// cast the RHS to the LHS
				iceAssert(rhsPtr);
				llvm::Value* ret = this->builder.CreateStructGEP(rhsPtr, 0);
				right = this->builder.CreateLoad(ret);	// mutating

				// string-to-int8* is 2.
				dist = this->getAutoCastDistance(right->getType(), target);
			}
		}
		else if(target->isFloatingPointTy() && right->getType()->isIntegerTy())
		{
			// int-to-float is 10.
			right = this->builder.CreateSIToFP(right, target);
			dist = this->getAutoCastDistance(right->getType(), target);
		}

		return dist;
	}

	int CodegenInstance::autoCastType(llvm::Value* left, llvm::Value*& right, llvm::Value* rhsPtr)
	{
		return this->autoCastType(left->getType(), right, rhsPtr);
	}











	std::string CodegenInstance::unwrapPointerType(std::string type, int* _indirections)
	{
		std::string sptr = std::string("*");
		size_t ptrStrLength = sptr.length();

		std::string actualType = type;
		if(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
		{
			int& indirections = *_indirections;

			while(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
				actualType = actualType.substr(0, actualType.length() - ptrStrLength), indirections++;
		}

		return actualType;
	}

	static llvm::Type* recursivelyParseTuple(CodegenInstance* cgi, Expr* user, std::string& str, bool allowFail)
	{
		iceAssert(str.length() > 0);
		iceAssert(str[0] == '(');

		str = str.substr(1);
		char front = str.front();
		if(front == ')')
			error(cgi, user, "Empty tuples are not supported");

		std::vector<llvm::Type*> types;
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
				llvm::Type* ty = cgi->parseTypeFromString(user, cur, allowFail);
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

		return llvm::StructType::get(cgi->getContext(), types);
	}

	static llvm::Type* recursivelyParseArray(CodegenInstance* cgi, Expr* user, std::string& type, bool allowFail)
	{
		iceAssert(type.size() > 0);

		llvm::Type* ret = 0;
		if(type[0] != '[')
		{
			std::string t = "";
			while(type[0] != ']')
			{
				t += type[0];
				type.erase(type.begin());
			}

			ret = cgi->parseTypeFromString(user, t, allowFail);
		}
		else
		{
			type = type.substr(1);
			ret = recursivelyParseArray(cgi, user, type, allowFail);

			// todo: FIXME -- arrays, not pointers.
			ret = ret->getPointerTo();

			if(type[0] != ']')
				error(cgi, user, "Expected closing '['");
		}

		return ret;
	}

	llvm::Type* CodegenInstance::parseTypeFromString(Expr* user, std::string type, bool allowFail)
	{
		if(type.length() > 0)
		{
			if(type[0] == '(')
			{
				// parse a tuple.
				llvm::Type* parsed = recursivelyParseTuple(this, user, type, allowFail);
				return parsed;
			}
			else if(type[0] == '[')
			{
				// array.
				std::string tp = type;
				llvm::Type* parsed = recursivelyParseArray(this, user, tp, allowFail);

				return parsed;
			}
			else
			{
				int indirections = 0;

				std::string actualType = this->unwrapPointerType(type, &indirections);
				if(actualType.find("[") != (size_t) -1)
				{
					size_t k = actualType.find("[");
					std::string base = actualType.substr(0, k);

					std::string arr = actualType.substr(k);
					llvm::Type* btype = this->parseTypeFromString(user, base, allowFail);


					std::vector<int> sizes;
					if(arr[0] == '[')
					{
						arr = arr.substr(1);
						while(true)
						{
							const char* c = arr.c_str();
							char* final = 0;

							size_t asize = strtoll(c, &final, 0);
							size_t numlen = final - c;

							arr = arr.substr(numlen);
							sizes.push_back(asize);

							if(arr[0] == ',')
							{
								arr = arr.substr(1);
							}
							else if(arr[0] == ']')
							{
								arr = arr.substr(1);
								break;
							}
						}
					}

					for(auto i : sizes)
					{
						btype = llvm::ArrayType::get(btype, i);
					}

					return btype;
				}
				else
				{
					llvm::Type* ret = this->getLlvmTypeFromString(user, ExprType(actualType), allowFail);

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


















	bool CodegenInstance::isArrayType(Expr* e)
	{
		iceAssert(e);
		llvm::Type* ltype = this->getLlvmType(e);
		return ltype && ltype->isArrayTy();
	}

	bool CodegenInstance::isIntegerType(Expr* e)
	{
		iceAssert(e);
		llvm::Type* ltype = this->getLlvmType(e);
		return ltype && ltype->isIntegerTy();
	}

	bool CodegenInstance::isSignedType(Expr* e)
	{
		return false;	// TODO: something about this
	}

	bool CodegenInstance::isPtr(Expr* expr)
	{
		llvm::Type* ltype = this->getLlvmType(expr);
		return ltype && ltype->isPointerTy();
	}

	bool CodegenInstance::isAnyType(llvm::Type* type)
	{
		if(type->isStructTy())
		{
			if(llvm::cast<llvm::StructType>(type)->hasName() && type->getStructName() == "Any")
			{
				return true;
			}

			TypePair_t* pair = this->getType("Any");
			iceAssert(pair);

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
			error("enosup");
		}
	}

	bool CodegenInstance::isEnum(llvm::Type* type)
	{
		if(!type) return false;

		bool res = true;
		if(!type->isStructTy())							res = false;
		if(res && type->getStructNumElements() != 1)	res = false;

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
			error("enosup");
		}
	}

	bool CodegenInstance::isTypeAlias(llvm::Type* type)
	{
		if(!type) return false;

		bool res = true;
		if(!type->isStructTy())							res = false;
		if(res && type->getStructNumElements() != 1)	res = false;

		TypePair_t* tp = 0;
		if((tp = this->getType(type)))
			return tp->second.second == TypeKind::TypeAlias;

		return res;
	}

	bool CodegenInstance::isBuiltinType(llvm::Type* ltype)
	{
		return (ltype && (ltype->isIntegerTy() || ltype->isFloatingPointTy()));
	}

	bool CodegenInstance::isBuiltinType(Expr* expr)
	{
		llvm::Type* ltype = this->getLlvmType(expr);
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
			return (vd->immutable ? ("val ") : ("var ")) + vd->name + ": " + this->getReadableType(vd);
		}
		else if(BinOp* bo = dynamic_cast<BinOp*>(expr))
		{
			return "(" + this->printAst(bo->left) + " " + Parser::arithmeticOpToString(bo->op) + " " + this->printAst(bo->right) + ")";
		}
		else if(UnaryOp* uo = dynamic_cast<UnaryOp*>(expr))
		{
			return "(" + Parser::arithmeticOpToString(uo->op) + this->printAst(uo->expr) + ")";
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

		error(this, expr, "Unknown shit (%s)", typeid(*expr).name());
	}
}































