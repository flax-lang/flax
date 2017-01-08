// TypeUtils.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "codegen.h"

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


	TypePair_t* CodegenInstance::findTypeInFuncTree(std::deque<std::string> scope, std::string name)
	{
		if(this->getExprTypeOfBuiltin(name) != 0)
			return 0;

		auto cft = this->getFuncTreeFromNS(scope);
		while(cft)
		{
			for(auto& f : cft->types)
			{
				if(f.first == name)
					return &f.second;
			}

			cft = cft->parent;
		}

		return 0;
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

		if(t->isStringType())
			return this->getEmptyString().value;

		else if(t->isDynamicArrayType())
			return this->createEmptyDynamicArray(t->toDynamicArrayType()->getElementType()).value;

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
			return fir::ParameterPackType::get(base);
		}
		else if(pt->isDynamicArrayType())
		{
			fir::Type* base = _recursivelyConvertType(cgi, allowFail, user, pt->toDynamicArrayType()->base);
			return fir::DynamicArrayType::get(base);
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




			TypePair_t* tp = cgi->findTypeInFuncTree(ns, atype);

			if(tp)
			{
				return tp->first;
			}
			else if(fir::Type* builtin = cgi->getExprTypeOfBuiltin(atype))
			{
				return builtin;
			}
			else
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

				if(allowFail) return 0;
				GenError::unknownSymbol(cgi, user, atype + (nsstr.size() > 0 ? (" in namespace " + nsstr) : ""), SymbolType::Type);
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

			auto ret = fir::FunctionType::get(args, retty, args.size() > 0 && args.back()->isParameterPackType());

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
		else if(type->isParameterPackType())
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

		return "?";
	}
}














namespace Ast
{
	fir::Type* DummyExpr::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
	{
		return cgi->getTypeFromParserType(this, this->ptype);
	}
}

















