// AggrTypeCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t Struct::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	iceAssert(this->didCreateType);
	TypePair_t* _type = cgi->getType(this->mangledName);
	if(!_type)
		GenError::unknownSymbol(cgi, this, this->name + " (mangled: " + this->mangledName + ")", SymbolType::Type);



	llvm::GlobalValue::LinkageTypes linkageType;
	if(this->attribs & Attr_VisPublic)
	{
		linkageType = llvm::Function::ExternalLinkage;
	}
	else
	{
		linkageType = llvm::Function::InternalLinkage;
	}






	// see if we have nested types
	for(auto nested : this->nestedTypes)
		nested->codegen(cgi);


	llvm::StructType* str = llvm::cast<llvm::StructType>(_type->first);

	// generate initialiser
	llvm::Function* defaultInitFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), linkageType, "__automatic_init__" + this->mangledName, cgi->module);

	cgi->addFunctionToScope(defaultInitFunc->getName(), FuncPair_t(defaultInitFunc, 0));
	llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", defaultInitFunc);
	cgi->builder.SetInsertPoint(iblock);

	// create the local instance of reference to self
	llvm::Value* self = &defaultInitFunc->getArgumentList().front();



	for(VarDecl* var : this->members)
	{
		if(!var->isStatic)
		{
			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			llvm::Value* ptr = cgi->builder.CreateStructGEP(self, i, "memberPtr_" + var->name);

			auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
			var->doInitialValue(cgi, cgi->getType(var->type.strType), r.first, r.second, ptr, false);
		}
		else
		{
			// generate some globals.
			// mangle the variable name.

			// a bit hacky, but still well-defined.
			std::string varname = cgi->mangleMemberFunction(this, var->name, std::deque<Ast::Expr*>());

			// generate a global variable (sorry!).
			llvm::GlobalValue* gv = new llvm::GlobalVariable(*cgi->module, var->inferredLType, var->immutable,
				llvm::GlobalValue::ExternalLinkage, llvm::Constant::getNullValue(var->inferredLType), varname);

			if(var->inferredLType->isStructTy())
			{
				TypePair_t* cmplxtype = cgi->getType(var->inferredLType);
				iceAssert(cmplxtype);

				llvm::Function* init = cgi->getStructInitialiser(var, cmplxtype, { gv });
				cgi->addGlobalConstructor(varname, init);
			}
			else
			{
				iceAssert(var->initVal);
				llvm::Value* val = var->initVal->codegen(cgi, gv).result.first;
				if(llvm::isa<llvm::Constant>(val))
				{
					llvm::cast<llvm::GlobalVariable>(gv)->setInitializer(llvm::cast<llvm::Constant>(val));
				}
				else
				{
					error(this, "Global variables currently only support constant initialisers");
				}
			}
		}
	}

	// create all the other automatic init functions for our extensions
	std::deque<llvm::Function*> extensionInitialisers;
	{
		int i = 0;
		for(auto ext : this->extensions)
		{
			extensionInitialisers.push_back(ext->createAutomaticInitialiser(cgi, str, i));
			i++;
		}
	}



	// check if we have any user-defined init() functions.
	// if we do, then we can stick the extension-init()-calls there
	// if not, then we need to do it here

	bool didFindInit = false;
	for(Func* f : this->funcs)
	{
		if(f->decl->name == "init")
		{
			didFindInit = true;
			break;
		}
	}

	if(!didFindInit)
	{
		for(llvm::Function* f : extensionInitialisers)
		{
			llvm::Function* actual = cgi->module->getFunction(f->getName());
			cgi->builder.CreateCall(actual, self);
		}
	}

	cgi->builder.CreateRetVoid();
	llvm::verifyFunction(*defaultInitFunc);



	// add getters/setters (ie. computed properties)
	// as functions to simplify our code
	for(ComputedProperty* c : this->cprops)
	{
		VarDecl* fakeSelf = new VarDecl(c->posinfo, "self", true);
		fakeSelf->type = this->name + "*";

		if(c->getter)
		{
			std::deque<VarDecl*> params { fakeSelf };
			FuncDecl* fakeDecl = new FuncDecl(c->posinfo, "_get" + std::to_string(c->name.length()) + c->name, params, c->type.strType);
			Func* fakeFunc = new Func(c->posinfo, fakeDecl, c->getter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->generatedFunc = fakeDecl;
		}
		if(c->setter)
		{
			VarDecl* setterArg = new VarDecl(c->posinfo, c->setterArgName, true);
			setterArg->type = c->type;

			std::deque<VarDecl*> params { fakeSelf, setterArg };
			FuncDecl* fakeDecl = new FuncDecl(c->posinfo, "_set" + std::to_string(c->name.length()) + c->name, params, c->type.strType);
			Func* fakeFunc = new Func(c->posinfo, fakeDecl, c->setter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->generatedFunc = fakeDecl;
		}
	}






















	// issue here is that functions aren't codegened (ie. don't have the llvm::Function*)
	// before their bodies are codegened, so this makes functions in structs order-dependent.

	// pass 1
	for(Func* f : this->funcs)
	{
		llvm::BasicBlock* ob = cgi->builder.GetInsertBlock();
		bool isOpOverload = f->decl->name.find("operator#") == 0;
		if(isOpOverload)
			f->decl->name = f->decl->name.substr(strlen("operator#"));

		llvm::Value* val = nullptr;



		// hack:
		// 1. append 'E' to the end of the function's basename, as per C++ ABI
		// 2. remove the first varDecl of its parameters (self)
		// 3. mangle the name
		// 4. restore the parameter
		// this makes sure that we don't get ridiculous mangled names for member functions


		val = f->decl->codegen(cgi).result.first;

		if(f->decl->name == "init")
			this->initFuncs.push_back(llvm::cast<llvm::Function>(val));


		cgi->builder.SetInsertPoint(ob);
		this->lfuncs.push_back(llvm::cast<llvm::Function>(val));

		if(isOpOverload)
		{
			ArithmeticOp ao = cgi->determineArithmeticOp(f->decl->name);
			this->lOpOverloads.push_back(std::make_pair(ao, llvm::cast<llvm::Function>(val)));
		}

		// make the functions public as well
		cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(f->decl, llvm::cast<llvm::Function>(val)));
	}

	// pass 2
	for(Func* f : this->funcs)
	{
		llvm::BasicBlock* ob = cgi->builder.GetInsertBlock();

		if(f->decl->name == "init")
		{
			std::deque<Expr*> todeque;

			VarRef* svr = new VarRef(this->posinfo, "self");
			todeque.push_back(svr);

			for(auto extInit : extensionInitialisers)
				f->block->statements.push_front(new FuncCall(this->posinfo, extInit->getName(), todeque));

			f->block->statements.push_front(new FuncCall(this->posinfo, "__automatic_init__" + this->mangledName, todeque));
		}

		f->codegen(cgi);
		cgi->builder.SetInsertPoint(ob);
	}

	if(this->initFuncs.size() == 0)
		this->initFuncs.push_back(defaultInitFunc);

	cgi->rootNode->publicTypes.push_back(std::pair<Struct*, llvm::Type*>(this, str));
	cgi->rootNode->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(0, defaultInitFunc));
	return Result_t(nullptr, nullptr);
}

