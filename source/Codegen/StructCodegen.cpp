// AggrTypeCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t Struct::codegen(CodegenInstance* cgi)
{
	cgi->isStructCodegen = true;

	assert(this->didCreateType);
	TypePair_t* _type = cgi->getType(this->name);
	if(!_type)
		GenError::unknownSymbol(this, this->name, SymbolType::Type);


	llvm::StructType* str = llvm::cast<llvm::StructType>(_type->first);
	llvm::Function* defaultInitFunc = 0;


	// generate initialiser
	{
		defaultInitFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), llvm::Function::ExternalLinkage, "__automatic_init#" + this->name, cgi->mainModule);

		cgi->addFunctionToScope(defaultInitFunc->getName(), FuncPair_t(defaultInitFunc, 0));
		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", defaultInitFunc);
		cgi->mainBuilder.SetInsertPoint(iblock);

		// create the local instance of reference to self
		llvm::Value* self = &defaultInitFunc->getArgumentList().front();

		for(VarDecl* var : this->members)
		{
			int i = this->nameMap[var->name];
			assert(i >= 0);

			llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(self, i, "memberPtr_" + var->name);

			auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
			var->doInitialValue(cgi, cgi->getType(var->type), r.first, r.second, ptr);
		}


		// issue here is that functions aren't codegened (ie. don't have the llvm::Function*)
		// before their bodies are codegened, so this makes functions in structs order-dependent.

		// pass 1
		for(Func* f : this->funcs)
		{
			llvm::BasicBlock* ob = cgi->mainBuilder.GetInsertBlock();

			std::string oname = f->decl->name;
			bool isOpOverload = oname.find("operator#") == 0;

			llvm::Value* val = nullptr;


			// this is kind of a hack. since mangleName() operates on f->decl->name, if we
			// modify that to include the __struct#Type prefix, then revert it after, it should
			// add it to f->decl->mangledName, but let us keep f->decl->name

			f->decl->name = cgi->mangleName(this, f->decl->name);
			val = f->decl->codegen(cgi).result.first;
			f->decl->name = oname;


			if(f->decl->name == "init")
				this->initFuncs.push_back(llvm::cast<llvm::Function>(val));


			cgi->mainBuilder.SetInsertPoint(ob);
			this->lfuncs.push_back(llvm::cast<llvm::Function>(val));

			if(isOpOverload)
			{
				oname = oname.substr(strlen("operator#"));
				std::stringstream ss;

				int i = 0;
				while(i < oname.length() && oname[i] != '_')
					(ss << oname[i]), i++;

				ArithmeticOp ao = cgi->determineArithmeticOp(ss.str());
				this->lOpOverloads.push_back(std::make_pair(ao, llvm::cast<llvm::Function>(val)));
			}

			// make the functions public as well
			cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(f->decl, llvm::cast<llvm::Function>(val)));
		}




		// pass 2
		for(Func* f : this->funcs)
		{
			llvm::BasicBlock* ob = cgi->mainBuilder.GetInsertBlock();

			if(f->decl->name == "init")
			{
				std::deque<Expr*> todeque;

				VarRef* svr = new VarRef(this->posinfo, "self");
				todeque.push_back(svr);

				// add the call to auto init
				FuncCall* fc = new FuncCall(this->posinfo, "__automatic_init#" + this->name, todeque);
				f->block->statements.push_front(fc);
			}

			f->codegen(cgi);
			cgi->mainBuilder.SetInsertPoint(ob);
		}




















		cgi->mainBuilder.CreateRetVoid();

		llvm::verifyFunction(*defaultInitFunc);
	}

	if(this->initFuncs.size() == 0)
		this->initFuncs.push_back(defaultInitFunc);


	// if we have an init function, then the __automatic_init will only be called from the local module
	// and only the normal init() needs to be exposed.
	cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(0, defaultInitFunc));

	cgi->isStructCodegen = false;
	return Result_t(nullptr, nullptr);
}

void Struct::createType(CodegenInstance* cgi)
{
	if(cgi->isDuplicateType(this->name))
		GenError::duplicateSymbol(this, this->name, SymbolType::Type);

	llvm::Type** types = new llvm::Type*[this->funcs.size() + this->members.size()];

	// create a bodyless struct so we can use it
	llvm::StructType* str = llvm::StructType::create(llvm::getGlobalContext(), this->name);
	cgi->addNewType(str, this, ExprType::Struct);



	if(!this->didCreateType)
	{
		// because we can't (and don't want to) mangle names in the parser,
		// we could only build an incomplete name -> index map
		// finish it here.

		for(auto p : this->opOverloads)
			p->codegen(cgi);

		for(Func* func : this->funcs)
		{
			// add the implicit self to the declarations.
			VarDecl* implicit_self = new VarDecl(this->posinfo, "self", true);
			implicit_self->type = this->name + "*";
			func->decl->params.push_front(implicit_self);

			std::string mangled = cgi->mangleName(func->decl->name, func->decl->params);
			if(this->nameMap.find(mangled) != this->nameMap.end())
				error(this, "Duplicate member '%s'", func->decl->name.c_str());

			func->decl->mangledName = cgi->mangleName(this, mangled);
		}

		for(VarDecl* var : this->members)
		{
			llvm::Type* type = cgi->getLlvmType(var);
			if(type == str)
				error(this, "Cannot have non-pointer member of type self");

			types[this->nameMap[var->name]] = cgi->getLlvmType(var);
		}
	}


	std::vector<llvm::Type*> vec(types, types + this->members.size());
	str->setBody(vec, this->packed);

	this->didCreateType = true;
	cgi->getRootAST()->publicTypes.push_back(std::pair<Struct*, llvm::Type*>(this, str));
	delete types;
}










