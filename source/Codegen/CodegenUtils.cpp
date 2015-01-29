// LlvmCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <vector>
#include <memory>
#include <cfloat>
#include <utility>
#include <fstream>
#include <stdint.h>
#include <typeinfo>
#include <iostream>
#include <cinttypes>
#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"
#include "../include/compiler.h"
#include "llvm/Support/Host.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Transforms/Instrumentation.h"

#include "../include/stacktrace.h"

using namespace Ast;
using namespace Codegen;

void __error_gen(Expr* relevantast, const char* msg, const char* type, bool ex, va_list ap)
{
	char* alloc = nullptr;
	vasprintf(&alloc, msg, ap);

	fprintf(stderr, "%s(%s:%" PRId64 ")%s Error%s: %s\n\n", COLOUR_BLACK_BOLD, relevantast ? relevantast->posinfo.file.c_str() : "?", relevantast ? relevantast->posinfo.line : 0, COLOUR_RED_BOLD, COLOUR_RESET, alloc);

	va_end(ap);
	if(ex) abort();
}




void error(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(relevantast, msg, "Error", true, ap);
}

void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, msg, "Error", true, ap);
}


void warn(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, msg, "Warning", false, ap);
}


void warn(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(relevantast, msg, "Warning", false, ap);
}











namespace Codegen
{
	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi)
	{
		llvm::InitializeNativeTarget();
		cgi->mainModule = new llvm::Module(Parser::getModuleName(filename), llvm::getGlobalContext());
		cgi->mainModule->setTargetTriple(llvm::sys::getProcessTriple());
		cgi->rootNode = root;

		std::string err;
		cgi->execEngine = llvm::EngineBuilder(cgi->mainModule).setErrorStr(&err).create();

		if(!cgi->execEngine)
		{
			fprintf(stderr, "%s", err.c_str());
			exit(1);
		}

		llvm::FunctionPassManager OurFPM = llvm::FunctionPassManager(cgi->mainModule);


		if(Compiler::getOptimisationLevel() > 0)
		{
			// Provide basic AliasAnalysis support for GVN.
			OurFPM.add(llvm::createBasicAliasAnalysisPass());

			// Do simple "peephole" optimisations and bit-twiddling optzns.
			OurFPM.add(llvm::createInstructionCombiningPass());

			// Reassociate expressions.
			OurFPM.add(llvm::createReassociatePass());

			// Eliminate Common SubExpressions.
			OurFPM.add(llvm::createGVNPass());

			// Simplify the control flow graph (deleting unreachable blocks, etc).
			OurFPM.add(llvm::createCFGSimplificationPass());

			// mem2reg
			OurFPM.add(llvm::createPromoteMemoryToRegisterPass());
		}

		OurFPM.doInitialization();


		// Set the global so the code gen can use this.
		cgi->Fpm = &OurFPM;
		cgi->pushScope();

		for(auto pair : cgi->rootNode->externalFuncs)
		{
			auto func = pair.second;

			// add to the func table
			llvm::Function* f = llvm::cast<llvm::Function>(cgi->mainModule->getOrInsertFunction(func->getName(), func->getFunctionType()));
			f->deleteBody();
			cgi->addFunctionToScope(func->getName(), std::pair<llvm::Function*, FuncDecl*>(f, pair.first));
		}

		for(auto pair : cgi->rootNode->externalTypes)
		{
			llvm::StructType* str = llvm::cast<llvm::StructType>(pair.second);
			cgi->getVisibleTypes()[str->getName()] = TypePair_t(str, TypedExpr_t(pair.first, ExprType::Struct));
		}

		cgi->rootNode->codegen(cgi);
		cgi->popScope();
	}

	void writeBitcode(std::string filename, CodegenInstance* cgi)
	{
		std::string e;
		llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
		size_t lastdot = filename.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));
		oname += ".bc";

		llvm::raw_fd_ostream rso(oname.c_str(), e, of);

		llvm::WriteBitcodeToFile(cgi->mainModule, rso);
		rso.close();
	}





















	llvm::LLVMContext& CodegenInstance::getContext()
	{
		// return mainModule->getContext();
		return llvm::getGlobalContext();
	}

	Root* CodegenInstance::getRootAST()
	{
		return rootNode;
	}







	void CodegenInstance::popScope()
	{
		SymTab_t* tab = symTabStack.back();
		TypeMap_t* types = visibleTypes.back();

		delete types;
		delete tab;

		symTabStack.pop_back();
		visibleTypes.pop_back();

		this->popFuncScope();
	}

	void CodegenInstance::pushScope(SymTab_t* tab, TypeMap_t* tp)
	{
		this->symTabStack.push_back(tab);
		this->visibleTypes.push_back(tp);
		this->pushFuncScope("#__anon__");
	}

	void CodegenInstance::pushScope()
	{
		this->pushScope(new SymTab_t(), new TypeMap_t());
	}

	SymTab_t& CodegenInstance::getSymTab()
	{
		return *this->symTabStack.back();
	}

	SymbolPair_t* CodegenInstance::getSymPair(const std::string& name)
	{
		for(int i = symTabStack.size(); i-- > 0;)
		{
			SymTab_t* tab = symTabStack[i];
			if(tab->find(name) != tab->end())
				return &(*tab)[name];
		}

		return nullptr;
	}

	llvm::Value* CodegenInstance::getSymInst(const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(name)))
			return pair->first;

		return nullptr;
	}

	VarDecl* CodegenInstance::getSymDecl(const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(name)))
			return pair->second;

		return nullptr;
	}

	bool CodegenInstance::isDuplicateSymbol(const std::string& name)
	{
		return getSymTab().find(name) != getSymTab().end();
	}




	// stack based types.
	TypeMap_t& CodegenInstance::getVisibleTypes()
	{
		return *visibleTypes.back();
	}

	TypePair_t* CodegenInstance::getType(std::string name)
	{
		for(TypeMap_t* map : visibleTypes)
		{
			if(map->find(name) != map->end())
				return &(*map)[name];
		}

		return nullptr;
	}

	bool CodegenInstance::isDuplicateType(std::string name)
	{
		return getType(name) != nullptr;
	}

	void CodegenInstance::popClosure()
	{
		this->closureStack.pop_back();
	}

	ClosureScope* CodegenInstance::getCurrentClosureScope()
	{
		return this->closureStack.size() > 0 ? &this->closureStack.back() : 0;
	}

	void CodegenInstance::pushClosure(Ast::BreakableClosure* closure, llvm::BasicBlock* body, llvm::BasicBlock* after)
	{
		ClosureScope cs = std::make_pair(closure, std::make_pair(body, after));
		this->closureStack.push_back(cs);
	}
















	// funcs
	void CodegenInstance::pushFuncScope(std::string namespc)
	{
		NamespacePair_t* nsp = new NamespacePair_t(namespc, FuncMap_t());
		this->funcTabStack.push_back(nsp);
	}

	void CodegenInstance::addFunctionToScope(std::string name, FuncPair_t func)
	{
		this->funcTabStack.back()->second[name] = func;
	}

	FuncPair_t* CodegenInstance::getDeclaredFunc(std::string name)
	{
		// todo: handle actual namespacing, ie. calling functions like
		// SomeNamespace::someFunction()
		for(int i = funcTabStack.size(); i-- > 0;)
		{
			FuncMap_t& tab = funcTabStack[i]->second;
			if(tab.find(name) != tab.end())
				return &tab[name];
		}

		return nullptr;
	}

	bool CodegenInstance::isDuplicateFuncDecl(std::string name)
	{
		return getDeclaredFunc(name) != nullptr;
	}

	void CodegenInstance::popFuncScope()
	{
		NamespacePair_t* nsp = this->funcTabStack.back();
		this->funcTabStack.pop_back();

		delete nsp;
	}




















	llvm::Type* CodegenInstance::unwrapPointerType(std::string type)
	{
		std::string sptr = std::string("Ptr");

		int indirections = 0;
		std::string actualType = type;
		if(actualType.length() > 3 && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
		{
			while(actualType.length() > 3 && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
				actualType = actualType.substr(0, actualType.length() - 3), indirections++;
		}

		llvm::Type* ret = nullptr;
		if(Parser::determineVarType(actualType) == VarType::UserDefined)
		{
			TypePair_t* notptrtype = getType(actualType);
			if(notptrtype)
			{
				ret = notptrtype->first;
			}
			else
			{
				error("(CodegenUtils.cpp:~338): Unknown type '%s'", actualType.c_str());
				return nullptr;
			}
		}
		else
		{
			ret = getLlvmTypeOfBuiltin(Parser::determineVarType(actualType));
		}

		while(indirections > 0)
		{
			ret = ret->getPointerTo();
			indirections--;
		}

		return ret;
	}
















	bool CodegenInstance::isBuiltinType(Expr* expr)
	{
		VarType e = determineVarType(expr);
		return e <= VarType::Bool || e == VarType::Float32 || e == VarType::Float64 || e == VarType::Void;
	}

	bool CodegenInstance::isPtr(Expr* expr)
	{
		VarType e = determineVarType(expr);
		return e == VarType::AnyPtr || getLlvmType(expr)->isPointerTy();
	}

	llvm::Type* CodegenInstance::getLlvmTypeOfBuiltin(VarType t)
	{
		switch(t)
		{
			case VarType::Uint8:
			case VarType::Int8:		return llvm::Type::getInt8Ty(getContext());

			case VarType::Uint16:
			case VarType::Int16:	return llvm::Type::getInt16Ty(getContext());

			case VarType::Uint32:
			case VarType::Int32:	return llvm::Type::getInt32Ty(getContext());

			case VarType::Uint64:
			case VarType::Int64:	return llvm::Type::getInt64Ty(getContext());

			case VarType::Float32:	return llvm::Type::getFloatTy(getContext());
			case VarType::Float64:	return llvm::Type::getDoubleTy(getContext());

			case VarType::Void:		return llvm::Type::getVoidTy(getContext());
			case VarType::Bool:		return llvm::Type::getInt1Ty(getContext());

			default:
				error("(%s:%s:%d) -> Internal check failed: not a builtin type", __FILE__, __PRETTY_FUNCTION__, __LINE__);
				return nullptr;
		}
	}

	llvm::Type* CodegenInstance::getLlvmType(Expr* expr)
	{
		VarType t;

		assert(expr);
		if((t = determineVarType(expr)) != VarType::UserDefined && t != VarType::Array)
		{
			return getLlvmTypeOfBuiltin(t);
		}
		else
		{
			VarRef* ref			= nullptr;
			VarDecl* decl		= nullptr;
			FuncCall* fc		= nullptr;
			FuncDecl* fd		= nullptr;
			Func* f				= nullptr;
			StringLiteral* sl	= nullptr;
			CastedType* ct		= nullptr;

			if((decl = dynamic_cast<VarDecl*>(expr)))
			{
				if(t != VarType::Array)
				{
					TypePair_t* type = getType(expr->type);
					if(!type)
					{
						// check if it ends with pointer, and if we have a type that's un-pointered
						llvm::Type* ret = unwrapPointerType(expr->type);
						if(!ret)
							error(expr, "(CodegenUtils.cpp:~439): Unknown type '%s'", expr->type.c_str());

						return ret;
					}

					return type->first;
				}


				// it's an array. decide on its size.
				size_t pos = decl->type.find_first_of('[');
				if(pos == std::string::npos)
					error("(%s:%s:%d) -> Internal check failed: invalid array declaration string", __FILE__, __PRETTY_FUNCTION__, __LINE__);

				std::string etype = decl->type.substr(0, pos);
				std::string atype = decl->type.substr(pos);
				assert(atype[0] == '[' && atype.back() == ']');

				std::string num = atype.substr(1).substr(0, atype.length() - 2);
				int sz = std::stoi(num);
				if(sz == 0)
					error(decl, "Dynamically sized arrays are not yet supported");

				VarType evt = Parser::determineVarType(etype);

				llvm::Type* eltype = nullptr;
				if(evt == VarType::Array)
					error(decl, "Nested arrays are not yet supported");

				if(evt == VarType::Void)
					error(decl, "You cannot create an array of void");

				if(evt != VarType::UserDefined)
				{
					eltype = getLlvmTypeOfBuiltin(evt);
				}
				else
				{
					TypePair_t* type = getType(etype);
					if(!type)
					{
						llvm::Type* ret = unwrapPointerType(etype);
						if(!ret)
							error(expr, "(CodegenUtils.cpp:~482): Unknown type '%s'", etype.c_str());

						else
							eltype = ret;
					}
					else
					{
						eltype = type->first;
					}
				}

				return llvm::ArrayType::get(eltype, sz);
			}
			else if((ref = dynamic_cast<VarRef*>(expr)))
			{
				return getLlvmType(getSymDecl(ref->name));
			}
			else if((ct = dynamic_cast<CastedType*>(expr)))
			{
				return unwrapPointerType(ct->name);
			}
			else if((fc = dynamic_cast<FuncCall*>(expr)))
			{
				FuncPair_t* fp = getDeclaredFunc(fc->name);
				if(!fp)
					error("(%s:%s:%d) -> Internal check failed: invalid function call to '%s'", __FILE__, __PRETTY_FUNCTION__, __LINE__, fc->name.c_str());

				return getLlvmType(fp->second);
			}
			else if((f = dynamic_cast<Func*>(expr)))
			{
				return getLlvmType(f->decl);
			}
			else if((fd = dynamic_cast<FuncDecl*>(expr)))
			{
				VarType vt;
				if((vt = Parser::determineVarType(fd->type)) != VarType::UserDefined)
					return getLlvmTypeOfBuiltin(vt);


				TypePair_t* type = getType(fd->type);
				if(!type)
				{
					llvm::Type* ret = unwrapPointerType(fd->type);

					if(!ret)
					{
						error(expr, "(%s:%s:%d) -> Internal check failed: Unknown type '%s'",
							__FILE__, __PRETTY_FUNCTION__, __LINE__, expr->type.c_str());
					}
					return ret;
				}

				return type->first;
			}
			else if((sl = dynamic_cast<StringLiteral*>(expr)))
			{
				return llvm::Type::getInt8PtrTy(getContext());
			}
		}

		error("(%s:%s:%d) -> Internal check failed: failed to determine type", __FILE__, __PRETTY_FUNCTION__, __LINE__);
		return nullptr;
	}

	VarType CodegenInstance::determineVarType(Expr* e)
	{
		VarRef* ref			= nullptr;
		VarDecl* decl		= nullptr;
		BinOp* bo			= nullptr;
		Number* num			= nullptr;
		FuncDecl* fd		= nullptr;
		FuncCall* fc		= nullptr;
		MemberAccess* ma	= nullptr;

		if((ref = dynamic_cast<VarRef*>(e)))
		{
			VarDecl* decl = getSymDecl(ref->name);
			if(!decl)
			{
				if((e->varType = Parser::determineVarType(ref->name)) != VarType::UserDefined)
					return e->varType;

				error(e, "Unknown variable '%s', could not find declaration", ref->name.c_str());
			}

			// it's a decl. get the type, motherfucker.
			return e->varType = Parser::determineVarType(decl->type);
		}
		else if((decl = dynamic_cast<VarDecl*>(e)))
		{
			// it's a decl. get the type, motherfucker.
			return e->varType = Parser::determineVarType(decl->type);
		}
		else if((num = dynamic_cast<Number*>(e)))
		{
			// it's a decl. get the type, motherfucker.
			return num->varType;
		}
		else if(dynamic_cast<UnaryOp*>(e))
		{
			return determineVarType(dynamic_cast<UnaryOp*>(e)->expr);
		}
		else if(dynamic_cast<Func*>(e))
		{
			return determineVarType(dynamic_cast<Func*>(e)->decl);
		}
		else if((fd = dynamic_cast<FuncDecl*>(e)))
		{
			return Parser::determineVarType(fd->type);
		}
		else if((fc = dynamic_cast<FuncCall*>(e)))
		{
			FuncPair_t* fp = this->getDeclaredFunc(fc->name);
			if(!fp)
				error(fc, "Failed to find function declaration for '%s'", fc->name.c_str());

			return Parser::determineVarType(fp->second->type);
		}
		else if((bo = dynamic_cast<BinOp*>(e)))
		{
			// check what kind of shit it is
			if(bo->op == ArithmeticOp::CmpLT || bo->op == ArithmeticOp::CmpGT || bo->op == ArithmeticOp::CmpLEq
				|| bo->op == ArithmeticOp::CmpGEq || bo->op == ArithmeticOp::CmpEq || bo->op == ArithmeticOp::CmpNEq)
			{
				return VarType::Bool;
			}
			else
			{
				if(bo->op == ArithmeticOp::Cast)
				{
					// in case of a cast, the right side is probably either a builtin type, or an identifier
					// either way, it got interpreted by the parser as a VarRef, probably.
					VarRef* vrtype = dynamic_cast<VarRef*>(bo->right);
					assert(vrtype);

					// look at the type.
					VarType vt = Parser::determineVarType(vrtype->name);
					return vt;
				}
				else
				{
					// need to determine type on both sides.
					bo->right = autoCastType(bo->left, bo->right);

					// make sure that now, both sides are the same.
					if(determineVarType(bo->left) != determineVarType(bo->right))
						error(bo, "Unable to form binary expression with different types '%s' and '%s'", getReadableType(bo->left).c_str(), getReadableType(bo->right).c_str());
				}

				return determineVarType(bo->left);
			}
		}
		else if((ma = dynamic_cast<MemberAccess*>(e)))
		{
			return determineVarType(ma->target);
		}
		else
		{
			// error("Unable to determine var type - '%s'", e->type.c_str());
			return VarType::UserDefined;
		}
	}

	bool CodegenInstance::isIntegerType(Expr* e)	{ return getLlvmType(e)->isIntegerTy(); }
	bool CodegenInstance::isSignedType(Expr* e)		{ return determineVarType(e) <= VarType::Int64; }

	llvm::AllocaInst* CodegenInstance::allocateInstanceInBlock(llvm::Function* func, llvm::Type* type, std::string name)
	{
		llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
		return tmpBuilder.CreateAlloca(type, 0, name);
	}

	llvm::AllocaInst* CodegenInstance::allocateInstanceInBlock(llvm::Function* func, VarDecl* var)
	{
		return allocateInstanceInBlock(func, getLlvmType(var), var->name);
	}


	llvm::Value* CodegenInstance::getDefaultValue(Expr* e)
	{
		llvm::Type* llvmtype = getLlvmType(e);

		VarType tp = determineVarType(e);
		switch(tp)
		{
			case VarType::Int8:		return llvm::ConstantInt::get(getContext(), llvm::APInt(8, 0, false));
			case VarType::Int16:	return llvm::ConstantInt::get(getContext(), llvm::APInt(16, 0, false));
			case VarType::Int32:	return llvm::ConstantInt::get(getContext(), llvm::APInt(32, 0, false));
			case VarType::Int64:	return llvm::ConstantInt::get(getContext(), llvm::APInt(64, 0, false));

			case VarType::Uint32:	return llvm::ConstantInt::get(getContext(), llvm::APInt(8, 0, true));
			case VarType::Uint64:	return llvm::ConstantInt::get(getContext(), llvm::APInt(16, 0, true));
			case VarType::Uint8:	return llvm::ConstantInt::get(getContext(), llvm::APInt(32, 0, true));
			case VarType::Uint16:	return llvm::ConstantInt::get(getContext(), llvm::APInt(64, 0, true));

			case VarType::Float32:	return llvm::ConstantFP::get(getContext(), llvm::APFloat(0.0f));
			case VarType::Float64:	return llvm::ConstantFP::get(getContext(), llvm::APFloat(0.0));
			case VarType::Bool:		return llvm::ConstantInt::get(getContext(), llvm::APInt(1, 0, true));

			case VarType::Array:
			{
				assert(llvmtype->isArrayTy());
				llvm::ArrayType* at = llvm::cast<llvm::ArrayType>(llvmtype);

				std::vector<llvm::Constant*> els;
				for(uint64_t i = 0; i < at->getNumElements(); i++)
					els.push_back(llvm::ConstantArray::getNullValue(at->getElementType()));

				return llvm::ConstantArray::get(at, els);
			}

			// todo: check for pointer type
			default:				return llvm::Constant::getNullValue(getLlvmType(e));
		}
	}

	std::string CodegenInstance::getReadableType(llvm::Type* type)
	{
		std::string thing;
		llvm::raw_string_ostream rso(thing);

		type->print(rso);

		return rso.str();
	}

	std::string CodegenInstance::getReadableType(Expr* expr)
	{
		return getReadableType(getLlvmType(expr));
	}

	Expr* CodegenInstance::autoCastType(Expr* left, Expr* right)
	{
		// adjust the right hand int literal, if it is one
		Number* n = nullptr;
		BinOp* b = nullptr;
		if((n = dynamic_cast<Number*>(right)) || (dynamic_cast<UnaryOp*>(right) && (n = dynamic_cast<Number*>(dynamic_cast<UnaryOp*>(right)->expr))))
		{
			if(determineVarType(left) == VarType::Int8 && n->ival <= INT8_MAX)			right->varType = VarType::Int8;
			else if(determineVarType(left) == VarType::Int16 && n->ival <= INT16_MAX)	right->varType = VarType::Int16;
			else if(determineVarType(left) == VarType::Int32 && n->ival <= INT32_MAX)	right->varType = VarType::Int32;
			else if(determineVarType(left) == VarType::Int64 && n->ival <= INT64_MAX)	right->varType = VarType::Int64;
			else if(determineVarType(left) == VarType::Uint8 && n->ival <= UINT8_MAX)	right->varType = VarType::Uint8;
			else if(determineVarType(left) == VarType::Uint16 && n->ival <= UINT16_MAX)	right->varType = VarType::Uint16;
			else if(determineVarType(left) == VarType::Uint32 && n->ival <= UINT32_MAX)	right->varType = VarType::Uint32;
			else if(determineVarType(left) == VarType::Uint64 && n->ival <= UINT64_MAX)	right->varType = VarType::Uint64;
			else if(determineVarType(left) == VarType::Float32 && n->dval <= FLT_MAX)	right->varType = VarType::Float32;
			else if(determineVarType(left) == VarType::Float64 && n->dval <= DBL_MAX)	right->varType = VarType::Float64;
		}

		// ignore it if we can't convert it, likely it is a more complex expression or a varRef.
		return right;
	}

	std::string CodegenInstance::mangleName(Struct* s, std::string orig)
	{
		return "__struct#" + s->name + "_" + orig;
	}

	std::string CodegenInstance::unmangleName(Struct* s, std::string orig)
	{
		std::string ret = orig;
		if(orig.find("__struct#") != 0)
			error("'%s' is not a mangled name of a struct.", orig.c_str());


		if(orig.length() < 10 || orig[9] != '_')
			error("Invalid mangled name '%s'", orig.c_str());


		// remove __struct#_
		ret = ret.substr(10);

		// make sure it's the right struct.
		if(ret.find(s->name) != 0)
			error("'%s' is not a mangled name of struct '%s'", orig.c_str(), s->name.c_str());

		return ret.substr(s->name.length());
	}

	std::string CodegenInstance::mangleName(std::string base, std::deque<VarDecl*> args)
	{
		std::deque<Expr*> a;
		for(auto arg : args)
			a.push_back(arg);

		return mangleName(base, a);
	}

	std::string CodegenInstance::mangleName(std::string base, std::deque<Expr*> args)
	{
		std::string mangled = "";

		for(Expr* e : args)
			mangled += "_" + getReadableType(e);

		return base + (mangled.empty() ? "#void" : ("#" + mangled));
	}


	bool CodegenInstance::isArrayType(Expr* e)
	{
		return getLlvmType(e)->isArrayTy();
	}

	ArithmeticOp CodegenInstance::determineArithmeticOp(std::string ch)
	{
		ArithmeticOp op;

		if(ch == "+")		op = ArithmeticOp::Add;
		else if(ch == "-")	op = ArithmeticOp::Subtract;
		else if(ch == "*")	op = ArithmeticOp::Multiply;
		else if(ch == "/")	op = ArithmeticOp::Divide;
		else if(ch == "%")	op = ArithmeticOp::Modulo;
		else if(ch == "<<")	op = ArithmeticOp::ShiftLeft;
		else if(ch == ">>")	op = ArithmeticOp::ShiftRight;
		else if(ch == "=")	op = ArithmeticOp::Assign;
		else if(ch == "<")	op = ArithmeticOp::CmpLT;
		else if(ch == ">")	op = ArithmeticOp::CmpGT;
		else if(ch == "<=")	op = ArithmeticOp::CmpLEq;
		else if(ch == ">=")	op = ArithmeticOp::CmpGEq;
		else if(ch == "==")	op = ArithmeticOp::CmpEq;
		else if(ch == "!=")	op = ArithmeticOp::CmpNEq;
		else if(ch == "&")	op = ArithmeticOp::BitwiseAnd;
		else if(ch == "|")	op = ArithmeticOp::BitwiseOr;
		else if(ch == "&&")	op = ArithmeticOp::LogicalOr;
		else if(ch == "||")	op = ArithmeticOp::LogicalAnd;
		else if(ch == "as")	op = ArithmeticOp::Cast;
		else			error("Unknown operator '%s'", ch.c_str());

		return op;
	}



	Result_t CodegenInstance::callOperatorOnStruct(TypePair_t* pair, llvm::Value* self, ArithmeticOp op, llvm::Value* val)
	{
		assert(pair);
		assert(pair->first);
		assert(pair->second.first);
		assert(pair->second.second == ExprType::Struct);

		Struct* str = dynamic_cast<Struct*>(pair->second.first);
		assert(str);

		llvm::Function* opov = str->lopmap[op];
		if(!opov)
			error("No valid operator overload");


		if(opov->getArgumentList().back().getType() != val->getType())
			error("No valid operator overload, have [%s], got [%s]", getReadableType(val->getType()).c_str(), getReadableType(opov->getArgumentList().back().getType()).c_str());

		// get the function with the same name in the current module
		opov = this->mainModule->getFunction(opov->getName());
		assert(opov);

		// try the assign op.
		if(op == ArithmeticOp::Assign && str->opmap[op])
		{
			// check args.
			mainBuilder.CreateCall2(opov, self, val);
			return Result_t(mainBuilder.CreateLoad(self), self);
		}
		else if(op == ArithmeticOp::CmpEq && str->opmap[op])
		{
			// check that both types work
			return Result_t(mainBuilder.CreateCall2(opov, self, val), 0);
		}


		error("Invalid operator on type");
		return Result_t(0, 0);
	}


}




namespace Ast
{
	uint32_t Attr_Invalid		= 0x00;
	uint32_t Attr_NoMangle		= 0x01;
	uint32_t Attr_VisPublic		= 0x02;
	uint32_t Attr_VisInternal	= 0x04;
	uint32_t Attr_VisPrivate	= 0x08;
	uint32_t Attr_ForceMangle	= 0x10;
}















