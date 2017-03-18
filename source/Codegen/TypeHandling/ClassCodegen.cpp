// ClassCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "classbase.h"

using namespace Ast;
using namespace Codegen;



fir::Type* ClassDef::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	if(this->createdType == 0)
		return this->createType(cgi);

	else return this->createdType;
}

Result_t ClassDef::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	this->createType(cgi);

	TypePair_t* _type = cgi->getType(this->ident);
	if(!_type) error(this, "how? generating class (%s) without type", this->ident.name.c_str());




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
		cgi->pushNamespaceScope(this->ident.name);

		nested.first->codegen(cgi);

		cgi->popNamespaceScope();
		cgi->popNestedTypeScope();
	}


	fir::ClassType* cls = this->createdType->toClassType();

	// generate initialiser
	{
		auto defaultInitId = this->ident;
		defaultInitId.kind = IdKind::AutoGenFunc;
		defaultInitId.name = "init_" + defaultInitId.name;
		defaultInitId.functionArguments = { cls->getPointerTo() };

		this->defaultInitialiser = cgi->module->getOrCreateFunction(defaultInitId, fir::FunctionType::get({ cls->getPointerTo() },
			fir::Type::getVoid(cgi->getContext()), false), linkageType);


		fir::IRBlock* currentblock = cgi->irb.getCurrentBlock();

		fir::IRBlock* iblock = cgi->irb.addNewBlockInFunction("initialiser_" + this->ident.name, this->defaultInitialiser);
		cgi->irb.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = this->defaultInitialiser->getArguments().front();

		for(VarDecl* var : this->members)
		{
			if(!var->isStatic)
			{
				fir::Value* ptr = cgi->irb.CreateGetStructMember(self, var->ident.name);

				auto r = var->initVal ? var->initVal->codegen(cgi) : Result_t(0, 0);

				var->inferType(cgi);
				var->doInitialValue(cgi, cgi->getType(var->concretisedType), r.value, r.pointer, ptr, false, r.valueKind);
			}
			else
			{
				// generate some globals for static variables.

				auto tmp = this->ident.scope;
				tmp.push_back(this->ident.name);

				Identifier vid = Identifier(var->ident.name, tmp, IdKind::Variable);

				// generate a global variable
				fir::GlobalVariable* gv = cgi->module->createGlobalVariable(vid, var->concretisedType,
					fir::ConstantValue::getZeroValue(var->concretisedType), var->immutable,
					(this->attribs & Attr_VisPublic) ? fir::LinkageType::External : fir::LinkageType::Internal);

				if(var->concretisedType->isStructType() || var->concretisedType->isClassType())
				{
					TypePair_t* cmplxtype = cgi->getType(var->concretisedType);
					iceAssert(cmplxtype);

					fir::Function* init = cgi->getStructInitialiser(var, cmplxtype, { gv });
					cgi->addGlobalConstructor(vid, init);
				}
				else
				{
					iceAssert(var->initVal);
					fir::Value* val = var->initVal->codegen(cgi, var->concretisedType, gv).value;
					if(dynamic_cast<fir::ConstantValue*>(val))
					{
						gv->setInitialValue(dynamic_cast<fir::ConstantValue*>(cgi->autoCastType(var->concretisedType, val)));
					}
					else
					{
						error(this, "Static variables currently only support constant initialisers");
					}
				}
			}
		}

		cgi->irb.CreateReturnVoid();
		cgi->irb.setCurrentBlock(currentblock);
	}







	// generate the decls before the bodies, so we can (a) call recursively, and (b) call other member functions independent of
	// order of declaration.


	// pass 1
	doCodegenForMemberFunctions(cgi, this);
	{
		cls->setMethods(this->lfuncs);
	}


	// do comprops here:
	// 1. we need to generate the decls separately (because they're fake)
	// 2. we need to *get* the fir::Function* to store somewhere to retrieve later
	// 3. we need the rest of the member decls to be in place, so we can call member functions
	// from the getters/setters.
	doCodegenForComputedProperties(cgi, this);

	// same reasoning for operators -- we need to 1. be able to call methods in the operator, and 2. call operators from the methods
	generateDeclForOperators(cgi, this);

	doCodegenForGeneralOperators(cgi, this);
	doCodegenForAssignmentOperators(cgi, this);
	doCodegenForSubscriptOperators(cgi, this);


	// pass 2
	for(Func* f : this->funcs)
	{
		generateMemberFunctionBody(cgi, this, f, this->defaultInitialiser);
	}









	for(auto protstr : this->protocolstrs)
	{
		ProtocolDef* prot = cgi->resolveProtocolName(this, protstr);
		iceAssert(prot);

		prot->assertTypeConformity(cgi, cls);
		this->conformedProtocols.push_back(prot);
	}




















	if(initFuncs.size() == 0)
	{
		this->initFuncs.push_back(this->defaultInitialiser);
	}
	else
	{
		// handles generic types making more default initialisers

		bool found = false;
		for(auto f : initFuncs)
		{
			if(f->getType() == this->defaultInitialiser->getType())
			{
				found = true;
				break;
			}
		}

		if(!found)
			this->initFuncs.push_back(this->defaultInitialiser);
	}

	// cgi->addPublicFunc(FuncDefPair(this->defaultInitialiser, 0, 0));

	return Result_t(0, 0);
}































fir::Type* ClassDef::createType(CodegenInstance* cgi)
{
	if(this->didCreateType)
		return this->createdType;


	// this->ident.scope = cgi->getFullScope();


	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		cgi->pushNamespaceScope(this->ident.name);

		nested.second = nested.first->createType(cgi);

		cgi->popNamespaceScope();
		cgi->popNestedTypeScope();
	}



	std::vector<std::pair<std::string, fir::Type*>> types;



	// create a bodyless struct so we can use it

	if(cgi->isDuplicateType(this->ident))
		GenError::duplicateSymbol(cgi, this, this->ident.str(), SymbolType::Type);

	fir::ClassType* cls = fir::ClassType::createWithoutBody(this->ident, cgi->getContext());

	iceAssert(this->createdType == 0);
	cgi->addNewType(cls, this, TypeKind::Class);





	for(Func* func : this->funcs)
	{
		// only override if we don't have one.
		if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			func->decl->attribs |= Attr_VisPublic;

		func->decl->parentClass = this;
	}

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		iceAssert(var->concretisedType != 0);

		fir::Type* type = var->concretisedType;

		if(type == cls)
		{
			error(var, "Cannot have non-pointer member of type self");
		}

		if(!var->isStatic)
		{
			types.push_back({ var->ident.name, var->getType(cgi) });
		}
	}

	cls->setMembers(types);
	this->didCreateType = true;

	this->createdType = cls;
	cgi->module->addNamedType(cls->getClassName(), cls);

	return cls;
}




















