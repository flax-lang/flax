// ClassCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"

using namespace Ast;
using namespace Codegen;


Result_t Class::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{

	iceAssert(this->didCreateType);
	TypePair_t* _type = cgi->getType(this->name);
	if(!_type)
		_type = cgi->getType(this->mangledName);

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


	cgi->pushNestedTypeScope(this);




	// see if we have nested types
	for(auto nested : this->nestedTypes)
		nested.first->codegen(cgi);


	llvm::StructType* str = llvm::cast<llvm::StructType>(_type->first);

	// generate initialiser
	llvm::Function* defaultInitFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(str, 0), false), linkageType, "__automatic_init__" + this->mangledName, cgi->module);

	{
		VarDecl* fakeSelf = new VarDecl(this->posinfo, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->posinfo, defaultInitFunc->getName(), { fakeSelf }, "Void");
		cgi->addFunctionToScope({ defaultInitFunc, fd });
	}


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
			// generate some globals for static variables.
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
			c->getterFunc = fakeDecl;
		}
		if(c->setter)
		{
			VarDecl* setterArg = new VarDecl(c->posinfo, c->setterArgName, true);
			setterArg->type = c->type;

			std::deque<VarDecl*> params { fakeSelf, setterArg };
			FuncDecl* fakeDecl = new FuncDecl(c->posinfo, "_set" + std::to_string(c->name.length()) + c->name, params, "Void");
			Func* fakeFunc = new Func(c->posinfo, fakeDecl, c->setter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->setterFunc = fakeDecl;
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
			f->decl->name = f->decl->name.substr(9 /*strlen("operator#")*/ );

		llvm::Value* val = nullptr;



		// hack:
		// 1. append 'E' to the end of the function's basename, as per C++ ABI
		// 2. remove the first varDecl of its parameters (self)
		// 3. mangle the name
		// 4. restore the parameter
		// this makes sure that we don't get ridiculous mangled names for member functions
		// also makes sure that we conform to the C++ ABI
		// (using 'E' means we don't include the implicit first parameter in the mangled name)


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
		cgi->addPublicFunc({ llvm::cast<llvm::Function>(val), f->decl });
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

	cgi->rootNode->publicTypes.push_back(std::pair<StructBase*, llvm::Type*>(this, str));
	cgi->addPublicFunc({ defaultInitFunc, 0 });





	cgi->popNestedTypeScope();
	return Result_t(nullptr, nullptr);
}








llvm::Type* Class::createType(CodegenInstance* cgi)
{
	if(this->didCreateType)
		return 0;

	if(cgi->isDuplicateType(this->name))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Type);






	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.second = nested.first->createType(cgi);
		cgi->popNestedTypeScope();
	}




	// check our inheritances??
	bool alreadyHaveSuperclass = false;
	for(auto super : this->protocolstrs)
	{
		TypePair_t* type = cgi->getType(super);
		if(type == 0)
			error(cgi, this, "Type %s does not exist", super.c_str());

		if(type->second.second == TypeKind::Class)
		{
			if(alreadyHaveSuperclass)
			{
				error(cgi, this, "Multiple inheritance is not supported, only one superclass"
					" can be inherited from. Consider using protocols instead");
			}

			alreadyHaveSuperclass = true;
		}
		else if(type->second.second != TypeKind::Protocol)
		{
			error(cgi, this, "%s is neither a protocol nor a class, and cannot be inherited from", super.c_str());
		}


		Class* supcls = dynamic_cast<Class*>(type->second.first);
		assert(supcls);

		// this will (should) do a recursive thing where they copy all their superclassed methods into themselves
		// by the time we see it.
		supcls->createType(cgi);


		// if it's a struct, copy its members into ourselves.
		if(type->second.second == TypeKind::Class)
		{
			this->superclass = { supcls, llvm::cast<llvm::StructType>(type->first) };

			// normal members
			for(auto mem : supcls->members)
			{
				auto pred = [mem](VarDecl* v) -> bool {

					return v->name == mem->name;
				};

				auto it = std::find_if(this->members.begin(), this->members.end(), pred);
				if(it != this->members.end())
				{
					error(cgi, *it, "Struct fields cannot be overriden, only computed properties can");
				}

				this->members.push_back(mem);
			}

			size_t nms = this->nameMap.size();
			for(auto nm : supcls->nameMap)
			{
				this->nameMap[nm.first] = nms;
				nms++;
			}

			// functions
			for(auto fn : supcls->funcs)
			{
				auto pred = [fn, cgi](Func* f) -> bool {

					if(fn->decl->params.size() != f->decl->params.size())
						return false;

					for(size_t i = 0; i < fn->decl->params.size(); i++)
					{
						if(cgi->getLlvmType(fn->decl->params[i]) != cgi->getLlvmType(f->decl->params[i]))
							return false;
					}

					return fn->decl->name == f->decl->name;
				};


				auto it = std::find_if(this->funcs.begin(), this->funcs.end(), pred);
				if(it != this->funcs.end())
				{
					// check for 'override'
					Func* f = *it;
					if(!(f->decl->attribs & Attr_Override))
					{
						error(cgi, f->decl, "Overriding function '%s' in superclass %s requires 'override' keyword",
							cgi->printAst(f->decl).c_str(), supcls->name.c_str());
					}
					else
					{
						// don't add the superclass one.
						continue;
					}
				}

				this->funcs.push_back((Func*) cgi->cloneAST(fn));
			}






			// computed properties
			for(auto cp : supcls->cprops)
			{
				auto pred = [cp](ComputedProperty* cpr) -> bool {

					return cp->name == cpr->name;
				};

				auto it = std::find_if(this->cprops.begin(), this->cprops.end(), pred);
				if(it != this->cprops.end())
				{
					// this thing exists.
					// check if ours has an override
					ComputedProperty* ours = *it;
					assert(ours->name == cp->name);

					if(!(ours->attribs & Attr_Override))
					{
						error(cgi, ours, "Overriding computed property '%s' in superclass %s needs 'override' keyword",
							ours->name.c_str(), supcls->name.c_str());
					}
					else
					{
						// we have 'override'.
						// disable this property, don't add it.
						continue;
					}
				}

				this->cprops.push_back((ComputedProperty*) cgi->cloneAST(cp));
			}
		}
		else
		{
			// protcols not supported yet.
			error(cgi, this, "enotsup");
		}
	}




	llvm::Type** types = new llvm::Type*[this->members.size()];

	// create a bodyless struct so we can use it
	this->mangledName = cgi->mangleWithNamespace(this->name, cgi->getNestedTypeList(), false);


	llvm::StructType* str = llvm::StructType::create(llvm::getGlobalContext(), this->mangledName);
	this->scope = cgi->namespaceStack;
	cgi->addNewType(str, this, TypeKind::Class);








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

		func->decl->parentClass = this;
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










	std::vector<llvm::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec);

	this->didCreateType = true;

	delete types;

	return str;
}




















