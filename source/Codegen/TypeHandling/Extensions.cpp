// ExtensionCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "classbase.h"

using namespace Ast;
using namespace Codegen;

fir::Type* ExtensionDef::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->createdType == 0)
		return this->createType(cgi);

	else return this->createdType;
}

fir::Type* ExtensionDef::createType(CodegenInstance* cgi)
{
	this->ident.scope = cgi->getFullScope();
	this->parentRoot = cgi->rootNode;

	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.second = nested.first->createType(cgi);
		cgi->popNestedTypeScope();
	}


	for(Func* func : this->funcs)
	{
		// only override if we don't have one.
		if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			func->decl->attribs |= Attr_VisPublic;

		func->decl->parentClass = this;
	}


	FunctionTree* ft = cgi->getCurrentFuncTree();
	{
		ft->extensions.insert(std::make_pair(this->ident.name, this));

		if(this->attribs & Attr_VisPublic)
			cgi->getCurrentFuncTree(0, cgi->rootNode->publicFuncTree)->extensions.insert(std::make_pair(this->ident.name, this));
	}

	this->didCreateType = true;

	if(cgi->getExprTypeOfBuiltin(this->ident.str()))
	{
		this->createdType = cgi->getExprTypeOfBuiltin(this->ident.str());
		return this->createdType;
	}
	else
	{
		TypePair_t* tp = cgi->getType(this->ident);

		if(!tp) error(this, "Type %s does not exist in the scope %s", this->ident.name.c_str(), this->ident.str().c_str());
		else if(tp->second.second != TypeKind::Class && tp->second.second != TypeKind::Struct)
			error(this, "Extensions can only be applied to classes or structs");

		iceAssert(tp->first);

		this->createdType = tp->first;
		return tp->first;
	}
}





