// AggrTypeCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t ArrayIndex::codegen(Codegen::CodegenInstance* cgi)
{
	// get our array type
	llvm::Type* atype = cgi->getLlvmType(this->var);
	llvm::Type* etype = nullptr;

	if(atype->isArrayTy())
		etype = llvm::cast<llvm::ArrayType>(atype)->getArrayElementType();

	else if(atype->isPointerTy())
		etype = atype->getPointerElementType();

	else
		error(this, "Can only index on pointer or array types.");


	// try and do compile-time bounds checking
	if(atype->isArrayTy())
	{
		assert(llvm::isa<llvm::ArrayType>(atype));
		llvm::ArrayType* at = llvm::cast<llvm::ArrayType>(atype);

		// dynamic arrays don't get bounds checking
		if(at->getNumElements() != 0)
		{
			Number* n = nullptr;
			if((n = dynamic_cast<Number*>(this->index)))
			{
				assert(!n->decimal);
				if(n->ival >= at->getNumElements())
					error(this, "Compile-time bounds checking detected index '%d' is out of bounds of %s[%d]", n->ival, this->var->name.c_str(), at->getNumElements());
			}
		}
	}

	// todo: verify for pointers
	Result_t lhsp = this->var->codegen(cgi);

	llvm::Value* lhs;
	if(lhsp.result.first->getType()->isPointerTy())	lhs = lhsp.result.first;
	else											lhs = lhsp.result.second;

	llvm::Value* gep = nullptr;
	llvm::Value* ind = this->index->codegen(cgi).result.first;

	if(atype->isStructTy())
	{
		llvm::Value* indices[2] = { llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvm::getGlobalContext()), 0), ind };
		gep = cgi->mainBuilder.CreateGEP(lhs, llvm::ArrayRef<llvm::Value*>(indices), "indexPtr");
	}
	else
	{
		gep = cgi->mainBuilder.CreateGEP(lhs, ind, "arrayIndex");
	}

	return Result_t(cgi->mainBuilder.CreateLoad(gep), gep);
}















Result_t Struct::codegen(CodegenInstance* cgi)
{
	assert(this->didCreateType);
	llvm::StructType* str = llvm::cast<llvm::StructType>(cgi->getType(this->name)->first);



	// generate initialiser
	{
		llvm::Function* autoinit = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), llvm::Function::ExternalLinkage, "__automatic_init#" + this->name, cgi->mainModule);


		// if we have an init function, then the __automatic_init will only be called from the local module
		// and only the normal init() needs to be exposed.
		if(!this->ifunc)
		{
			cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(0, autoinit));
		}

		cgi->addFunctionToScope(autoinit->getName(), std::pair<llvm::Function*, FuncDecl*>(autoinit, 0));
		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", autoinit);
		cgi->mainBuilder.SetInsertPoint(iblock);

		// create the local instance of reference to self
		llvm::Value* self = &autoinit->getArgumentList().front();

		for(VarDecl* var : this->members)
		{
			int i = this->nameMap[var->name];
			assert(i >= 0);

			llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(self, i, "memberPtr_" + var->name);

			var->initVal = cgi->autoCastType(var, var->initVal);
			cgi->mainBuilder.CreateStore(var->initVal ? var->initVal->codegen(cgi).result.first : cgi->getDefaultValue(var), ptr);
		}

		for(Func* f : this->funcs)
		{
			llvm::BasicBlock* ob = cgi->mainBuilder.GetInsertBlock();

			std::string oname = f->decl->name;
			bool isOpOverload = oname.find("operator#") == 0;

			llvm::Function* lf = nullptr;
			llvm::Value* val = nullptr;
			if(f->decl->name == "init")
			{
				this->ifunc = f;
				f->decl->mangledName = cgi->mangleName(this, f->decl->name);		// f->decl->name
				val = f->decl->codegen(cgi).result.first;

				std::deque<Expr*> todeque;

				VarRef* svr = new VarRef(this->posinfo, "self");
				todeque.push_back(svr);

				FuncCall* fc = new FuncCall(this->posinfo, "__automatic_init#" + this->name, todeque);
				f->block->statements.push_front(fc);

				this->initFunc = llvm::cast<llvm::Function>((lf = llvm::cast<llvm::Function>(f->codegen(cgi).result.first)));
			}
			else
			{
				// mangle
				f->decl->name = cgi->mangleName(this, f->decl->name);
				val = f->decl->codegen(cgi).result.first;
				lf = llvm::cast<llvm::Function>(f->codegen(cgi).result.first);
			}

			cgi->mainBuilder.SetInsertPoint(ob);
			this->lfuncs.push_back(lf);

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
			cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(f->decl, lf));
		}

		cgi->mainBuilder.CreateRetVoid();

		llvm::verifyFunction(*autoinit);
		this->defifunc = autoinit;
	}

	if(!this->ifunc)
	{
		this->initFunc = this->defifunc;
	}

	return Result_t(nullptr, nullptr);
}

