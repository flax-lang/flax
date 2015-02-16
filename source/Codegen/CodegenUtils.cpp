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
#include "../include/parser.h"
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
	void doCodegen(std::string filename, Root* root, CodegenInstance* cgi)
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
			OurFPM.add(llvm::createLoadCombinePass());
			OurFPM.add(llvm::createConstantHoistingPass());
			OurFPM.add(llvm::createDelinearizationPass());
			OurFPM.add(llvm::createFlattenCFGPass());
			OurFPM.add(llvm::createScalarizerPass());
			OurFPM.add(llvm::createSinkingPass());
			OurFPM.add(llvm::createStructurizeCFGPass());
			OurFPM.add(llvm::createInstructionSimplifierPass());
			OurFPM.add(llvm::createDeadStoreEliminationPass());
			OurFPM.add(llvm::createDeadInstEliminationPass());
			OurFPM.add(llvm::createMemCpyOptPass());
			OurFPM.add(llvm::createMergedLoadStoreMotionPass());

			OurFPM.add(llvm::createSCCPPass());
			OurFPM.add(llvm::createAggressiveDCEPass());
		}

		// always do the mem2reg pass, our generated code is too inefficient
		OurFPM.add(llvm::createPromoteMemoryToRegisterPass());
		OurFPM.add(llvm::createScalarReplAggregatesPass());
		OurFPM.add(llvm::createConstantPropagationPass());
		OurFPM.add(llvm::createDeadCodeEliminationPass());


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
			cgi->addFunctionToScope(func->getName(), FuncPair_t(f, pair.first));
		}

		for(auto pair : cgi->rootNode->externalTypes)
		{
			llvm::StructType* str = llvm::cast<llvm::StructType>(pair.second);
			cgi->addNewType(str, pair.first, ExprType::Struct);
		}

		TypeInfo::initialiseTypeInfo(cgi);
		cgi->rootNode->codegen(cgi);
		TypeInfo::generateTypeInfo(cgi);

		cgi->popScope();

		// free the memory
		cgi->clearScope();
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
		return llvm::getGlobalContext();
	}

	Root* CodegenInstance::getRootAST()
	{
		return rootNode;
	}





	void CodegenInstance::popScope()
	{
		symTabStack.pop_back();
	}

	void CodegenInstance::clearScope()
	{
		symTabStack.clear();
		this->clearNamespaceScope();
	}

	void CodegenInstance::pushScope(SymTab_t tab)
	{
		this->symTabStack.push_back(tab);
	}

	void CodegenInstance::pushScope()
	{
		this->pushScope(SymTab_t());
	}

	SymTab_t& CodegenInstance::getSymTab()
	{
		return this->symTabStack.back();
	}

	SymbolPair_t* CodegenInstance::getSymPair(Expr* user, const std::string& name)
	{
		for(int i = symTabStack.size(); i-- > 0;)
		{
			SymTab_t& tab = symTabStack[i];
			if(tab.find(name) != tab.end())
				return &(tab[name]);
		}

		return nullptr;
	}

	llvm::Value* CodegenInstance::getSymInst(Expr* user, const std::string& name)
	{
		SymbolPair_t* pair = getSymPair(user, name);
		if(pair)
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

	void CodegenInstance::addSymbol(std::string name, llvm::Value* ai, VarDecl* vardecl)
	{
		SymbolValidity_t sv(ai, SymbolValidity::Valid);
		SymbolPair_t sp(sv, vardecl);

		this->getSymTab()[name] = sp;
	}


	void CodegenInstance::addNewType(llvm::Type* ltype, Struct* atype, ExprType e)
	{
		TypePair_t tpair(ltype, TypedExpr_t(atype, e));
		std::string mangled = this->mangleWithNamespace(atype->name);

		if(this->typeMap.find(mangled) == this->typeMap.end())
		{
			this->typeMap[mangled] = tpair;
		}
		else
		{
			error(atype, "Duplicate type %s", atype->name.c_str());
		}
	}


	void CodegenInstance::removeType(std::string name)
	{
		if(this->typeMap.find(name) == this->typeMap.end())
			error("Type '%s' does not exist, cannot remove", name.c_str());

		this->typeMap.erase(name);
	}

	TypePair_t* CodegenInstance::getType(std::string name)
	{
		#if 0
		printf("finding %s\n{\n", name.c_str());
		for(auto p : this->typeMap)
			printf("\t%s\n", p.first.c_str());

		printf("}\n");
		#endif

		if(this->typeMap.find(name) != this->typeMap.end())
			return &(this->typeMap[name]);

		return nullptr;
	}

	TypePair_t* CodegenInstance::getType(llvm::Type* type)
	{
		if(!type)
			return nullptr;

		for(auto pair : this->typeMap)
		{
			if(pair.second.first == type)
				return &this->typeMap[pair.first];
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

	void CodegenInstance::pushBracedBlock(BreakableBracedBlock* block, llvm::BasicBlock* body, llvm::BasicBlock* after)
	{
		BracedBlockScope cs = std::make_pair(block, std::make_pair(body, after));
		this->blockStack.push_back(cs);
	}
















	// funcs
	void CodegenInstance::pushNamespaceScope(std::string namespc)
	{
		this->namespaceStack.push_back(namespc);
	}

	void CodegenInstance::addFunctionToScope(std::string name, FuncPair_t func)
	{
		this->funcMap[name] = func;
	}

	FuncPair_t* CodegenInstance::getDeclaredFunc(std::string name)
	{
		FuncMap_t& tab = this->funcMap;

		#if 0
		printf("find %s:\n{\n", name.c_str());
		for(auto p : tab) printf("\t%s\n", p.first.c_str());
		printf("}\n");
		#endif

		if(tab.find(name) != tab.end())
			return &tab[name];

		return nullptr;
	}

	FuncPair_t* CodegenInstance::getDeclaredFunc(FuncCall* fc)
	{
		FuncPair_t* fp = this->getDeclaredFunc(fc->name);

		if(!fp)	fp = this->getDeclaredFunc(this->mangleName(fc->name, fc->params));
		if(!fp)	fp = this->getDeclaredFunc(this->mangleCppName(fc->name, fc->params));

		if(!fp)	fp = this->getDeclaredFunc(this->mangleWithNamespace(fc->name));
		if(!fp)	fp = this->getDeclaredFunc(this->mangleName(this->mangleWithNamespace(fc->name), fc->params));

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

	static void searchForAndApplyExtension(CodegenInstance* cgi, std::deque<Expr*> exprs, std::string extName)
	{
		for(Expr* e : exprs)
		{
			Extension* ext		= dynamic_cast<Extension*>(e);
			NamespaceDecl* ns	= dynamic_cast<NamespaceDecl*>(e);

			if(ext && ext->mangledName == extName)
				ext->createType(cgi);

			else if(ns)
				searchForAndApplyExtension(cgi, ns->innards->statements, extName);
		}
	}

	void CodegenInstance::applyExtensionToStruct(std::string ext)
	{
		searchForAndApplyExtension(this, this->rootNode->topLevelExpressions, ext);
	}





	static std::string convertToMangled(CodegenInstance* cgi, llvm::Type* type)
	{
		std::string r = cgi->getReadableType(type);

		int ind = 0;
		r = cgi->unwrapPointerType(r, &ind);

		if(r.find("Int8") == 0)			r = "a";
		else if(r.find("Int16") == 0)	r = "s";
		else if(r.find("Int32") == 0)	r = "i";
		else if(r.find("Int64") == 0)	r = "l";
		else if(r.find("Int") == 0)		r = "l";

		else if(r.find("Uint8") == 0)	r = "h";
		else if(r.find("Uint16") == 0)	r = "t";
		else if(r.find("Uint32") == 0)	r = "j";
		else if(r.find("Uint64") == 0)	r = "m";
		else if(r.find("Uint") == 0)	r = "m";

		else if(r.find("Float32") == 0)	r = "f";
		else if(r.find("Float64") == 0)	r = "d";
		else if(r.find("Void") == 0)	r = "v";
		else
		{
			if(r.size() > 0 && r.front() == '%')
				r = r.substr(1);

			// remove anything at the back
			// find first of space, then remove everything after

			size_t firstSpace = r.find_first_of(' ');
			if(firstSpace != std::string::npos)
				r.erase(firstSpace);

			r = std::to_string(r.length()) + r;
		}

		for(int i = 0; i < ind; i++)
			r += "P";

		return r;
	}


	std::string CodegenInstance::mangleMemberFunction(StructBase* s, std::string orig, std::deque<Expr*> args)
	{
		return this->mangleMemberFunction(s, orig, args, this->namespaceStack);
	}

	std::string CodegenInstance::mangleMemberFunction(StructBase* s, std::string orig, std::deque<Expr*> args, std::deque<std::string> ns)
	{
		std::string mangled;
		mangled = this->mangleWithNamespace("", ns);

		// last char is 0
		if(mangled.length() > 0)
		{
			assert(mangled.back() == '0');
			mangled = mangled.substr(0, mangled.length() - 1);
		}

		mangled += std::to_string(s->name.length()) + s->name;
		mangled += this->mangleName(std::to_string(orig.length()) + orig + "E", args);

		return mangled;
	}

	std::string CodegenInstance::mangleName(StructBase* s, FuncCall* fc)
	{
		std::deque<llvm::Type*> largs;
		assert(this->getType(s->mangledName));

		bool first = true;
		for(Expr* e : fc->params)
		{
			if(!first)
			{
				// we have an implicit self, don't push that
				largs.push_back(this->getLlvmType(e));
			}

			first = false;
		}

		std::string basename = fc->name + "E";
		std::string mangledFunc = this->mangleName(basename, largs);
		return this->mangleWithNamespace(s->name) + std::to_string(basename.length()) + mangledFunc;
	}

	std::string CodegenInstance::mangleName(StructBase* s, std::string orig)
	{
		return this->mangleWithNamespace(s->name) + std::to_string(orig.length()) + orig;
	}


	std::string CodegenInstance::mangleName(std::string base, std::deque<llvm::Type*> args)
	{
		std::string mangled = "";

		for(llvm::Type* e : args)
			mangled += convertToMangled(this, e);

		return base + (mangled.empty() ? "v" : (mangled));
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
	std::string CodegenInstance::mangleWithNamespace(std::string original)
	{
		return this->mangleWithNamespace(original, this->namespaceStack);
	}


	std::string CodegenInstance::mangleWithNamespace(std::string original, std::deque<std::string> ns)
	{
		std::string ret = "_Z";
		ret += (ns.size() > 0 ? "N" : "");

		for(std::string s : ns)
		{
			if(s.length() > 0)
				ret += std::to_string(s.length()) + s;
		}

		ret += std::to_string(original.length()) + original;
		if(ns.size() == 0)
			ret = original;

		return ret;
	}

	std::string CodegenInstance::mangleRawNamespace(std::string _orig)
	{
		std::string original = _orig;
		std::string ret = "_ZN";

		// we have a name now
		size_t next = 0;
		while((next = original.find_first_of("::")) != std::string::npos)
		{
			std::string ns = original.substr(0, next);
			ret += std::to_string(ns.length()) + ns;

			original = original.substr(next, -1);

			if(original.find("::") == 0)
				original = original.substr(2);
		}

		if(original.length() > 0)
			ret += std::to_string(original.length()) + original;

		return ret;
	}















	std::string CodegenInstance::unwrapPointerType(std::string type, int* _indirections)
	{
		std::string sptr = std::string("*");
		size_t ptrStrLength = sptr.length();

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















	llvm::Instruction::BinaryOps CodegenInstance::getBinaryOperator(ArithmeticOp op, bool isSigned, bool isFP)
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

	bool CodegenInstance::isBuiltinType(llvm::Type* ltype)
	{
		return (ltype && (ltype->isIntegerTy() || ltype->isFloatingPointTy()));
	}

	bool CodegenInstance::isBuiltinType(Expr* expr)
	{
		llvm::Type* ltype = this->getLlvmType(expr);
		return this->isBuiltinType(ltype);
	}

	llvm::Type* CodegenInstance::getLlvmTypeOfBuiltin(std::string type)
	{
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
		return ltype && ltype->isPointerTy();
	}

	llvm::Type* CodegenInstance::getLlvmType(Expr* expr)
	{
		assert(expr);
		{
			VarRef* ref			= dynamic_cast<VarRef*>(expr);
			VarDecl* decl		= dynamic_cast<VarDecl*>(expr);
			FuncCall* fc		= dynamic_cast<FuncCall*>(expr);
			FuncDecl* fd		= dynamic_cast<FuncDecl*>(expr);
			Func* f				= dynamic_cast<Func*>(expr);
			StringLiteral* sl	= dynamic_cast<StringLiteral*>(expr);
			UnaryOp* uo			= dynamic_cast<UnaryOp*>(expr);
			CastedType* ct		= dynamic_cast<CastedType*>(expr);
			MemberAccess* ma	= dynamic_cast<MemberAccess*>(expr);
			BinOp* bo			= dynamic_cast<BinOp*>(expr);
			Number* nm			= dynamic_cast<Number*>(expr);
			BoolVal* bv			= dynamic_cast<BoolVal*>(expr);
			Return* retr		= dynamic_cast<Return*>(expr);

			if(decl)
			{
				if(decl->type == "Inferred")
				{
					assert(decl->inferredLType);
					return decl->inferredLType;
				}
				else
				{
					TypePair_t* type = getType(decl->type);
					if(!type)
					{
						// check if it ends with pointer, and if we have a type that's un-pointered
						if(decl->type.find("::") != std::string::npos)
						{
							decl->type = this->mangleRawNamespace(decl->type);
							return this->getLlvmType(decl);
						}

						return unwrapPointerType(decl->type);
					}

					return type->first;
				}
			}
			else if(ref)
			{
				VarDecl* decl = getSymDecl(ref, ref->name);
				if(!decl)
					error(expr, "(%s:%d) -> Internal check failed: invalid var ref to '%s'", __FILE__, __LINE__, ref->name.c_str());

				return getLlvmType(getSymDecl(ref, ref->name));
			}
			else if(uo)
			{
				if(uo->op == ArithmeticOp::Deref)
					return this->getLlvmType(uo->expr)->getPointerElementType();

				else if(uo->op == ArithmeticOp::AddrOf)
					return this->getLlvmType(uo->expr)->getPointerTo();

				else
					return this->getLlvmType(uo->expr);
			}
			else if(ct)
			{
				return unwrapPointerType(ct->name);
			}
			else if(fc)
			{
				FuncPair_t* fp = getDeclaredFunc(fc);
				if(!fp)
					error(expr, "(%s:%d) -> Internal check failed: invalid function call to '%s'", __FILE__, __LINE__, fc->name.c_str());

				return getLlvmType(fp->second);
			}
			else if(f)
			{
				return getLlvmType(f->decl);
			}
			else if(fd)
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
			else if(sl)
			{
				return this->getType("String")->first;
			}
			else if(ma)
			{
				// first, get the type of the lhs
				llvm::Type* lhs = this->getLlvmType(ma->target);
				TypePair_t* pair = this->getType(lhs);

				if(!pair)
					error(expr, "Invalid type '%s'", this->getReadableType(lhs).c_str());

				Struct* str = dynamic_cast<Struct*>(pair->second.first);
				assert(str);

				VarRef* memberVr = dynamic_cast<VarRef*>(ma->member);
				FuncCall* memberFc = dynamic_cast<FuncCall*>(ma->member);

				if(memberVr)
				{
					for(VarDecl* mem : str->members)
					{
						if(mem->name == memberVr->name)
							return this->getLlvmType(mem);
					}
				}
				else if(memberFc)
				{
					error("enosup");
				}
				else
				{
					error(expr, "invalid");
				}


				return this->getLlvmType(ma->member);
			}
			else if(bo)
			{
				if(bo->op == ArithmeticOp::CmpLT || bo->op == ArithmeticOp::CmpGT || bo->op == ArithmeticOp::CmpLEq
				|| bo->op == ArithmeticOp::CmpGEq || bo->op == ArithmeticOp::CmpEq || bo->op == ArithmeticOp::CmpNEq)
				{
					return llvm::IntegerType::getInt1Ty(this->getContext());
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
						// usually the right
						return this->getLlvmType(bo->right);
					}
				}
			}
			else if(nm)
			{
				return nm->codegen(this).result.first->getType();
			}
			else if(bv)
			{
				return llvm::Type::getInt1Ty(getContext());
			}
			else if(retr)
			{
				return this->getLlvmType(retr->val);
			}
			else if(dynamic_cast<DummyExpr*>(expr))
			{
				return llvm::Type::getVoidTy(getContext());
			}
			else if(dynamic_cast<If*>(expr))
			{
				return llvm::Type::getVoidTy(getContext());
			}

		}

		error(expr, "(%s:%d) -> Internal check failed: failed to determine type '%s'", __FILE__, __LINE__, typeid(*expr).name());
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

		StringReplace(ret, "void", "Void");
		StringReplace(ret, "i1", "Bool");
		StringReplace(ret, "i8", "Int8");
		StringReplace(ret, "i16", "Int16");
		StringReplace(ret, "i32", "Int32");
		StringReplace(ret, "i64", "Int64");
		StringReplace(ret, "float", "Float32");
		StringReplace(ret, "double", "Float64");

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

	void CodegenInstance::autoCastType(llvm::Value* left, llvm::Value*& right, llvm::Value* rhsPtr)
	{
		assert(left);
		assert(right);

		if(left->getType()->isIntegerTy() && right->getType()->isIntegerTy()
			&& left->getType()->getIntegerBitWidth() != right->getType()->getIntegerBitWidth())
		{
			unsigned int lBits = left->getType()->getIntegerBitWidth();
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
						if(constVal->getZExtValue() < max)
							shouldCast = true;
					}
				}
			}

			if(shouldCast)
				right = this->mainBuilder.CreateIntCast(right, left->getType(), false);
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(left->getType()->isPointerTy() && left->getType()->getPointerElementType() == llvm::Type::getInt8Ty(this->getContext()))
		{
			if(right->getType()->isStructTy() && right->getType()->getStructName() == this->mangleWithNamespace("String", std::deque<std::string>()))
			{
				// get the struct gep:
				// Layout of string:
				// var data: Int8*
				// var length: Uint64
				// var allocated: Uint64

				// cast the RHS to the LHS
				assert(rhsPtr);
				llvm::Value* ret = this->mainBuilder.CreateStructGEP(rhsPtr, 0);
				right = this->mainBuilder.CreateLoad(ret);	// mutating
			}
		}
	}


























	bool CodegenInstance::isArrayType(Expr* e)
	{
		llvm::Type* ltype = this->getLlvmType(e);
		return ltype && ltype->isArrayTy();
	}

	ArithmeticOp CodegenInstance::determineArithmeticOp(std::string ch)
	{
		return Parser::mangledStringToOperator(ch);
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
		llvm::Value* newrhs = this->mainBuilder.CreateMul(rhs, intval);

		// convert the lhs pointer to an int value, so we can add/sub on it
		llvm::Value* ptrval = this->mainBuilder.CreatePtrToInt(lhs, newrhs->getType());

		// create the add/sub
		llvm::Value* res = this->mainBuilder.CreateBinOp(lop, ptrval, newrhs);

		// turn the int back into a pointer, so we can store it back into the var.
		llvm::Value* tempRes = this->mainBuilder.CreateAlloca(lhs->getType());

		llvm::Value* properres = this->mainBuilder.CreateIntToPtr(res, lhs->getType());
		this->mainBuilder.CreateStore(properres, tempRes);
		return Result_t(properres, tempRes);
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

	bool CodegenInstance::verifyAllPathsReturn(Func* func)
	{
		if(this->getLlvmType(func)->isVoidTy())
			return false;

		// check the block
		if(func->block->statements.size() == 0)
			error(func, "Function %s has return type '%s', but returns nothing", func->decl->name.c_str(), func->decl->type.c_str());


		// now loop through all exprs in the block
		Return* ret = 0;
		Expr* final = 0;
		for(Expr* e : func->block->statements)
		{
			If* i = dynamic_cast<If*>(e);
			final = e;

			if(i)
				ret = recursiveVerifyBranch(this, func, i);

			// "top level" returns we will just accept.
			else if((ret = dynamic_cast<Return*>(e)))
				break;
		}

		if(!ret && this->getLlvmType(final) == this->getLlvmType(func))
			return true;

		if(!ret)
			error(func, "Function '%s' missing return statement", func->decl->name.c_str());

		verifyReturnType(this, func, func->block, ret);
		return false;
	}
}














