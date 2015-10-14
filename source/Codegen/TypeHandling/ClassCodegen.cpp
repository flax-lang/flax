// ClassCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"

using namespace Ast;
using namespace Codegen;


Result_t Class::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	iceAssert(this->didCreateType);
	TypePair_t* _type = cgi->getType(this->name);
	if(!_type)
		_type = cgi->getType(this->mangledName);

	if(!_type)
		GenError::unknownSymbol(cgi, this, this->name + " (mangled: " + this->mangledName + ")", SymbolType::Type);


	// if we're already done, don't.
	if(this->didCodegen)
		return Result_t(0, 0);

	this->didCodegen = true;

	fir::LinkageType linkageType;
	if(this->attribs & Attr_VisPublic)
	{
		linkageType = fir::LinkageType::External;
	}
	else
	{
		linkageType = fir::LinkageType::Internal;
	}






	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.first->codegen(cgi);
		cgi->popNestedTypeScope();
	}


	fir::StructType* str = dynamic_cast<fir::StructType*>(_type->first);

	// generate initialiser
	fir::Function* defaultInitFunc = new fir::Function("__auto_init__" + this->mangledName,
		fir::FunctionType::getFunction({ str->getPointerTo() }, fir::PrimitiveType::getVoid(cgi->getContext()), false),
		cgi->module, linkageType);

	{
		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->pin, defaultInitFunc->getName(), { fakeSelf }, "Void");
		cgi->addFunctionToScope({ defaultInitFunc, fd });
	}


	fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser", defaultInitFunc);
	cgi->builder.setCurrentBlock(iblock);

	// create the local instance of reference to self
	fir::Value* self = defaultInitFunc->getArguments().front();

	for(VarDecl* var : this->members)
	{
		if(!var->isStatic)
		{
			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			fir::Value* ptr = cgi->builder.CreateGetConstStructMember(self, i);

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
			fir::GlobalVariable* gv = new fir::GlobalVariable(varname, cgi->module, var->inferredLType, var->immutable,
				fir::LinkageType::External, fir::ConstantValue::getNullValue(var->inferredLType));

			if(var->inferredLType->isStructType())
			{
				TypePair_t* cmplxtype = cgi->getType(var->inferredLType);
				iceAssert(cmplxtype);

				fir::Function* init = cgi->getStructInitialiser(var, cmplxtype, { gv });
				cgi->addGlobalConstructor(varname, init);
			}
			else
			{
				iceAssert(var->initVal);
				fir::Value* val = var->initVal->codegen(cgi, gv).result.first;
				if(dynamic_cast<fir::ConstantValue*>(val))
				{
					gv->setInitialValue(dynamic_cast<fir::ConstantValue*>(val));
				}
				else
				{
					error(this, "Global variables currently only support constant initialisers");
				}
			}
		}
	}

	// create all the other automatic init functions for our extensions
	std::deque<fir::Function*> extensionInitialisers;
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
		for(fir::Function* f : extensionInitialisers)
		{
			fir::Function* actual = cgi->module->getFunction(f->getName());
			cgi->builder.CreateCall1(actual, self);
		}
	}

	cgi->builder.CreateReturnVoid();
	// fir::verifyFunction(*defaultInitFunc);



	// add getters/setters (ie. computed properties)
	// as functions to simplify our code
	for(ComputedProperty* c : this->cprops)
	{
		VarDecl* fakeSelf = new VarDecl(c->pin, "self", true);
		fakeSelf->type = this->name + "*";

		if(c->getter)
		{
			std::deque<VarDecl*> params { fakeSelf };
			FuncDecl* fakeDecl = new FuncDecl(c->pin, "_get" + std::to_string(c->name.length()) + c->name, params, c->type.strType);
			Func* fakeFunc = new Func(c->pin, fakeDecl, c->getter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->getterFunc = fakeDecl;
		}
		if(c->setter)
		{
			VarDecl* setterArg = new VarDecl(c->pin, c->setterArgName, true);
			setterArg->type = c->type;

			std::deque<VarDecl*> params { fakeSelf, setterArg };
			FuncDecl* fakeDecl = new FuncDecl(c->pin, "_set" + std::to_string(c->name.length()) + c->name, params, "Void");
			Func* fakeFunc = new Func(c->pin, fakeDecl, c->setter);

			if((this->attribs & Attr_VisPublic) /*&& !(c->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic))*/)
				fakeDecl->attribs |= Attr_VisPublic;

			this->funcs.push_back(fakeFunc);
			c->setterFunc = fakeDecl;
		}
	}






















	// issue here is that functions aren't codegened (ie. don't have the fir::Function*)
	// before their bodies are codegened, so this makes functions in structs order-dependent.

	// pass 1
	for(Func* f : this->funcs)
	{
		fir::IRBlock* ob = cgi->builder.getCurrentBlock();
		bool isOpOverload = f->decl->name.find("operator#") == 0;
		if(isOpOverload)
			f->decl->name = f->decl->name.substr(9 /*strlen("operator#")*/ );

		fir::Value* val = nullptr;



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
			this->initFuncs.push_back(dynamic_cast<fir::Function*>(val));


		cgi->builder.setCurrentBlock(ob);
		this->lfuncs.push_back(dynamic_cast<fir::Function*>(val));

		if(isOpOverload)
		{
			ArithmeticOp ao = cgi->determineArithmeticOp(f->decl->name);
			this->lOpOverloads.push_back(std::make_pair(ao, dynamic_cast<fir::Function*>(val)));
		}

		// make the functions public as well
		cgi->addPublicFunc({ dynamic_cast<fir::Function*>(val), f->decl });
	}

	// pass 2
	for(Func* f : this->funcs)
	{
		fir::IRBlock* ob = cgi->builder.getCurrentBlock();

		if(f->decl->name == "init")
		{
			std::deque<Expr*> todeque;

			VarRef* svr = new VarRef(this->pin, "self");
			todeque.push_back(svr);


			for(auto extInit : extensionInitialisers)
				f->block->statements.push_front(new FuncCall(this->pin, extInit->getName(), todeque));

			f->block->statements.push_front(new FuncCall(this->pin, "__auto_init__" + this->mangledName, todeque));
		}

		f->codegen(cgi);
		cgi->builder.setCurrentBlock(ob);
	}

	if(this->initFuncs.size() == 0)
		this->initFuncs.push_back(defaultInitFunc);

	cgi->rootNode->publicTypes.push_back(std::pair<StructBase*, fir::Type*>(this, str));
	cgi->addPublicFunc({ defaultInitFunc, 0 });





	return Result_t(nullptr, nullptr);
}