Result_t OpOverload::codegen(CodegenInstance* cgi)
{
	// this is never really called for actual codegen. operators are handled as functions,
	// so we just put them into the structs' funcs.
	// BinOp will do a lookup on the opMap, but never call codegen for this.

	// however, this will get called, because we need to know if the parameters for
	// the operator overload are legit. people ignore our return value.


	if(this->op == ArithmeticOp::Assign)
	{
		// make sure the first and only parameter is an 'other' pointer.
		FuncDecl* decl = this->func->decl;

		if(decl->params.size() != 1)
		{
			error("Operator overload for '=' can only have one argument (have %d)", decl->params.size());
		}

		// we can't actually do much, because they can assign to anything
	}
	else if(this->op == ArithmeticOp::CmpEq)
	{
		FuncDecl* decl = this->func->decl;
		if(decl->params.size() != 1)
			error("Operator overload for '==' can only have one argument");

		if(decl->type != "Bool")
			error("Operator overload for '==' must return a boolean value");

		llvm::Type* ptype = cgi->getLlvmType(decl->params.front());
		assert(ptype);

		llvm::Type* stype = cgi->getType(this->str->name)->first;

		if(ptype->getPointerTo() == stype->getPointerTo())
			error(this, "Argument of overload operators (usually 'other') must be a pointer.");

		if(ptype != stype->getPointerTo())
			error(this, "Type mismatch [%s, %s]", cgi->getReadableType(ptype).c_str(), cgi->getReadableType(stype).c_str());
	}
	else
	{
		error("(%s:%d) -> Internal check failed: invalid operator", __FILE__, __LINE__);
	}

	return Result_t(0, 0);
}












Result_t MemberAccess::codegen(CodegenInstance* cgi)
{
	// gen the var ref on the left.
	ValPtr_t p = this->target->codegen(cgi).result;

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;

	if(selfPtr == nullptr)
	{
		// we don't have a pointer value for this
		// it's required for CreateStructGEP, so we'll have to make a temp variable
		// then store the result of the LHS into it.

		selfPtr = cgi->mainBuilder.CreateAlloca(self->getType());
		cgi->mainBuilder.CreateStore(self, selfPtr);
	}

	bool isPtr = false;

	llvm::Type* type = p.first->getType();
	if(!type)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);

	if(!type->isStructTy())
	{
		if(type->isPointerTy() && type->getPointerElementType()->isStructTy())
			type = type->getPointerElementType(), isPtr = true;

		else
			error(this, "Cannot do member access on non-struct types");
	}

	TypePair_t* pair = cgi->getType(type->getStructName());
	if(!pair)
		error("(%s:%d) -> Internal check failed: failed to retrieve type", __FILE__, __LINE__);

	if(pair->second.second == ExprType::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);

		assert(str);
		assert(self);

		// get the index for the member
		Expr* rhs = this->member;
		int i = -1;

		VarRef* var = nullptr;
		FuncCall* fc = nullptr;
		if((var = dynamic_cast<VarRef*>(rhs)))
		{
			if(str->nameMap.find(var->name) != str->nameMap.end())
				i = str->nameMap[var->name];
			else
				error(this, "Type '%s' does not have a member '%s'", str->name.c_str(), var->name.c_str());
		}
		else if((fc = dynamic_cast<FuncCall*>(rhs)))
		{
			// VarRef* fakevr = new VarRef(this->posinfo, "self");
			// fc->params.push_front(fakevr);
			i = -1;
		}
		else
		{
			error(this, "(%s:%d) -> Internal check failed: no comprehendo", __FILE__, __LINE__);
		}










		if(fc)
		{
			// make the args first.
			// since getting the llvm type of a MemberAccess can't be done without codegening the Ast itself,
			// we codegen first, then use the llvm version.
			std::vector<llvm::Value*> args;
			std::deque<llvm::Type*> argtypes;
			args.push_back(isPtr ? self : selfPtr);
			argtypes.push_back((isPtr ? self : selfPtr)->getType());

			for(Expr* e : fc->params)
			{
				args.push_back(e->codegen(cgi).result.first);
				argtypes.push_back(args.back()->getType());
			}

			// need to remove the dummy 'self' reference

			// now we need to determine if it exists, and its params.
			Func* callee = nullptr;
			for(Func* f : str->funcs)
			{
				std::string match = cgi->mangleName(str, cgi->mangleName(fc->name, argtypes));
				if(f->decl->mangledName == match)
				{
					callee = f;
					break;
				}
			}

			if(!callee)
				error(this, "No such function with name '%s' as member of struct '%s'", fc->name.c_str(), str->name.c_str());

			llvm::Function* lcallee = 0;
			for(llvm::Function* lf : str->lfuncs)
			{
				if(lf->getName() == callee->decl->mangledName)
				{
					lcallee = lf;
					break;
				}
			}

			if(!lcallee)
				error(this, "(%s:%d) -> Internal check failed: failed to find function %s", __FILE__, __LINE__, fc->name.c_str());

			lcallee = cgi->mainModule->getFunction(lcallee->getName());
			assert(lcallee);

			return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), 0);
		}
		else if(var)
		{
			assert(i >= 0);

			// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
			llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + (fc ? fc->name : var->name));
			llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);
			return Result_t(val, ptr);
		}
		else
		{
			error("(%s:%d) -> Internal check failed: not var or function?!", __FILE__, __LINE__);
		}
	}


	error("(%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __LINE__);
	return Result_t(0, 0);
}
















