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


// Expr* __debugExpr = 0;

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

			return this->irb.CreateStructGEP(alloca, 0);
		}

		return alloca;
	}


	fir::Value* CodegenInstance::getStackAlloc(fir::Type* type, std::string name)
	{
		return this->irb.CreateStackAlloc(type, name);
	}

	fir::Value* CodegenInstance::getImmutStackAllocValue(fir::Value* initValue, std::string name)
	{
		return this->irb.CreateImmutStackAlloc(initValue->getType(), initValue, name);
	}


	fir::Value* CodegenInstance::getDefaultValue(Expr* e)
	{
		fir::Type* t = e->getType(this);

		if(t->isStringType()) return this->getEmptyString().value;

		return fir::ConstantValue::getNullValue(e->getType(this));
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





	int CodegenInstance::getAutoCastDistance(fir::Type* from, fir::Type* to)
	{
		if(!from || !to)
			return -1;

		if(from->isTypeEqual(to))
			return 0;

		if(from->isPrimitiveType() && to->isPrimitiveType())
		{
			auto fprim = from->toPrimitiveType();
			auto tprim = to->toPrimitiveType();

			if(fprim->isLiteralType() && fprim->isFloatingPointType() && tprim->isFloatingPointType())
			{
				return 1;
			}
			else if(fprim->isLiteralType() && fprim->isIntegerType() && tprim->isIntegerType())
			{
				return 1;
			}
			else if(fprim->isLiteralType() && fprim->isIntegerType() && tprim->isFloatingPointType())
			{
				return 3;
			}
		}



		int ret = 0;
		if(from->isIntegerType() && to->isIntegerType()
			&& (from->toPrimitiveType()->getIntegerBitWidth() != to->toPrimitiveType()->getIntegerBitWidth()
				|| from->toPrimitiveType()->isSigned() != to->toPrimitiveType()->isSigned()))
		{
			unsigned int ab = from->toPrimitiveType()->getIntegerBitWidth();
			unsigned int bb = to->toPrimitiveType()->getIntegerBitWidth();

			// we only allow promotion, never truncation (implicitly anyway)
			// rules:
			// u8 can fit in u16, u32, u64, i16, i32, i64
			// u16 can fit in u32, u64, i32, i64
			// u32 can fit in u64, i64
			// u64 can only fit in itself.

			bool as = from->toPrimitiveType()->isSigned();
			bool bs = to->toPrimitiveType()->isSigned();


			if(as == false && bs == true)
			{
				return bb >= 2 * ab;
			}
			else if(as == true && bs == false)
			{
				// never allow this (signed to unsigned)
				return -1;
			}

			// ok, signs are same now.
			// make sure bitsize >=.
			if(bb < ab) return -1;


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
		// float to float is 1
		else if(to->isFloatingPointType() && from->isFloatingPointType())
		{
			if(to->toPrimitiveType()->getFloatingPointBitWidth() > from->toPrimitiveType()->getFloatingPointBitWidth())
				return 1;

			return -1;
		}
		// check for string to int8*
		else if(to->isPointerType() && to->getPointerElementType() == fir::Type::getInt8(this->getContext())
			&& from->isStringType())
		{
			return 2;
		}
		else if(from->isPointerType() && from->getPointerElementType() == fir::Type::getInt8(this->getContext())
			&& to->isStringType())
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

		int dist = this->getAutoCastDistance(from->getType(), target);
		if(distance != 0)
			*distance = dist;


		if(from->getType() == target)
		{
			retval = from;
		}


		if(target->isIntegerType() && from->getType()->isIntegerType()
			&& (target->toPrimitiveType()->getIntegerBitWidth() != from->getType()->toPrimitiveType()->getIntegerBitWidth()
				|| from->getType()->toPrimitiveType()->isLiteralType()))
		{
			bool shouldCast = dist >= 0;
			fir::ConstantInt* ci = 0;
			if(dist == -1 || from->getType()->toPrimitiveType()->isLiteralType())
			{
				if((ci = dynamic_cast<fir::ConstantInt*>(from)))
				{
					if(ci->getType()->isSignedIntType())
					{
						shouldCast = fir::checkSignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getSignedValue());
					}
					else
					{
						shouldCast = fir::checkUnsignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getUnsignedValue());
					}
				}
			}

			if(shouldCast)
			{
				// if it is a literal, we need to create a new constant with a proper type
				if(from->getType()->toPrimitiveType()->isLiteralType())
				{
					if(ci)
					{
						fir::PrimitiveType* real = 0;
						if(ci->getType()->isSignedIntType())
							real = fir::PrimitiveType::getIntN(target->toPrimitiveType()->getIntegerBitWidth());

						else
							real = fir::PrimitiveType::getUintN(target->toPrimitiveType()->getIntegerBitWidth());

						from = fir::ConstantInt::get(real, ci->getSignedValue());
					}
					else
					{
						// nothing?
						from->setType(from->getType()->toPrimitiveType()->getUnliteralType());
					}

					retval = from;
				}

				// check signed to unsigned first
				if(target->toPrimitiveType()->isSigned() != from->getType()->toPrimitiveType()->isSigned())
				{
					from = this->irb.CreateIntSignednessCast(from, from->getType()->toPrimitiveType()->getOppositeSignedType());
				}

				retval = this->irb.CreateIntSizeCast(from, target);
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
					retval = this->irb.CreateIntSizeCast(from, target);
			}
			else
			{
				// TODO: making this more like C.
				// note(behaviour): check this
				// implicit casting -- signed to unsigned of SAME BITWITH IS ALLOWED.

				if(target->toPrimitiveType()->getIntegerBitWidth() >= from->getType()->toPrimitiveType()->getIntegerBitWidth())
					retval = this->irb.CreateIntSizeCast(from, target);
			}
		}

		// float literals
		else if(target->isFloatingPointType() && from->getType()->isFloatingPointType())
		{
			bool shouldCast = dist >= 0;
			fir::ConstantFP* cf = 0;

			if(dist == -1 || from->getType()->toPrimitiveType()->isLiteralType())
			{
				if((cf = dynamic_cast<fir::ConstantFP*>(from)))
				{
					shouldCast = fir::checkFloatingPointLiteralFitsIntoType(target->toPrimitiveType(), cf->getValue());
					// warn(__debugExpr, "fits: %d", shouldCast);
				}
			}

			if(shouldCast)
			{
				// if it is a literal, we need to create a new constant with a proper type
				if(from->getType()->toPrimitiveType()->isLiteralType())
				{
					if(cf)
					{
						from = fir::ConstantFP::get(target, cf->getValue());
						// info(__debugExpr, "(%s / %s)", target->str().c_str(), from->getType()->str().c_str());
					}
					else
					{
						// nothing.
						from->setType(from->getType()->toPrimitiveType()->getUnliteralType());
					}

					retval = from;
				}

				if(from->getType()->toPrimitiveType()->getFloatingPointBitWidth() < target->toPrimitiveType()->getFloatingPointBitWidth())
					retval = this->irb.CreateFExtend(from, target);
			}
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(target == fir::Type::getInt8Ptr() && from->getType()->isStringType())
		{
			// GEP needs a pointer
			if(fromPtr == 0) fromPtr = this->getImmutStackAllocValue(from);

			iceAssert(fromPtr);
			retval = this->irb.CreateGetStringData(fromPtr);
		}
		else if(target->isFloatingPointType() && from->getType()->isIntegerType())
		{
			// int-to-float is 10.
			retval = this->irb.CreateIntToFloatCast(from, target);
		}
		else if(target->isPointerType() && from->getType()->isNullPointer())
		{
			// retval = fir::ConstantValue::getNullValue(target);
			retval = this->irb.CreatePointerTypeCast(from, target);
		}
		else if(from->getType()->isTupleType() && target->isTupleType()
			&& from->getType()->toTupleType()->getElementCount() == target->toTupleType()->getElementCount())
		{
			// somewhat complicated
			iceAssert(fromPtr);

			fir::Value* tuplePtr = this->getStackAlloc(target);

			for(size_t i = 0; i < from->getType()->toTupleType()->getElementCount(); i++)
			{
				fir::Value* gep = this->irb.CreateStructGEP(tuplePtr, i);
				fir::Value* fromGep = this->irb.CreateStructGEP(fromPtr, i);

				fir::Value* casted = this->autoCastType(gep->getType()->getPointerElementType(), this->irb.CreateLoad(fromGep), fromGep);

				this->irb.CreateStore(casted, gep);
			}

			retval = this->irb.CreateLoad(fromPtr);
		}



		return retval;
	}


	fir::Value* CodegenInstance::autoCastType(fir::Value* left, fir::Value* right, fir::Value* rhsPtr, int* distance)
	{
		return this->autoCastType(left->getType(), right, rhsPtr, distance);
	}











	static fir::Type* _recursivelyConvertType(CodegenInstance* cgi, bool allowFail, Expr* user, pts::Type* pt)
	{
		if(pt->resolvedFType)
		{
			return pt->resolvedFType;
		}
		else if(pt->isPointerType())
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
		else if(pt->isFunctionType())
		{
			auto ft = pt->toFunctionType();

			std::deque<fir::Type*> args;
			// temporarily push a new generic stack
			cgi->pushGenericTypeStack();

			for(auto gt : ft->genericTypes)
				cgi->pushGenericType(gt.first, fir::ParametricType::get(gt.first));

			for(auto arg : ft->argTypes)
				args.push_back(_recursivelyConvertType(cgi, allowFail, user, arg));

			fir::Type* retty = _recursivelyConvertType(cgi, allowFail, user, ft->returnType);

			auto ret = fir::FunctionType::get(args, retty, args.size() > 0 && args.back()->isLLVariableArrayType());

			return ret;
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
		fir::Type* ltype = e->getType(this);
		return ltype && ltype->isArrayType();
	}

	bool CodegenInstance::isIntegerType(Expr* e)
	{
		iceAssert(e);
		fir::Type* ltype = e->getType(this);
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

	bool CodegenInstance::isBuiltinType(fir::Type* t)
	{
		bool ret = (t && (t->isIntegerType() || t->isFloatingPointType() || t->isStringType() || t->isCharType()));
		if(!ret)
		{
			while(t->isPointerType())
				t = t->getPointerElementType();

			ret = (t && (t->isIntegerType() || t->isFloatingPointType() || t->isStringType() || t->isCharType()));
		}

		return ret;
	}

	bool CodegenInstance::isBuiltinType(Expr* expr)
	{
		fir::Type* ltype = expr->getType(this);
		return this->isBuiltinType(ltype);
	}

	bool CodegenInstance::isRefCountedType(fir::Type* type)
	{
		// strings, and structs with rc inside
		if(type->isStructType())
		{
			for(auto m : type->toStructType()->getElements())
			{
				if(this->isRefCountedType(m))
					return true;
			}

			return false;
		}
		else if(type->isClassType())
		{
			for(auto m : type->toClassType()->getElements())
			{
				if(this->isRefCountedType(m))
					return true;
			}

			return false;
		}
		else if(type->isTupleType())
		{
			for(auto m : type->toTupleType()->getElements())
			{
				if(this->isRefCountedType(m))
					return true;
			}

			return false;
		}
		else if(type->isArrayType())
		{
			return this->isRefCountedType(type->toArrayType()->getElementType());
		}
		else if(type->isLLVariableArrayType())
		{
			return this->isRefCountedType(type->toArrayType()->getElementType());
		}
		else
		{
			return type->isStringType();
		}
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
				str += p->ident.name + ": " + (p->concretisedType ? p->concretisedType->str() : p->ptype->str()) + ", ";
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
				+ (vd->concretisedType ? vd->getType(this)->str() : vd->ptype->str());
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














namespace Ast
{
	fir::Type* DummyExpr::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
	{
		return cgi->getTypeFromParserType(this, this->ptype);
	}
}

