fir::Type* Class::createType(CodegenInstance* cgi)
{
	if(this->didCreateType)
		return 0;

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
			error(this, "Type %s does not exist", super.c_str());

		if(type->second.second == TypeKind::Class)
		{
			if(alreadyHaveSuperclass)
			{
				error(this, "Multiple inheritance is not supported, only one superclass"
					" can be inherited from. Consider using protocols instead");
			}

			alreadyHaveSuperclass = true;
		}
		else if(type->second.second != TypeKind::Protocol)
		{
			error(this, "%s is neither a protocol nor a class, and cannot be inherited from", super.c_str());
		}


		Class* supcls = dynamic_cast<Class*>(type->second.first);
		assert(supcls);

		// this will (should) do a recursive thing where they copy all their superclassed methods into themselves
		// by the time we see it.
		supcls->createType(cgi);


		// if it's a struct, copy its members into ourselves.
		if(type->second.second == TypeKind::Class)
		{
			this->superclass = { supcls, type->first->toStructType() };

			// normal members
			for(auto mem : supcls->members)
			{
				auto pred = [mem](VarDecl* v) -> bool {

					return v->name == mem->name;
				};

				auto it = std::find_if(this->members.begin(), this->members.end(), pred);
				if(it != this->members.end())
				{
					error(*it, "Fields cannot be overriden, only computed properties can");
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
						error(f->decl, "Overriding function '%s' in superclass %s requires 'override' keyword",
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
						error(ours, "Overriding computed property '%s' in superclass %s needs 'override' keyword",
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
			error(this, "enotsup");
		}
	}




	fir::Type** types = new fir::Type*[this->members.size()];

	// create a bodyless struct so we can use it
	std::deque<std::string> fullScope = cgi->getFullScope();
	this->mangledName = cgi->mangleWithNamespace(this->name, fullScope, false);

	if(cgi->isDuplicateType(this->mangledName))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Type);


	fir::StructType* str = fir::StructType::getOrCreateNamedStruct(this->mangledName, { }, cgi->getContext());

	this->scope = fullScope;
	cgi->addNewType(str, this, TypeKind::Class);








	// because we can't (and don't want to) mangle names in the parser,
	// we could only build an incomplete name -> index map
	// finish it here.

	for(auto p : this->opOverloads)
	{
		// before calling codegen (that checks for valid overloads), insert the "self" parameter
		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);

		std::string fulltype;
		for(auto s : cgi->getFullScope())
			fulltype += s + "::";

		fakeSelf->type = fulltype + this->name + "*";

		p->func->decl->params.push_front(fakeSelf);

		p->codegen(cgi);

		// remove it after
		iceAssert(p->func->decl->params.front() == fakeSelf);
		p->func->decl->params.pop_front();
	}

	for(Func* func : this->funcs)
	{
		// only override if we don't have one.
		if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			func->decl->attribs |= Attr_VisPublic;

		func->decl->parentClass = this;
		std::string mangled = cgi->mangleFunctionName(func->decl->name, func->decl->params);
		if(this->nameMap.find(mangled) != this->nameMap.end())
		{
			error(this, "Duplicate member '%s'", func->decl->name.c_str());
		}
	}

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		// fir::Type* type = cgi->getLlvmType(var);

		iceAssert(var->inferredLType != 0);
		fir::Type* type = var->inferredLType;

		if(type == str)
		{
			error(this, "Cannot have non-pointer member of type self");
		}

		cgi->applyExtensionToStruct(cgi->mangleWithNamespace(var->type.strType));
		if(!var->isStatic)
		{
			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			types[i] = cgi->getLlvmType(var);
		}
	}










	std::vector<fir::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec);

	this->didCreateType = true;

	delete types;

	return str;
}




















