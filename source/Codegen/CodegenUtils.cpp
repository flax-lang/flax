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










namespace Codegen
{
	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi)
	{
		llvm::InitializeNativeTarget();
		cgi->mainModule = new llvm::Module(Parser::getModuleName(filename), llvm::getGlobalContext());
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

			// hmm.
			OurFPM.add(llvm::createScalarizerPass());
			OurFPM.add(llvm::createLoadCombinePass());
			OurFPM.add(llvm::createConstantHoistingPass());
			OurFPM.add(llvm::createStructurizeCFGPass());
		}

		// always do the mem2reg pass, our generated code is too inefficient
		OurFPM.add(llvm::createPromoteMemoryToRegisterPass());
		OurFPM.add(llvm::createScalarReplAggregatesPass());
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
			cgi->addNewType(str, pair.first, ExprType::Struct);
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



















	void CodegenInstance::addNewType(llvm::Type* ltype, Struct* atype, ExprType e)
	{
		TypePair_t tpair(ltype, TypedExpr_t(atype, e));
		(*this->visibleTypes.back())[atype->name] = tpair;
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
		this->pushFuncScope(this->mainBuilder.GetInsertBlock() ? this->mainBuilder.GetInsertBlock()->getName() : "__anon__");
	}

	void CodegenInstance::pushScope()
	{
		this->pushScope(new SymTab_t(), new TypeMap_t());
	}

	SymTab_t& CodegenInstance::getSymTab()
	{
		return *this->symTabStack.back();
	}

	SymbolPair_t* CodegenInstance::getSymPair(Expr* user, const std::string& name)
	{
		for(int i = symTabStack.size(); i-- > 0;)
		{
			SymTab_t* tab = symTabStack[i];
			if(tab->find(name) != tab->end())
				return &(*tab)[name];
		}

		return nullptr;
	}

	llvm::Value* CodegenInstance::getSymInst(Expr* user, const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(user, name)))
		{
			if(pair->first.second != SymbolValidity::Valid)
				GenError::useAfterFree(user, name);

			return pair->first.first;
		}

		return nullptr;
	}

	VarDecl* CodegenInstance::getSymDecl(Expr* user, const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(user, name)))
			return pair->second;

		return nullptr;
	}

	bool CodegenInstance::isDuplicateSymbol(const std::string& name)
	{
		return getSymTab().find(name) != getSymTab().end();
	}

	void CodegenInstance::addSymbol(std::string name, llvm::Value* ai, Ast::VarDecl* vardecl)
	{
		SymbolValidity_t sv(ai, SymbolValidity::Valid);
		SymbolPair_t sp(sv, vardecl);

		this->getSymTab()[name] = sp;
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

	void CodegenInstance::popBracedBlock()
	{
		this->blockStack.pop_back();
	}

	BracedBlockScope* CodegenInstance::getCurrentBracedBlockScope()
	{
		return this->blockStack.size() > 0 ? &this->blockStack.back() : 0;
	}

	void CodegenInstance::pushBracedBlock(Ast::BreakableBracedBlock* block, llvm::BasicBlock* body, llvm::BasicBlock* after)
	{
		BracedBlockScope cs = std::make_pair(block, std::make_pair(body, after));
		this->blockStack.push_back(cs);
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

	FuncPair_t* CodegenInstance::getDeclaredFunc(Ast::FuncCall* fc)
	{
		FuncPair_t* fp = this->getDeclaredFunc(fc->name);
		std::string cmangled = "";
		std::string cppmangled = "";

		if(!fp)	fp = this->getDeclaredFunc(cmangled = this->mangleName(fc->name, fc->params));
		if(!fp)	fp = this->getDeclaredFunc(cppmangled = this->mangleCppName(fc->name, fc->params));

		return fp;
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


















	std::string CodegenInstance::unwrapPointerType(std::string type, int* _indirections)
	{
		std::string sptr = std::string("*");
		int ptrStrLength = sptr.length();

		int& indirections = *_indirections;
		std::string actualType = type;
		if(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
		{
			while(actualType.length() > ptrStrLength && std::equal(sptr.rbegin(), sptr.rend(), actualType.rbegin()))
				actualType = actualType.substr(0, actualType.length() - ptrStrLength), indirections++;
		}

		return actualType;
	}

	llvm::Type* CodegenInstance::unwrapPointerType(std::string type)
	{
		int indirections = 0;

		std::string actualType = this->unwrapPointerType(type, &indirections);
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
				GenError::unknownSymbol(0, actualType, SymbolType::Type);
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















	llvm::Instruction::BinaryOps CodegenInstance::getBinaryOperator(Ast::ArithmeticOp op, bool isSigned, bool isFP)
	{
		using llvm::Instruction;
		switch(op)
		{
			case ArithmeticOp::Add:
			case ArithmeticOp::PlusEquals:
				return !isFP ? Instruction::BinaryOps::Add : Instruction::BinaryOps::FAdd;

			case ArithmeticOp::Subtract:
			case ArithmeticOp::MinusEquals:
				return !isFP ? Instruction::BinaryOps::Sub : Instruction::BinaryOps::FSub;

			case ArithmeticOp::Multiply:
			case ArithmeticOp::MultiplyEquals:
				return !isFP ? Instruction::BinaryOps::Mul : Instruction::BinaryOps::FMul;

			case ArithmeticOp::Divide:
			case ArithmeticOp::DivideEquals:
				return !isFP ? (isSigned ? Instruction::BinaryOps::SDiv : Instruction::BinaryOps::UDiv) : Instruction::BinaryOps::FDiv;

			case ArithmeticOp::Modulo:
			case ArithmeticOp::ModEquals:
				return !isFP ? (isSigned ? Instruction::BinaryOps::SRem : Instruction::BinaryOps::URem) : Instruction::BinaryOps::FRem;

			case ArithmeticOp::ShiftLeft:
			case ArithmeticOp::ShiftLeftEquals:
				return Instruction::BinaryOps::Shl;

			case ArithmeticOp::ShiftRight:
			case ArithmeticOp::ShiftRightEquals:
				return isSigned ? Instruction::BinaryOps::AShr : Instruction::BinaryOps::LShr;

			case ArithmeticOp::BitwiseAnd:
			case ArithmeticOp::BitwiseAndEquals:
				return Instruction::BinaryOps::And;

			case ArithmeticOp::BitwiseOr:
			case ArithmeticOp::BitwiseOrEquals:
				return Instruction::BinaryOps::Or;

			case ArithmeticOp::BitwiseXor:
			case ArithmeticOp::BitwiseXorEquals:
				return Instruction::BinaryOps::Xor;

			default:
				return (Instruction::BinaryOps) 0;
		}
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
			case VarType::UintPtr:	return llvm::Type::getIntNTy(getContext(), this->mainModule->getDataLayout()->getPointerSizeInBits());

			default:
				error("(%s:%d) -> Internal check failed: not a builtin type", __FILE__, __LINE__);
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
			UnaryOp* uo			= nullptr;
			CastedType* ct		= nullptr;
			MemberAccess* ma	= nullptr;
			BinOp* bo			= nullptr;

			if((decl = dynamic_cast<VarDecl*>(expr)))
			{
				if(t != VarType::Array)
				{
					if(decl->type == "Inferred")
					{
						assert(decl->inferredLType);
						return decl->inferredLType;
					}
					else
					{
						TypePair_t* type = getType(expr->type);
						if(!type)
						{
							// check if it ends with pointer, and if we have a type that's un-pointered
							llvm::Type* ret = unwrapPointerType(expr->type);
							assert(ret);	// if it returned without calling error(), it shouldn't be null.
							return ret;
						}

						return type->first;
					}
				}


				// it's an array. decide on its size.
				size_t pos = decl->type.find_first_of('[');
				if(pos == std::string::npos)
					error("(%s:%d) -> Internal check failed: invalid array declaration string", __FILE__, __LINE__);

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
						assert(ret);	// if it returned without calling error(), it shouldn't be null.

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
				return getLlvmType(getSymDecl(ref, ref->name));
			}
			else if((uo = dynamic_cast<UnaryOp*>(expr)))
			{
				if(uo->op == ArithmeticOp::Deref)
					return this->getLlvmType(uo->expr)->getPointerElementType();

				else if(uo->op == ArithmeticOp::AddrOf)
					return this->getLlvmType(uo->expr)->getPointerTo();

				else
					return this->getLlvmType(uo->expr);
			}
			else if((ct = dynamic_cast<CastedType*>(expr)))
			{
				return unwrapPointerType(ct->name);
			}
			else if((fc = dynamic_cast<FuncCall*>(expr)))
			{
				FuncPair_t* fp = getDeclaredFunc(fc);
				if(!fp)
					error(expr, "(%s:%d) -> Internal check failed: invalid function call to '%s'", __FILE__, __LINE__, fc->name.c_str());

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
						error(expr, "(%s:%d) -> Internal check failed: Unknown type '%s'",
							__FILE__, __LINE__, expr->type.c_str());
					}
					return ret;
				}

				return type->first;
			}
			else if((sl = dynamic_cast<StringLiteral*>(expr)))
			{
				return llvm::Type::getInt8PtrTy(getContext());
			}
			else if((ma = dynamic_cast<MemberAccess*>(expr)))
			{
				return this->getLlvmType(ma->target);
			}
			else if((bo = dynamic_cast<BinOp*>(expr)))
			{
				if(bo->op == ArithmeticOp::CmpLT || bo->op == ArithmeticOp::CmpGT || bo->op == ArithmeticOp::CmpLEq
				|| bo->op == ArithmeticOp::CmpGEq || bo->op == ArithmeticOp::CmpEq || bo->op == ArithmeticOp::CmpNEq)
				{
					return llvm::IntegerType::getInt1Ty(this->getContext());
				}
				else
				{
					return this->getLlvmType(bo->right);
				}
			}
		}

		error(expr, "(%s:%d) -> Internal check failed: failed to determine type '%s'", __FILE__, __LINE__, typeid(*expr).name());
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
		BoolVal* bv			= nullptr;

		if((ref = dynamic_cast<VarRef*>(e)))
		{
			VarDecl* decl = getSymDecl(ref, ref->name);
			if(!decl)
			{
				if((e->varType = Parser::determineVarType(ref->name)) != VarType::UserDefined)
					return e->varType;

				GenError::unknownSymbol(e, ref->name, SymbolType::Variable);
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
			// it's a number. get the type, motherfucker.
			return num->varType;
		}
		else if((bv = dynamic_cast<BoolVal*>(e)))
		{
			return VarType::Bool;
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
			FuncPair_t* fp = this->getDeclaredFunc(fc);
			if(!fp)
				return VarType::UserDefined;
				// error(fc, "Failed to find function declaration for '%s'", fc->name.c_str());

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
					// if(determineVarType(bo->left) != determineVarType(bo->right))
					// 	error(bo, "Unable to form binary expression with different types '%s' and '%s'", getReadableType(bo->left).c_str(), getReadableType(bo->right).c_str());
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

	llvm::AllocaInst* CodegenInstance::allocateInstanceInBlock(llvm::Type* type, std::string name)
	{
		return this->mainBuilder.CreateAlloca(type, 0, name);
	}

	llvm::AllocaInst* CodegenInstance::allocateInstanceInBlock(VarDecl* var)
	{
		return allocateInstanceInBlock(getLlvmType(var), var->name);
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
			case VarType::UintPtr:	return llvm::ConstantInt::get(getContext(), llvm::APInt(64, 0, true));

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
		std::string thing;
		llvm::raw_string_ostream rso(thing);

		type->print(rso);


		// turn it into Flax types.
		std::string ret = rso.str();

		StringReplace(ret, "i1", "Bool");
		StringReplace(ret, "i8", "Int8");
		StringReplace(ret, "i16", "Int16");
		StringReplace(ret, "i32", "Int32");
		StringReplace(ret, "i64", "Int64");
		StringReplace(ret, "float", "Float32");
		StringReplace(ret, "double", "Float64");

		return ret;
	}

	std::string CodegenInstance::getReadableType(Expr* expr)
	{
		return getReadableType(getLlvmType(expr));
	}

	Expr* CodegenInstance::autoCastType(Expr* left, Expr* right)
	{
		// adjust the right hand int literal, if it is one
		Number* n = nullptr;
		if((n = dynamic_cast<Number*>(right)) || (dynamic_cast<UnaryOp*>(right) && (n = dynamic_cast<Number*>(dynamic_cast<UnaryOp*>(right)->expr))))
		{
			if(determineVarType(left) == VarType::Int8 && n->ival <= INT8_MAX)				right->varType = VarType::Int8;
			else if(determineVarType(left) == VarType::Int16 && n->ival <= INT16_MAX)		right->varType = VarType::Int16;
			else if(determineVarType(left) == VarType::Int32 && n->ival <= INT32_MAX)		right->varType = VarType::Int32;
			else if(determineVarType(left) == VarType::Int64 && n->ival <= INT64_MAX)		right->varType = VarType::Int64;
			else if(determineVarType(left) == VarType::Uint8 && n->ival <= UINT8_MAX)		right->varType = VarType::Uint8;
			else if(determineVarType(left) == VarType::Uint16 && n->ival <= UINT16_MAX)		right->varType = VarType::Uint16;
			else if(determineVarType(left) == VarType::Uint32 && n->ival <= UINT32_MAX)		right->varType = VarType::Uint32;
			else if(determineVarType(left) == VarType::Uint64 && n->ival <= UINT64_MAX)		right->varType = VarType::Uint64;
			else if(determineVarType(left) == VarType::UintPtr && n->ival <= UINTPTR_MAX)	right->varType = VarType::UintPtr;
			else if(determineVarType(left) == VarType::Float32 && n->dval <= FLT_MAX)		right->varType = VarType::Float32;
			else if(determineVarType(left) == VarType::Float64 && n->dval <= DBL_MAX)		right->varType = VarType::Float64;
		}

		// ignore it if we can't convert it, likely it is a more complex expression or a varRef.
		return right;
	}

	void CodegenInstance::autoCastLlvmType(llvm::Value*& lhs, llvm::Value*& rhs)
	{
		if(lhs->getType()->isIntegerTy() && rhs->getType()->isFloatingPointTy())
			lhs = this->mainBuilder.CreateSIToFP(lhs, rhs->getType());

		else if(rhs->getType()->isIntegerTy() && lhs->getType()->isFloatingPointTy())
			rhs = this->mainBuilder.CreateSIToFP(rhs, lhs->getType());
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


		if(orig.length() < 9)
			error("Invalid mangled name '%s'", orig.c_str());

		// remove __struct#
		ret = ret.substr(9);

		// make sure it's the right struct.
		if(ret.find(s->name) != 0)
			error("'%s' is not a mangled name of struct '%s'", orig.c_str(), s->name.c_str());

		// remove the leading '_'
		ret = ret.substr(1);

		return ret.substr(s->name.length());
	}
	std::string CodegenInstance::mangleName(std::string base, std::deque<llvm::Type*> args)
	{
		std::string mangled = "";

		for(llvm::Type* e : args)
			mangled += "_" + getReadableType(e);

		return base + (mangled.empty() ? "#void" : ("#" + mangled));
	}

	std::string CodegenInstance::mangleName(std::string base, std::deque<Expr*> args)
	{
		std::deque<llvm::Type*> a;
		for(auto arg : args)
			a.push_back(this->getLlvmType(arg));

		return mangleName(base, a);
	}

	std::string CodegenInstance::mangleName(std::string base, std::deque<VarDecl*> args)
	{
		std::deque<llvm::Type*> a;
		for(auto arg : args)
			a.push_back(this->getLlvmType(arg));

		return mangleName(base, a);
	}

	std::string CodegenInstance::mangleCppName(std::string base, std::deque<VarDecl*> args)
	{
		std::deque<Expr*> a;
		for(auto arg : args)
			a.push_back(arg);

		return this->mangleCppName(base, a);
	}

	#if 0
	static char typeStringToCppMangledShorthand(std::string stype)
	{
		// todo: handle namespaces when we get there
		// according to C++ conventions:

		// todo: linux and bsd have different definitions for "uint64_t"
		// BSD uses ULL, linux uses UL

		if(stype == "Int8")			return 'a';
		else if(stype == "Int16")	return 's';
		else if(stype == "Int32")	return 'i';
		else if(stype == "Int64")	return 'x';			// 'l' for long (linux)

		else if(stype == "Uint8")	return 'h';
		else if(stype == "Uint16")	return 't';
		else if(stype == "Uint32")	return 'j';
		else if(stype == "Uint64")	return 'y';			// 'm' for unsigned long (linux)

		else if(stype == "Bool")	return 'b';
		else if(stype == "Float32")	return 'f';
		else if(stype == "Float64")	return 'd';
		else return '?';
	}
	#endif

	std::string CodegenInstance::mangleCppName(std::string base, std::deque<Expr*> args)
	{
		// TODO:
		return "enosup";

		#if 0
		// better for perf then a bunch of +=s, probably.
		std::stringstream mangled;

		// Always start with '_Z'.
		mangled << "_Z";

		// length of unmangled function name
		mangled << base.length();

		// original function name
		mangled << base;

		if(args.size() == 0)
			mangled << "v";


		for(Expr* d : args)
		{
			VarType vt = this->determineVarType(d);
			if(vt != VarType::Array && vt != VarType::UserDefined)
			{
				mangled << typeStringToCppMangledShorthand(Parser::getVarTypeString(vt));
			}
			else if(vt == VarType::UserDefined)
			{
				llvm::Type* type = 0;

				// get the type from one of our magic functions
				// note: llvm doesn't give half a shit whether the integer is signed or unsigned
				// so mangling would give the wrong chars.
				// we need a way of getting a proper, BuiltinType from a varRef.
				// next, we might get a pointer type, so the determineVarType() call would return UserDefined.
				// we would then need to cry

				VarRef*		vr = 0;
				VarDecl*	vd = dynamic_cast<VarDecl*>(d);

				if(!vd && (vr = dynamic_cast<VarRef*>(d)))
				{
					vd = this->getSymDecl(vr, vr->name);
					if(!vd)
						GenError::unknownSymbol(vr, vr->name, SymbolType::Variable);
				}
				assert(vd);
				type = getLlvmType(vd);
				assert(type);


				{
					int indr = 0;
					std::string unwrapped = unwrapPointerType(vd->type, &indr);

					for(int i = 0; i < indr; i++)
						mangled << "P";



					mangled << typeStringToCppMangledShorthand(unwrapped);
					type = 0;
				}

				if(type)
				{
					while(type->isPointerTy())
					{
						mangled << "P";
						type = type->getPointerElementType();
					}

					mangled << (this->mainModule->getDataLayout()->getTypeSizeInBits(type) / 8);
					mangled << type->getStructName().str().substr(1);	// remove leading '%' on llvm types
				}
			}
			else
			{
				// todo: not supported
				error("enosup");
			}
		}

		return mangled.str();
		#endif
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
		else if(ch == ".")	op = ArithmeticOp::MemberAccess;
		else				error("Unknown operator '%s'", ch.c_str());

		return op;
	}



	Result_t CodegenInstance::callOperatorOnStruct(TypePair_t* pair, llvm::Value* self, ArithmeticOp op, llvm::Value* val, bool fail)
	{
		assert(pair);
		assert(pair->first);
		assert(pair->second.first);
		assert(pair->second.second == ExprType::Struct);

		Struct* str = dynamic_cast<Struct*>(pair->second.first);
		assert(str);

		llvm::Function* opov = nullptr;
		for(auto f : str->lOpOverloads)
		{
			if(f.first == op && (f.second->getArgumentList().back().getType() == val->getType()))
			{
				opov = f.second;
				break;
			}
		}

		if(!opov)
		{
			if(fail)	GenError::noOpOverload(str, str->name, op);
			else		return Result_t(0, 0);
		}

		// get the function with the same name in the current module
		opov = this->mainModule->getFunction(opov->getName());
		assert(opov);

		// try the assign op.
		if(op == ArithmeticOp::Assign)
		{
			// check args.
			mainBuilder.CreateCall2(opov, self, val);
			return Result_t(mainBuilder.CreateLoad(self), self);
		}
		else if(op == ArithmeticOp::CmpEq)
		{
			// check that both types work
			return Result_t(mainBuilder.CreateCall2(opov, self, val), 0);
		}

		if(fail)	GenError::noOpOverload(str, str->name, op);
		return Result_t(0, 0);
	}

	llvm::Function* CodegenInstance::getStructInitialiser(Expr* user, TypePair_t* pair, std::vector<llvm::Value*> vals)
	{
		assert(pair);
		assert(pair->first);
		assert(pair->second.first);
		assert(pair->second.second == ExprType::Struct);

		Struct* str = dynamic_cast<Struct*>(pair->second.first);
		assert(str);

		llvm::Function* initf = 0;
		for(llvm::Function* initers : str->initFuncs)
		{
			if(initers->arg_size() < 1)
				error(user, "(%s:%d) -> Internal check failed: init() should have at least one (implicit) parameter", __FILE__, __LINE__);

			if(initers->arg_size() != vals.size())
				continue;

			int i = 0;
			for(auto it = initers->arg_begin(); it != initers->arg_end(); it++, i++)
			{
				llvm::Value& arg = (*it);
				if(vals[i]->getType() != arg.getType())
					goto breakout;
			}

			// fuuuuuuuuck this is ugly
			initf = initers;
			break;

			breakout:
			continue;
		}

		if(!initf)
			GenError::invalidInitialiser(user, str, vals);

		return this->mainModule->getFunction(initf->getName());
	}









	Result_t CodegenInstance::doPointerArithmetic(ArithmeticOp op, llvm::Value* lhs, llvm::Value* lhsptr, llvm::Value* rhs)
	{
		assert(lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy()
		&& (op == ArithmeticOp::Add || op == ArithmeticOp::Subtract));

		llvm::Instruction::BinaryOps lop = this->getBinaryOperator(op, false, false);
		assert(lop);


		// first, multiply the RHS by the number of bits the pointer type is, divided by 8
		// eg. if int16*, then +4 would be +4 int16s, which is (4 * (8 / 4)) = 4 * 2 = 8 bytes

		uint64_t ptrWidth = this->mainModule->getDataLayout()->getPointerSizeInBits();
		uint64_t typesize = this->mainModule->getDataLayout()->getTypeSizeInBits(lhs->getType()->getPointerElementType()) / 8;
		llvm::APInt apint = llvm::APInt(ptrWidth, typesize);
		llvm::Value* intval = llvm::Constant::getIntegerValue(llvm::IntegerType::getIntNTy(this->getContext(), ptrWidth), apint);

		if(rhs->getType()->getIntegerBitWidth() != ptrWidth)
			rhs = this->mainBuilder.CreateIntCast(rhs, intval->getType(), false);


		// this is the properly adjusted thing
		printf("ptr arith: %s, %s\n", this->getReadableType(rhs->getType()).c_str(), this->getReadableType(intval->getType()).c_str());
		llvm::Value* newrhs = this->mainBuilder.CreateMul(rhs, intval);


		// convert the lhs pointer to an int value, so we can add/sub on it
		llvm::Value* ptrval = this->mainBuilder.CreatePtrToInt(lhs, newrhs->getType());

		// create the add/sub
		llvm::Value* res = this->mainBuilder.CreateBinOp(lop, ptrval, newrhs);

		// turn the int back into a pointer, so we can store it back into the var.
		llvm::Value* properres = this->mainBuilder.CreateIntToPtr(res, lhs->getType());
		this->mainBuilder.CreateStore(properres, lhsptr);
		return Result_t(properres, lhsptr);
	}


	static void errorNoReturn(Expr* e)
	{
		error(e, "Not all code paths return a value");
	}

	static void verifyReturnType(CodegenInstance* cgi, Func* f, BracedBlock* bb, Return* r)
	{
		if(!r)
		{
			errorNoReturn(bb);
		}
		else
		{
			llvm::Type* expected = 0;
			llvm::Type* have = 0;

			if((have = cgi->getLlvmType(r->val)) != (expected = cgi->getLlvmType(f->decl)))
				error(r, "Function has return type '%s', but return statement returned value of type '%s' instead", cgi->getReadableType(expected).c_str(), cgi->getReadableType(have).c_str());
		}
	}

	static Return* recursiveVerifyBranch(CodegenInstance* cgi, Func* f, If* ifbranch);
	static Return* recursiveVerifyBlock(CodegenInstance* cgi, Func* f, BracedBlock* bb)
	{
		if(bb->statements.size() == 0)
			errorNoReturn(bb);

		Return* r = nullptr;
		for(Expr* e : bb->statements)
		{
			If* i = nullptr;
			if((i = dynamic_cast<If*>(e)))
				recursiveVerifyBranch(cgi, f, i);

			else if((r = dynamic_cast<Return*>(e)))
				break;
		}

		verifyReturnType(cgi, f, bb, r);
		return r;
	}

	static Return* recursiveVerifyBranch(CodegenInstance* cgi, Func* f, If* ib)
	{
		Return* r = 0;
		for(std::pair<Expr*, BracedBlock*> pair : ib->cases)
		{
			r = recursiveVerifyBlock(cgi, f, pair.second);
		}

		if(ib->final)
			r = recursiveVerifyBlock(cgi, f, ib->final);

		return r;
	}

	void CodegenInstance::verifyAllPathsReturn(Func* func)
	{
		if(this->determineVarType(func) == VarType::Void)
			return;

		// check the block
		if(func->block->statements.size() == 0)
			error(func, "Function %s has return type '%s', but returns nothing", func->decl->name.c_str(), func->decl->type.c_str());


		// now loop through all exprs in the block
		Return* ret = 0;
		for(Expr* e : func->block->statements)
		{
			If* i = nullptr;

			if((i = dynamic_cast<If*>(e)))
				ret = recursiveVerifyBranch(this, func, i);

			// "top level" returns we will just accept.
			else if((ret = dynamic_cast<Return*>(e)))
				break;
		}

		if(!ret)
			error(func, "Function '%s' missing return statement", func->decl->name.c_str());

		verifyReturnType(this, func, func->block, ret);
	}
}