void Struct::createType(CodegenInstance* cgi)
{
	if(cgi->isDuplicateType(this->name))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Type);

	llvm::Type** types = new llvm::Type*[this->members.size()];

	// create a bodyless struct so we can use it
	this->mangledName = cgi->mangleWithNamespace(this->name, false);


	llvm::StructType* str = llvm::StructType::create(llvm::getGlobalContext(), this->mangledName);
	this->scope = cgi->namespaceStack;
	cgi->addNewType(str, this, TypeKind::Struct);

	if(!this->didCreateType)
	{
		// see if we have nested types
		for(auto nested : this->nestedTypes)
		{
			cgi->pushNamespaceScope(this->name);
			nested->createType(cgi);
			cgi->popNamespaceScope();
		}


		// because we can't (and don't want to) mangle names in the parser,
		// we could only build an incomplete name -> index map
		// finish it here.

		for(auto p : this->opOverloads)
			p->codegen(cgi);

		for(Func* func : this->funcs)
		{
			// only override if we don't have one.
			if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
				func->decl->attribs |= Attr_VisPublic;

			func->decl->parentStruct = this;
			std::string mangled = cgi->mangleFunctionName(func->decl->name, func->decl->params);
			if(this->nameMap.find(mangled) != this->nameMap.end())
			{
				error(cgi, this, "Duplicate member '%s'", func->decl->name.c_str());
			}
		}

		for(VarDecl* var : this->members)
		{
			var->inferType(cgi);
			llvm::Type* type = cgi->getLlvmType(var);
			if(type == str)
			{
				error(cgi, this, "Cannot have non-pointer member of type self");
			}

			cgi->applyExtensionToStruct(cgi->mangleWithNamespace(var->type.strType));
			if(!var->isStatic)
			{
				int i = this->nameMap[var->name];
				iceAssert(i >= 0);

				types[i] = cgi->getLlvmType(var);
			}
		}
	}


	std::vector<llvm::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec, this->packed);

	this->didCreateType = true;

	delete types;
}







Result_t OpOverload::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// this is never really called for actual codegen. operators are handled as functions,
	// so we just put them into the structs' funcs.
	// BinOp will do a lookup on the opMap, but never call codegen for this.

	// however, this will get called, because we need to know if the parameters for
	// the operator overload are legit. people ignore our return value.

	FuncDecl* decl = this->func->decl;
	if(this->op == ArithmeticOp::Assign)
	{
		if(decl->params.size() != 1)
		{
			error(cgi, this, "Operator overload for '=' can only have one argument (have %d)", decl->params.size());
		}

		// we can't actually do much, because they can assign to anything
	}
	else if(this->op == ArithmeticOp::CmpEq)
	{
		if(decl->params.size() != 1)
			error(cgi, this, "Operator overload for '==' can only have one argument");

		if(decl->type.strType != "Bool")
			error(cgi, this, "Operator overload for '==' must return a boolean value");
	}
	else if(this->op == ArithmeticOp::Add || this->op == ArithmeticOp::Subtract || this->op == ArithmeticOp::Multiply
		|| this->op == ArithmeticOp::Divide || this->op == ArithmeticOp::PlusEquals || this->op == ArithmeticOp::MinusEquals
		|| this->op == ArithmeticOp::MultiplyEquals || this->op == ArithmeticOp::DivideEquals)
	{
		if(decl->params.size() != 1)
			error(cgi, this, "Operator overload can only have one argument");
	}
	else
	{
		error(cgi, this, "(%s:%d) -> Internal check failed: invalid operator", __FILE__, __LINE__);
	}

	return Result_t(0, 0);
}