void Struct::createType(CodegenInstance* cgi)
{
	if(cgi->isDuplicateType(this->name))
		error(this, "Redefinition of type '%s'", this->name.c_str());

	llvm::Type** types = new llvm::Type*[this->funcs.size() + this->members.size()];

	if(cgi->isDuplicateType(this->name))
		error(this, "Duplicate type '%s'", this->name.c_str());

	this->ifunc = nullptr;

	// create a bodyless struct so we can use it
	llvm::StructType* str = llvm::StructType::create(llvm::getGlobalContext(), this->name);
	cgi->addNewType(str, this, ExprType::Struct);



	if(!this->didCreateType)
	{
		// because we can't (and don't want to) mangle names in the parser,
		// we could only build an incomplete name -> index map
		// finish it here.
		for(auto p : this->typeList)
		{
			Func* f = nullptr;
			OpOverload* oo = nullptr;

			if((oo = dynamic_cast<OpOverload*>(p.first)))
				f = oo->func;


			if(f || (f = dynamic_cast<Func*>(p.first)))
			{
				std::string mangled = cgi->mangleName(f->decl->name, f->decl->params);
				if(this->nameMap.find(mangled) != this->nameMap.end())
					error(this, "Duplicate member '%s'", f->decl->name.c_str());

				f->decl->mangledName = mangled;
				this->nameMap[mangled] = p.second;
			}
		}

		for(auto p : this->opOverloads)
			p->codegen(cgi);

		for(Func* func : this->funcs)
		{
			// add the implicit self to the declarations.
			VarDecl* implicit_self = new VarDecl(this->posinfo, "self", true);
			implicit_self->type = this->name + "Ptr";
			func->decl->params.push_front(implicit_self);
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

		if(Parser::determineVarType(decl->type) != VarType::Bool)
			error("Operator overload for '==' must returna boolean value");

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
		error("(%s:%s:%d) -> Internal check failed: invalid operator", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	}

	return Result_t(0, 0);
}












Result_t MemberAccess::codegen(CodegenInstance* cgi)
{
	// gen the var ref on the left.
	ValPtr_t p = this->target->codegen(cgi).result;

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;
	bool isPtr = false;

	llvm::Type* type = cgi->getLlvmType(this->target);
	if(!type)
		error("(%s:%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	if(!type->isStructTy())
	{
		if(type->isPointerTy() && type->getPointerElementType()->isStructTy())
			type = type->getPointerElementType(), isPtr = true;

		else
			error(this, "Cannot do member access on non-aggregate types");
	}

	TypePair_t* pair = cgi->getType(type->getStructName());
	if(!pair)
		error("(%s:%s:%d) -> Internal check failed: failed to retrieve type", __FILE__, __PRETTY_FUNCTION__, __LINE__);


	llvm::Function* insertfunc = cgi->mainBuilder.GetInsertBlock()->getParent();

	if(pair->second.second == ExprType::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);
		llvm::Type* str_t = pair->first;

		assert(str);
		assert(self);

		// get the index for the member
		Expr* rhs = this->member;
		int i = -1;

		VarRef* var = nullptr;
		FuncCall* fc = nullptr;
		if((var = dynamic_cast<VarRef*>(rhs)))
			i = str->nameMap[var->name];

		else if((fc = dynamic_cast<FuncCall*>(rhs)))
			i = -1;

		else
			error("(%s:%d) -> Internal check failed: no comprehendo", __FILE__, __LINE__);


		// if we're a function call
		if(fc)
		{
			// now we need to determine if it exists, and its params.
			Func* callee = nullptr;
			for(Func* f : str->funcs)
			{
				// when comparing, we need to remangle the first bit that is the implicit self pointer
				if(f->decl->name == cgi->mangleName(str, fc->name))
				{
					callee = f;
					break;
				}
			}

			if(!callee)
				error(this, "No such function with name '%s' as member of struct '%s'", fc->name.c_str(), str->name.c_str());

			// do some casting
			for(int i = 0; i < fc->params.size(); i++)
				fc->params[i] = cgi->autoCastType(callee->decl->params[i], fc->params[i]);

			std::vector<llvm::Value*> args;
			args.push_back(isPtr ? self : selfPtr);

			for(Expr* e : fc->params)
			{
				args.push_back(e->codegen(cgi).result.first);
			}

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

			return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), 0);
		}
		else if(var)
		{
			assert(i >= 0);
			// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
			llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + (fc ? fc->name : var->name));
			llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);

			// else we're just var access
			return Result_t(val, ptr);
		}
	}


	error("(%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __LINE__);
	return Result_t(0, 0);
}
















