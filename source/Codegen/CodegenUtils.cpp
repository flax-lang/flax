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
		cgi->pushNamespaceScope("");

		for(auto pair : cgi->rootNode->externalFuncs)
		{
			auto func = pair.second;

			// add to the func table
			llvm::Function* f = llvm::cast<llvm::Function>(cgi->mainModule->getOrInsertFunction(func->getName(), func->getFunctionType()));

			f->deleteBody();
			cgi->addFunctionToScope(func->getName(), new FuncPair_t(f, pair.first));
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
		TypePair_t* tpair = new TypePair_t(ltype, TypedExpr_t(atype, e));

		if(this->typeMap.find(atype->name) == this->typeMap.end())
			this->typeMap[atype->name] = tpair;
		else
		{
			error(0, "duplicate type %s", atype->name.c_str());
		}
	}

	llvm::LLVMContext& CodegenInstance::getContext()
	{
		return llvm::getGlobalContext();
	}

	Root* CodegenInstance::getRootAST()
	{
		return rootNode;
	}







	void CodegenInstance::popScope()
	{
		SymTab_t* tab = symTabStack.back();
		delete tab;

		symTabStack.pop_back();
	}

	void CodegenInstance::clearScope()
	{
		symTabStack.clear();

		this->clearNamespaceScope();
	}

	void CodegenInstance::pushScope(SymTab_t* tab)
	{
		this->symTabStack.push_back(tab);
	}

	void CodegenInstance::pushScope()
	{
		this->pushScope(new SymTab_t());
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
		if(this->typeMap.find(name) != this->typeMap.end())
			return this->typeMap[name];

		return nullptr;
	}

	TypePair_t* CodegenInstance::getType(llvm::Type* type)
	{
		for(auto pair : this->typeMap)
		{
			if(pair.second->first == type)
				return pair.second;
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
	void CodegenInstance::pushNamespaceScope(std::string namespc)
	{
		this->namespaceStack.push_back(namespc);
	}

	void CodegenInstance::addFunctionToScope(std::string name, FuncPair_t* func)
	{
		this->funcMap[name] = func;
	}

	FuncPair_t* CodegenInstance::getDeclaredFunc(std::string name)
	{
		for(int i = this->namespaceStack.size(); i-- > 0;)
		{
			FuncMap_t& tab = this->funcMap;
			if(tab.find(name) != tab.end())
				return tab[name];
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

	void CodegenInstance::popNamespaceScope()
	{
		this->namespaceStack.pop_back();
	}

	void CodegenInstance::clearNamespaceScope()
	{
		this->namespaceStack.clear();
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
		llvm::Type* ret = this->getLlvmType(actualType);

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
		llvm::Type* ltype = this->getLlvmType(expr);
		return (ltype->isIntegerTy() || ltype->isFloatingPointTy());
	}

	llvm::Type* CodegenInstance::getLlvmTypeOfBuiltin(std::string type)
	{
		if(type == "Int8")			return llvm::Type::getInt8Ty(this->getContext());
		else if(type == "Int16")	return llvm::Type::getInt16Ty(this->getContext());
		else if(type == "Int32")	return llvm::Type::getInt32Ty(this->getContext());
		else if(type == "Int64")	return llvm::Type::getInt64Ty(this->getContext());

		else if(type == "Uint8")	return llvm::Type::getInt8Ty(this->getContext());
		else if(type == "Uint16")	return llvm::Type::getInt16Ty(this->getContext());
		else if(type == "Uint32")	return llvm::Type::getInt32Ty(this->getContext());
		else if(type == "Uint64")	return llvm::Type::getInt64Ty(this->getContext());

		else if(type == "Float32")	return llvm::Type::getFloatTy(this->getContext());
		else if(type == "Float64")	return llvm::Type::getFloatTy(this->getContext());
		else if(type == "Bool")		return llvm::Type::getInt1Ty(this->getContext());
		else if(type == "Void")		return llvm::Type::getVoidTy(this->getContext());
		else return nullptr;
	}

	llvm::Type* CodegenInstance::getLlvmType(std::string type)
	{
		llvm::Type* ret = this->getLlvmTypeOfBuiltin(type);
		if(ret) return ret;

		// not so lucky
		TypePair_t* tp = this->getType(type);
		if(!tp)
			GenError::unknownSymbol(0, type, SymbolType::Type);

		return tp->first;
	}

	bool CodegenInstance::isPtr(Expr* expr)
	{
		llvm::Type* ltype = this->getLlvmType(expr);
		return ltype->isPointerTy();
	}

	llvm::Type* CodegenInstance::getLlvmType(Expr* expr)
	{
		assert(expr);
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
			Number* nm			= nullptr;

			if((decl = dynamic_cast<VarDecl*>(expr)))
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
			else if((nm = dynamic_cast<Number*>(expr)))
			{
				return nm->codegen(this).result.first->getType();
			}
		}

		error(expr, "(%s:%d) -> Internal check failed: failed to determine type '%s'", __FILE__, __LINE__, typeid(*expr).name());
		return nullptr;
	}

	bool CodegenInstance::isIntegerType(Expr* e)	{ return getLlvmType(e)->isIntegerTy(); }
	bool CodegenInstance::isSignedType(Expr* e)		{ return false; }		// TODO: something about this

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

	void CodegenInstance::autoCastLlvmType(llvm::Value*& lhs, llvm::Value*& rhs)
	{
		// assert(lhs);
		// assert(rhs);

		// if(lhs->getType()->isIntegerTy() && rhs->getType()->isFloatingPointTy())
		// 	lhs = this->mainBuilder.CreateSIToFP(lhs, rhs->getType());

		// else if(rhs->getType()->isIntegerTy() && lhs->getType()->isFloatingPointTy())
		// 	rhs = this->mainBuilder.CreateSIToFP(rhs, lhs->getType());
	}

	void CodegenInstance::autoCastType(llvm::Value* left, llvm::Value*& right)
	{
		assert(left);
		assert(right);

		if(left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
		{
			right = this->mainBuilder.CreateIntCast(right, left->getType(), false);
		}
	}

	std::string CodegenInstance::mangleName(Struct* s, std::string orig)
	{
		return "__struct#" + this->mangleWithNamespace(s->name) + "_" + orig;
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
		return "enosup";
	}



	std::string CodegenInstance::mangleCppName(std::string base, std::deque<Expr*> args)
	{
		// TODO:
		return "enosup";
	}

	std::string CodegenInstance::mangleWithNamespace(std::string original, std::deque<std::string> ns)
	{
		std::string ret = "__NS";
		for(std::string s : ns)
		{
			if(s.length() > 0)
				ret += std::to_string(s.length()) + s;
		}

		ret += std::to_string(original.length()) + original;

		return ret;
	}

	std::string CodegenInstance::mangleWithNamespace(std::string original)
	{
		std::deque<std::string> ns;
		for(std::string np : this->namespaceStack)
			ns.push_back(np);

		return this->mangleWithNamespace(original, ns);
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
		if(this->getLlvmType(func)->isVoidTy())
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