Result_t ExtensionDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->isDuplicate || this->didCodegen)
		return Result_t(0, 0);

	this->didCodegen = true;
	iceAssert(this->didCreateType);


	// do the thing
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

	TypePair_t* tp = 0;
	fir::Type* fstr = cgi->getExprTypeOfBuiltin(this->ident.str());

	if(fstr == 0)
	{
		tp = cgi->getType(this->ident);
		iceAssert(tp);
		iceAssert(tp->first);
		iceAssert(tp->second.first);

		fstr = tp->first;
	}


	iceAssert(fstr);



	// check for duplicates in other extensions
	for(auto ext : (fstr->isClassType() || fstr->isStructType()
		? cgi->getExtensionsForType(dynamic_cast<StructBase*>(tp->second.first)) : cgi->getExtensionsForBuiltinType(fstr)))
	{
		if(ext == this) continue;

		for(auto e : ext->cprops)
		{
			for(auto cp : this->cprops)
			{
				if(e->ident.name == cp->ident.name)
				{
					errorNoExit(cp, "Property '%s' was previously declared in another extension", cp->ident.name.c_str());
					info(e, "Previous declaration was here:");
					doTheExit();
				}
			}
		}

		for(auto f : ext->funcs)
		{
			FuncDefPair fp(0, f->decl, f);
			for(auto tf : this->funcs)
			{
				if(f->decl->ident.name == tf->decl->ident.name)
				{
					std::deque<fir::Type*> ps;
					for(auto p : tf->decl->params)
						ps.push_back(p->getType(cgi, true));	// allow fail

					int _ = 0;
					if(cgi->isValidFuncOverload(fp, ps, &_, true))
					{
						errorNoExit(tf->decl, "Function '%s' was previously declared in another extension, with an identical type",
							tf->decl->ident.name.c_str());

						info(f->decl, "Previous declaration was here:");
						doTheExit();
					}
				}
			}
		}

		for(auto f : ext->operatorOverloads)
		{
			FuncDefPair fp(0, f->func->decl, f->func);
			for(auto tf : this->operatorOverloads)
			{
				if(f->op == tf->op)
				{
					std::deque<fir::Type*> ps;
					for(auto p : tf->func->decl->params)
						ps.push_back(p->getType(cgi, true));	// allow fail

					int _ = 0;
					if(cgi->isValidFuncOverload(fp, ps, &_, true))
					{
						errorNoExit(tf->func->decl, "Operator overload for '%s' was previously declared in another extension",
							Parser::arithmeticOpToString(cgi, tf->op).c_str());

						info(f->func->decl, "Previous declaration was here:");
						doTheExit();
					}
				}
			}
		}

		for(auto f : ext->subscriptOverloads)
		{
			FuncDefPair fp(0, f->getterFn->decl, f->getterFn);
			for(auto tf : this->subscriptOverloads)
			{
				std::deque<fir::Type*> ps;
				for(auto p : tf->decl->params)
					ps.push_back(p->getType(cgi, true));	// allow fail

				int _ = 0;
				if(cgi->isValidFuncOverload(fp, ps, &_, true))
				{
					errorNoExit(tf->decl, "Subscript operator was previously declared in another extension");
					info(f->decl, "Previous declaration was here:");
					doTheExit();
				}
			}
		}

		for(auto f : ext->assignmentOverloads)
		{
			FuncDefPair fp(0, f->func->decl, f->func);
			for(auto tf : this->assignmentOverloads)
			{
				std::deque<fir::Type*> ps;
				for(auto p : tf->func->decl->params)
					ps.push_back(p->getType(cgi, true));	// allow fail

				int _ = 0;
				if(cgi->isValidFuncOverload(fp, ps, &_, true))
				{
					errorNoExit(tf->func->decl, "Assignment operator was previously declared in another extension");
					info(f->func->decl, "Previous declaration was here:");
					doTheExit();
				}
			}
		}
	}









	doCodegenForMemberFunctions(cgi, this);
	doCodegenForComputedProperties(cgi, this);


	// only allow these funny shennanigans if we're extending a class
	fir::Function* defaultInit = 0;
	if(fstr->isClassType() || fstr->isStructType())
	{
		StructBase* astr = dynamic_cast<StructBase*>(tp->second.first);
		iceAssert(astr);

		// note: poor copy-pasta

		for(auto e : astr->members)
		{
			for(auto cp : this->cprops)
			{
				if(e->ident.name == cp->ident.name)
				{
					errorNoExit(cp, "Property '%s' already exists in the base type", cp->ident.name.c_str());
					info(e, "Previous declaration was here:");
					doTheExit();
				}
			}
		}

		if(ClassDef* cd = dynamic_cast<ClassDef*>(astr))
		{
			for(auto f : cd->funcs)
			{
				FuncDefPair fp(0, f->decl, f);
				for(auto tf : this->funcs)
				{
					if(f->decl->ident.name == tf->decl->ident.name)
					{
						std::deque<fir::Type*> ps;
						for(auto p : tf->decl->params)
							ps.push_back(p->getType(cgi, true));	// allow fail

						int _ = 0;
						if(cgi->isValidFuncOverload(fp, ps, &_, true))
						{
							errorNoExit(tf->decl, "Function '%s' already exists in the base type taking identical arguments",
								tf->decl->ident.name.c_str());

							info(f->decl, "Previous declaration was here:");
							doTheExit();
						}
					}
				}
			}

			for(auto f : cd->operatorOverloads)
			{
				FuncDefPair fp(0, f->func->decl, f->func);
				for(auto tf : this->operatorOverloads)
				{
					if(f->op == tf->op)
					{
						std::deque<fir::Type*> ps;
						for(auto p : tf->func->decl->params)
							ps.push_back(p->getType(cgi, true));	// allow fail

						int _ = 0;
						if(cgi->isValidFuncOverload(fp, ps, &_, true))
						{
							errorNoExit(tf->func->decl, "Operator overload for '%s' already exists in the base type",
								Parser::arithmeticOpToString(cgi, tf->op).c_str());

							info(f->func->decl, "Previous declaration was here:");
							doTheExit();
						}
					}
				}
			}

			for(auto f : cd->subscriptOverloads)
			{
				FuncDefPair fp(0, f->getterFn->decl, f->getterFn);
				for(auto tf : this->subscriptOverloads)
				{
					std::deque<fir::Type*> ps;
					for(auto p : tf->decl->params)
						ps.push_back(p->getType(cgi, true));	// allow fail

					int _ = 0;
					if(cgi->isValidFuncOverload(fp, ps, &_, true))
					{
						errorNoExit(tf->decl, "Subscript operator already exists in the base type");
						info(f->decl, "Previous declaration was here:");
						doTheExit();
					}
				}
			}

			for(auto f : cd->assignmentOverloads)
			{
				FuncDefPair fp(0, f->func->decl, f->func);
				for(auto tf : this->assignmentOverloads)
				{
					std::deque<fir::Type*> ps;
					for(auto p : tf->func->decl->params)
						ps.push_back(p->getType(cgi, true));	// allow fail

					int _ = 0;
					if(cgi->isValidFuncOverload(fp, ps, &_, true))
					{
						errorNoExit(tf->func->decl, "Assignment operator already exists in the base type");
						info(f->func->decl, "Previous declaration was here:");
						doTheExit();
					}
				}
			}
		}















		iceAssert(astr->defaultInitialiser);
		defaultInit = cgi->module->getOrCreateFunction(astr->defaultInitialiser->getName(), astr->defaultInitialiser->getType(),
			astr->defaultInitialiser->linkageType);

		generateDeclForOperators(cgi, this);

		doCodegenForGeneralOperators(cgi, this);
		doCodegenForAssignmentOperators(cgi, this);
		doCodegenForSubscriptOperators(cgi, this);
	}


	for(Func* f : this->funcs)
	{
		if(!fstr->isClassType() && f->decl->ident.name == "init")
			error(f->decl, "Extended initialisers can only be declared on class types");

		generateMemberFunctionBody(cgi, this, f, defaultInit);
	}



	for(auto protstr : this->protocolstrs)
	{
		ProtocolDef* prot = cgi->resolveProtocolName(this, protstr);
		iceAssert(prot);

		prot->assertTypeConformity(cgi, fstr);
		this->conformedProtocols.push_back(prot);
	}




	return Result_t(0, 0);
}













































