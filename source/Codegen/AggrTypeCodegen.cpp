// AggrTypeCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


ValPtr_p ArrayIndex::codegen(Codegen::CodegenInstance* cgi)
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
	ValPtr_p lhsp = this->var->codegen(cgi);

	llvm::Value* lhs;
	if(lhsp.first->getType()->isPointerTy())		lhs = lhsp.first;
	else											lhs = lhsp.second;

	llvm::Value* gep = nullptr;
	llvm::Value* ind = this->index->codegen(cgi).first;

	if(atype->isStructTy())
	{
		llvm::Value* indices[2] = { llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvm::getGlobalContext()), 0), ind };
		gep = cgi->mainBuilder.CreateGEP(lhs, llvm::ArrayRef<llvm::Value*>(indices), "indexPtr");
	}
	else
	{
		gep = cgi->mainBuilder.CreateGEP(lhs, ind, "arrayIndex");
	}

	return ValPtr_p(cgi->mainBuilder.CreateLoad(gep), gep);
}















ValPtr_p Struct::codegen(CodegenInstance* cgi)
{
	assert(this->didCreateType);
	llvm::StructType* str = llvm::cast<llvm::StructType>(cgi->getType(this->name)->first);



	// generate initialiser
	{
		llvm::Function* func = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), llvm::Function::ExternalLinkage, "__automatic_init#" + this->name, cgi->mainModule);


		// if(this->attribs & Attr_VisPublic)
		{
			// make the initialiser public as well
			cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(0, func));
		}

		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", func);
		cgi->mainBuilder.SetInsertPoint(iblock);

		// create the local instance of reference to self
		llvm::Value* self = &func->getArgumentList().front();

		for(VarDecl* var : this->members)
		{
			int i = this->nameMap[var->name];
			assert(i >= 0);

			llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(self, i, "memberPtr_" + var->name);

			var->initVal = cgi->autoCastType(var, var->initVal);
			cgi->mainBuilder.CreateStore(var->initVal ? var->initVal->codegen(cgi).first : cgi->getDefaultValue(var), ptr);
		}

		for(Func* f : this->funcs)
		{
			int i = this->nameMap[f->decl->mangledName];
			llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(self, i, "memberPtr_" + f->decl->name);
			llvm::BasicBlock* ob = cgi->mainBuilder.GetInsertBlock();


			std::string oname = f->decl->name;
			bool isOpOverload = oname.find("operator#") == 0;

			llvm::Function* lf = nullptr;
			llvm::Value* val = nullptr;
			if(f == this->ifunc)
			{
				f->decl->name = cgi->mangleName(this, f->decl->name);
				val = f->decl->codegen(cgi).first;

				std::deque<Expr*> fuckingshit;

				VarRef* svr = new VarRef("self");
				fuckingshit.push_back(svr);

				FuncCall* fc = new FuncCall("__automatic_init#" + this->name, fuckingshit);
				f->closure->statements.push_front(fc);

				auto oi = cgi->mainBuilder.GetInsertBlock();
				this->initFunc = llvm::cast<llvm::Function>((lf = llvm::cast<llvm::Function>(f->codegen(cgi).first)));
				cgi->mainBuilder.SetInsertPoint(oi);
			}
			else
			{
				// mangle
				f->decl->name = cgi->mangleName(this, f->decl->name);
				val = f->decl->codegen(cgi).first;
				lf = llvm::cast<llvm::Function>(f->codegen(cgi).first);
			}

			cgi->mainBuilder.SetInsertPoint(ob);
			cgi->mainBuilder.CreateStore(val, ptr);

			if(isOpOverload)
			{
				oname = oname.substr(strlen("operator#"));
				std::stringstream ss;

				int i = 0;
				while(i < oname.length() && oname[i] != '_')
					(ss << oname[i]), i++;

				ArithmeticOp ao = cgi->determineArithmeticOp(ss.str());
				this->lopmap[ao] = llvm::cast<llvm::Function>(val);
			}

			// make the functions public as well
			cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(f->decl, lf));
		}

		cgi->mainBuilder.CreateRetVoid();

		llvm::verifyFunction(*func);
		this->defifunc = func;
	}


	if(!this->ifunc)
	{
		this->initFunc = this->defifunc;
	}

	return ValPtr_p(nullptr, nullptr);
}

void Struct::createType(CodegenInstance* cgi)
{
	if(cgi->isDuplicateType(this->name))
		error(this, "Redefinition of type '%s'", this->name.c_str());

	llvm::Type** types = new llvm::Type*[this->funcs.size() + this->members.size()];

	if(cgi->isDuplicateType(this->name))
		error(this, "Duplicate type '%s'", this->name.c_str());

	// check if there's an explicit initialiser
	this->ifunc = nullptr;

	// create a bodyless struct so we can use it
	llvm::StructType* str = llvm::StructType::create(llvm::getGlobalContext(), this->name);
	cgi->getVisibleTypes()[this->name] = TypePair_t(str, TypedExpr_t(this, ExprType::Struct));



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

		for(auto p : this->opmap)
			p.second->codegen(cgi);


		for(Func* func : this->funcs)
		{
			if(func->decl->name == "init")
				this->ifunc = func;

			std::vector<llvm::Type*> args;

			// implicit first paramter, is not shown
			VarDecl* implicit_self = new VarDecl("self", true);
			implicit_self->type = this->name + "Ptr";
			func->decl->params.push_front(implicit_self);

			for(VarDecl* v : func->decl->params)
			{
				llvm::Type* vt = cgi->getLlvmType(v);
				if(vt == str)
					error(this, "Cannot have non-pointer member of type self");

				args.push_back(vt);
			}

			types[this->nameMap[func->decl->mangledName]] = llvm::PointerType::get(llvm::FunctionType::get(cgi->getLlvmType(func), llvm::ArrayRef<llvm::Type*>(args), false), 0);
		}

		for(VarDecl* var : this->members)
		{
			llvm::Type* type = cgi->getLlvmType(var);
			if(type == str)
				error(this, "Cannot have non-pointer member of type self");

			types[this->nameMap[var->name]] = cgi->getLlvmType(var);
		}
	}


	std::vector<llvm::Type*> vec(types, types + (this->funcs.size() + this->members.size()));
	str->setBody(vec);


	this->didCreateType = true;
	cgi->getRootAST()->publicTypes.push_back(std::pair<Struct*, llvm::Type*>(this, str));

	delete types;
}










ValPtr_p OpOverload::codegen(CodegenInstance* cgi)
{
	// this is never really called. operators are handled as functions
	// so, we just put them into the structs' funcs.
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

	return ValPtr_p(0, 0);
}












ValPtr_p MemberAccess::codegen(CodegenInstance* cgi)
{
	// gen the var ref on the left.
	ValPtr_p p = this->target->codegen(cgi);

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
		{
			std::string mangled = cgi->mangleName(fc->name, fc->params);
			i = str->nameMap[mangled];
		}
		else
			error("(%s:%s:%d) -> Internal check failed: no comprehendo", __FILE__, __PRETTY_FUNCTION__, __LINE__);

		assert(i >= 0);

		// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
		llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + (fc ? fc->name : var->name));
		llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);

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
				args.push_back(e->codegen(cgi).first);
				if(args.back() == nullptr)
					return ValPtr_p(0, 0);
			}

			return ValPtr_p(cgi->mainBuilder.CreateCall(val, args), 0);
		}
		else if(var)
		{
			return ValPtr_p(val, ptr);
		}
	}


	error("(%s:%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	return ValPtr_p(0, 0);
}
















