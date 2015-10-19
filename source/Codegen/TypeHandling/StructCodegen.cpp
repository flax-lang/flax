// StructCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t Struct::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
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


	fir::StructType* str = dynamic_cast<fir::StructType*>(_type->first);
	cgi->module->addNamedType(str->getStructName(), str);

	cgi->rootNode->publicTypes.push_back(std::pair<StructBase*, fir::Type*>(this, str));

	// generate initialiser
	{
		fir::Function* defifunc = cgi->module->getOrCreateFunction("__auto_init__" + this->mangledName,
			fir::FunctionType::get({ str->getPointerTo() }, fir::PrimitiveType::getVoid(), false), linkageType);

		this->initFuncs.push_back(defifunc);


		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->pin, defifunc->getName(), { fakeSelf }, "Void");
		cgi->addFunctionToScope({ defifunc, fd });


		fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser" + this->name, defifunc);
		cgi->builder.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = defifunc->getArguments().front();

		for(VarDecl* var : this->members)
		{
			// not supported in structs
			iceAssert(!var->isStatic);

			int i = this->nameMap[var->name];
			iceAssert(i >= 0);

			fir::Value* ptr = cgi->builder.CreateGetConstStructMember(self, i);

			auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
			var->doInitialValue(cgi, cgi->getType(var->type.strType), r.first, r.second, ptr, false);
		}




		cgi->builder.CreateReturnVoid();
		// fir::verifyFunction(*defifunc);

		cgi->addPublicFunc({ defifunc, fd });
	}


	// create memberwise initialiser
	{
		std::vector<fir::Type*> types;
		types.push_back(str->getPointerTo());
		for(auto e : str->getElements())
			types.push_back(e);


		fir::Function* memifunc = cgi->module->getOrCreateFunction("__auto_mem_init__" + this->mangledName,
			fir::FunctionType::get(types, fir::PrimitiveType::getVoid(cgi->getContext()), false), linkageType);

		this->initFuncs.push_back(memifunc);

		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		std::deque<VarDecl*> args;
		args.push_back(fakeSelf);

		for(auto m : this->members)
			args.push_back(m);

		FuncDecl* fd = new FuncDecl(this->pin, memifunc->getName(), args, "Void");
		cgi->addFunctionToScope({ memifunc, fd });




		fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser" + this->name, memifunc);
		cgi->builder.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = memifunc->getArguments().front();


		for(size_t i = 0; i < this->members.size(); i++)
		{
			fir::Value* v = memifunc->getArguments()[i + 1];

			v->setName("memberPtr_" + std::to_string(i));
			fir::Value* ptr = cgi->builder.CreateGetConstStructMember(self, i);

			cgi->builder.CreateStore(v, ptr);
		}


		cgi->builder.CreateReturnVoid();
		// fir::verifyFunction(*memifunc);

		cgi->addPublicFunc({ memifunc, fd });
	}








	for(OpOverload* oo : this->opOverloads)
	{
		fir::IRBlock* ob = cgi->builder.getCurrentBlock();
		Func* f = oo->func;

		f->decl->name = f->decl->name.substr(9 /*strlen("operator#")*/ );
		f->decl->parentClass = this;

		fir::Value* val = f->decl->codegen(cgi).result.first;

		cgi->builder.setCurrentBlock(ob);
		ArithmeticOp ao = cgi->determineArithmeticOp(f->decl->name);
		this->lOpOverloads.push_back(std::make_pair(ao, dynamic_cast<fir::Function*>(val)));

		// make the functions public as well
		cgi->addPublicFunc({ dynamic_cast<fir::Function*>(val), f->decl });


		ob = cgi->builder.getCurrentBlock();

		oo->func->codegen(cgi);
		cgi->builder.setCurrentBlock(ob);
	}


	return Result_t(0, 0);
}








fir::Type* Struct::createType(CodegenInstance* cgi)
{
	if(this->didCreateType)
		return 0;

	// check our inheritances??
	fir::Type** types = new fir::Type*[this->members.size()];

	// create a bodyless struct so we can use it
	this->mangledName = cgi->mangleWithNamespace(this->name, cgi->getFullScope(), false);

	if(cgi->isDuplicateType(this->mangledName))
		GenError::duplicateSymbol(cgi, this, this->name, SymbolType::Type);



	fir::StructType* str = fir::StructType::createNamedWithoutBody(this->mangledName, cgi->getContext(), this->packed);

	this->scope = cgi->namespaceStack;
	cgi->addNewType(str, this, TypeKind::Struct);




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

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		fir::Type* type = cgi->getLlvmType(var);
		if(type == str)
		{
			error(this, "Cannot have non-pointer member of type self");
		}

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




















