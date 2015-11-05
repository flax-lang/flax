// DotOperatorCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


static Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, fir::Value* ref, Class* str, bool isStaticFunctionCall);
static Result_t doVariable(CodegenInstance* cgi, VarRef* var, fir::Value* ref, StructBase* str, int i);
static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cp, fir::Value* _rhs, fir::Value* ref, Class* str);
// static Result_t doBaseClass(CodegenInstance* cgi, ComputedProperty* cp, fir::Value* _rhs, fir::Value* ref, Class* str);


Result_t ComputedProperty::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// handled elsewhere.
	return Result_t(0, 0);
}

Result_t CodegenInstance::getStaticVariable(Expr* user, Class* str, std::string name)
{
	std::string mangledName = this->mangleMemberFunction(str, name, std::deque<Ast::Expr*>());
	if(fir::GlobalVariable* gv = this->module->getGlobalVariable(mangledName))
	{
		// todo: another kinda hacky thing.
		// this is present in some parts of the code, i don't know how many.
		// basically, if the thing is supposed to be immutable, we're not going to return
		// the ptr/ref value.

		return Result_t(this->builder.CreateLoad(gv), gv);
	}

	error(user, "Struct '%s' has no such static member '%s'", str->name.c_str(), name.c_str());
}


Result_t MemberAccess::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* _rhs)
{
	if(this->matype != MAType::LeftVariable && this->matype != MAType::LeftFunctionCall)
	{
		if(this->matype == MAType::Invalid) error(this, "??");
		return cgi->resolveStaticDotOperator(this, true).second;
	}



	// gen the var ref on the left.
	Result_t res = this->cachedCodegenResult;
	if(res.result.first == 0 && res.result.second == 0)
	{
		res = this->left->codegen(cgi);
	}
	else
	{
		error("");
		// reset this?
		// this->cachedCodegenResult = Result_t(0, 0);
	}

	ValPtr_t p = res.result;

	fir::Value* self = p.first;
	fir::Value* selfPtr = p.second;

	// info(this, "self type = %s, self ptr type = %s", self->getType()->str().c_str(), selfPtr ? selfPtr->getType()->str().c_str() : "");


	bool isPtr = false;
	bool isWrapped = false;

	fir::Type* type = self->getType();
	if(!type)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);


	if(cgi->isTypeAlias(type))
	{
		iceAssert(type->isStructType());
		iceAssert(type->toStructType()->getElementCount() == 1);
		type = type->toStructType()->getElementN(0);

		warn(this, "typealias encountered");
		isWrapped = true;
	}


	if(!type->isStructType())
	{
		if(type->isPointerType() && type->getPointerElementType()->isStructType())
		{
			type = type->getPointerElementType(), isPtr = true;
		}
		else
		{
			error(this, "Cannot do member access on non-struct type %s", cgi->getReadableType(type).c_str());
		}
	}


	// find out whether we need self or selfptr.
	if(selfPtr == nullptr && !isPtr)
	{
		// we don't have a pointer value for this
		// it's required for CreateStructGEP, so we'll have to make a temp variable
		// then store the result of the LHS into it.

		if(lhsPtr && lhsPtr->getType() == type->getPointerTo())
		{
			selfPtr = lhsPtr;
		}
		else
		{
			selfPtr = cgi->allocateInstanceInBlock(type);
			cgi->builder.CreateStore(self, selfPtr);
		}
	}


	// handle type aliases
	if(isWrapped)
	{
		bool wasSelfPtr = false;

		if(selfPtr)
		{
			selfPtr = cgi->lastMinuteUnwrapType(this, selfPtr);
			wasSelfPtr = true;
			isPtr = false;
		}
		else
		{
			self = cgi->lastMinuteUnwrapType(this, self);
		}


		// if we're faced with a double pointer, we need to load it once
		if(wasSelfPtr)
		{
			if(selfPtr->getType()->isPointerType() && selfPtr->getType()->getPointerElementType()->isPointerType())
				selfPtr = cgi->builder.CreateLoad(selfPtr);
		}
		else
		{
			if(self->getType()->isPointerType() && self->getType()->getPointerElementType()->isPointerType())
				self = cgi->builder.CreateLoad(self);
		}
	}






	fir::StructType* st = type->toStructType();

	TypePair_t* pair = cgi->getType(type);
	if(!pair && (!st || !st->isLiteralStruct()))
	{
		error("(%s:%d) -> Internal check failed: failed to retrieve type (%s)", __FILE__, __LINE__, cgi->getReadableType(type).c_str());
	}
	else if(st && st->isLiteralStruct())
	{
		type = st;
	}


	if((st && cgi->isTupleType(st)) || (pair->second.second == TypeKind::Tuple))
	{
		Number* n = dynamic_cast<Number*>(this->right);
		iceAssert(n);

		// if the lhs is immutable, don't give a pointer.
		// todo: fix immutability (actually across the entire compiler)
		bool immut = false;

		if(VarRef* vr = dynamic_cast<VarRef*>(this->left))
		{
			VarDecl* vd = cgi->getSymDecl(this, vr->name);
			iceAssert(vd);

			immut = vd->immutable;
		}

		return cgi->doTupleAccess(selfPtr, n, !immut);
	}
	else if(pair->second.second == TypeKind::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);

		iceAssert(str);
		iceAssert(self);


		// transform
		Expr* rhs = this->right;

		// get the index for the member
		// Expr* rhs = this->right;
		int i = -1;

		VarRef* var = dynamic_cast<VarRef*>(rhs);
		FuncCall* fc = dynamic_cast<FuncCall*>(rhs);


		if(var)
		{
			if(str->nameMap.find(var->name) != str->nameMap.end())
			{
				i = str->nameMap[var->name];
			}
			else
			{
				error(var, "Struct '%s' has no such member '%s'", str->name.c_str(), var->name.c_str());
			}
		}
		else if(!var && !fc)
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(this, "Type '%s' is not a tuple", str->name.c_str());
			}
			else
			{
				error(this, "(%s:%d) -> Internal check failed: no comprehendo (%s)", __FILE__, __LINE__, typeid(*rhs).name());
			}
		}

		if(var)
		{
			iceAssert(i >= 0);
			return doVariable(cgi, var, isPtr ? self : selfPtr, str, i);
		}
		else
		{
			error(rhs, "Unsupported operation on RHS of dot operator (%s)", typeid(*rhs).name());
		}
	}
	else if(pair->second.second == TypeKind::Class)
	{
		Class* cls = dynamic_cast<Class*>(pair->second.first);

		iceAssert(cls);
		iceAssert(self);

		// transform
		Expr* rhs = this->right;

		// get the index for the member
		// Expr* rhs = this->right;
		int i = -1;

		VarRef* var = dynamic_cast<VarRef*>(rhs);
		FuncCall* fc = dynamic_cast<FuncCall*>(rhs);


		if(var)
		{
			if(cls->nameMap.find(var->name) != cls->nameMap.end())
			{
				i = cls->nameMap[var->name];
			}
			else
			{
				iceAssert(cgi->getStructMemberByName(cls, var));
			}
		}
		else if(!var && !fc)
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(this, "Type '%s' is not a tuple", cls->name.c_str());
			}
			else
			{
				error(this, "(%s:%d) -> Internal check failed: no comprehendo (%s)", __FILE__, __LINE__, typeid(*rhs).name());
			}
		}

		if(fc)
		{
			size_t i = 0;
			std::deque<FuncPair_t> candidates;
			for(auto f : cls->funcs)
			{
				FuncPair_t fp = { cls->lfuncs[i], f->decl };
				if(f->decl->name == fc->name && f->decl->isStatic)
					candidates.push_back(fp);

				i++;
			}

			Resolved_t res = cgi->resolveFunctionFromList(fc, candidates, fc->name, fc->params);
			if(res.resolved) return doFunctionCall(cgi, fc, isPtr ? self : selfPtr, cls, true);


			return doFunctionCall(cgi, fc, isPtr ? self : selfPtr, cls, false);
		}
		else if(var)
		{
			if(i >= 0)
			{
				return doVariable(cgi, var, isPtr ? self : selfPtr, cls, i);
			}
			else
			{
				ComputedProperty* cprop = nullptr;
				for(ComputedProperty* c : cls->cprops)
				{
					if(c->name == var->name)
					{
						cprop = c;
						break;
					}
				}

				iceAssert(cprop);
				return doComputedProperty(cgi, var, cprop, _rhs, isPtr ? self : selfPtr, cls);
			}
		}
		else
		{
			iceAssert(!"Not var or function?!");
		}
	}
	else if(pair->second.second == TypeKind::Enum)
	{
		// return enumerationAccessCodegen(cgi, this->left, this->right);
		return cgi->getEnumerationCaseValue(this->left, this->right);
	}

	iceAssert(!"Encountered invalid expression");
}







static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, fir::Value* _rhs, fir::Value* ref, Class* str)
{
	if(_rhs)
	{
		if(!cprop->setter)
		{
			error(var, "Property '%s' of type has no setter and is readonly", cprop->name.c_str());
		}

		fir::Function* lcallee = 0;
		for(fir::Function* lf : str->lfuncs)
		{
			// printf("candidate: %s vs %s\n", cprop->setterFunc->mangledName.c_str(), lf->getName().str().c_str());
			if(lf->getName() == cprop->setterFunc->mangledName)
			{
				lcallee = lf;
				break;
			}
		}

		if(!lcallee)
			error(var, "?!??!!");


		std::vector<fir::Value*> args { ref, _rhs };

		// todo: rather large hack. since the nature of computed properties
		// is that they don't have a backing storage in the struct itself, we need
		// to return something. We're still used in a binOp though, so...

		// create a fake alloca to return to them.
		lcallee = cgi->module->getFunction(lcallee->getName());

		fir::Value* val = cgi->builder.CreateCall(lcallee, args);
		fir::Value* fake = cgi->allocateInstanceInBlock(_rhs->getType());

		cgi->builder.CreateStore(val, fake);

		return Result_t(val, fake);
	}
	else
	{
		fir::Function* lcallee = 0;
		for(fir::Function* lf : str->lfuncs)
		{
			if(lf->getName() == cprop->getterFunc->mangledName)
			{
				lcallee = lf;
				break;
			}
		}

		if(!lcallee)
			error(var, "?!??!!???");

		lcallee = cgi->module->getFunction(lcallee->getName());
		std::vector<fir::Value*> args { ref };
		return Result_t(cgi->builder.CreateCall(lcallee, args), 0);
	}
}

static Result_t doVariable(CodegenInstance* cgi, VarRef* var, fir::Value* ref, StructBase* str, int i)
{
	iceAssert(i >= 0);

	// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
	iceAssert(ref);

	fir::Value* ptr = cgi->builder.CreateStructGEP(ref, i);
	fir::Value* val = cgi->builder.CreateLoad(ptr);

	if(str->members[i]->immutable)
		ptr = 0;

	return Result_t(val, ptr);
}

static Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, fir::Value* ref, Class* str, bool isStaticFunctionCall)
{
	// make the args first.
	// since getting the type of a MemberAccess can't be done without codegening the Ast itself,
	// we codegen first, then use the codegen value to get the type.
	std::vector<fir::Value*> args { ref };

	for(Expr* e : fc->params)
		args.push_back(e->codegen(cgi).result.first);


	// now we need to determine if it exists, and its params.
	Func* callee = cgi->getFunctionFromMemberFuncCall(str, fc);
	iceAssert(callee);

	if(callee->decl->isStatic)
	{
		// remove the 'self' parameter
		args.erase(args.begin());
	}


	if(callee->decl->isStatic != isStaticFunctionCall)
	{
		error(fc, "Cannot call instance method '%s' without an instance", callee->decl->name.c_str());
	}




	fir::Function* lcallee = 0;
	for(fir::Function* lf : str->lfuncs)
	{
		if(lf->getName() == callee->decl->mangledName)
		{
			lcallee = lf;
			break;
		}
	}

	if(!lcallee)
		error(fc, "(%s:%d) -> Internal check failed: failed to find function %s", __FILE__, __LINE__, fc->name.c_str());

	lcallee = cgi->module->getFunction(lcallee->getName());
	iceAssert(lcallee);

	return Result_t(cgi->builder.CreateCall(lcallee, args), 0);
}



std::pair<fir::Type*, Result_t> CodegenInstance::resolveStaticDotOperator(MemberAccess* ma, bool actual)
{
	iceAssert(ma->matype == MAType::LeftNamespace || ma->matype == MAType::LeftTypename);

	// this makes the (valid and reasonable) assumption that all static access must happen before any non-static access.
	// ie. there is no way to invoke static dot operator semantics after an instance is encountered.

	// if we know the left side is some kind of static access,
	// we completely ignore it (since we can't get a value out of codegen), and basically
	// traverse it manually.

	// move leftwards. everything left of us *must* be static access.
	// this means varrefs only.

	// another (valid and reasonable) assumption is that once we encounter a typename (ie. static member or
	// nested type access), there will not be namespace access anymore.

	std::deque<std::string> list;

	Class* curType = 0;

	MemberAccess* cur = ma;
	while(MemberAccess* cleft = dynamic_cast<MemberAccess*>(cur->left))
	{
		cur = cleft;
		iceAssert(cur);

		VarRef* vr = dynamic_cast<VarRef*>(cur->right);
		iceAssert(vr);

		list.push_front(vr->name);
	}

	iceAssert(cur);
	{
		VarRef* vr = dynamic_cast<VarRef*>(cur->left);
		iceAssert(vr);

		list.push_front(vr->name);
	}

	std::deque<std::string> origList = list;


	std::deque<std::string> nsstrs;
	FunctionTree* ftree = this->getCurrentFuncTree(&nsstrs);
	while(list.size() > 0)
	{
		std::string front = list.front();
		list.pop_front();

		bool found = false;


		// printf("current: %s\n", front.c_str());
		if(curType == 0)
		{
			// check if it's a namespace.
			for(auto sub : ftree->subs)
			{
				iceAssert(sub);
				if(sub->nsName == front)
				{
					// yes.
					nsstrs.push_back(front);
					ftree = this->getCurrentFuncTree(&nsstrs);
					iceAssert(ftree);

					found = true;
					break;
				}
			}

			if(found)
			{
				// printf("found (1)\n");
				continue;
			}

			if(TypePair_t* tp = this->getType(this->mangleWithNamespace(front, nsstrs, false)))
			{
				// printf("got type %s (%zu)\n", front.c_str(), list.size());
				iceAssert(tp->second.first);
				curType = dynamic_cast<Class*>(tp->second.first);
				iceAssert(curType);

				found = true;
				continue;
			}
		}
		else
		{
			this->pushNestedTypeScope(curType);
			for(auto sb : curType->nestedTypes)
			{
				if(sb.first->name == front)
				{
					curType = sb.first;
					found = true;
					break;
				}
			}
			this->popNestedTypeScope();

			if(found) continue;
		}

		std::string lscope = ma->matype == MAType::LeftNamespace ? "namespace" : "type";
		error(ma, "No such member %s in %s %s", front.c_str(), lscope.c_str(),
			lscope == "namespace" ? ftree->nsName.c_str() : (curType ? curType->name.c_str() : "uhm..."));
	}










	// what is the right side?
	if(FuncCall* fc = dynamic_cast<FuncCall*>(ma->right))
	{
		Resolved_t res;
		if(curType == 0)
		{
			res = this->resolveFunctionFromList(ma, ftree->funcs, fc->name, fc->params);
			if(!res.resolved)
			{
				FunctionTree* pubft = this->getCurrentFuncTree(&nsstrs, this->rootNode->publicFuncTree);
				res = this->resolveFunctionFromList(ma, pubft->funcs, fc->name, fc->params);

				// printf("search ftree %d, nothing\n", ftree->id);
			}
		}
		else
		{
			iceAssert(curType->funcs.size() == curType->lfuncs.size());

			std::deque<FuncPair_t> flist;
			for(size_t i = 0; i < curType->funcs.size(); i++)
			{
				if(curType->funcs[i]->decl->name == fc->name)
					flist.push_back(FuncPair_t(curType->lfuncs[i], curType->funcs[i]->decl));
			}

			res = this->resolveFunctionFromList(ma, flist, fc->name, fc->params);
		}

		if(!res.resolved)
		{
			// note: we might have a type on the RHS (and namespaces/classes on the left)
			// check for this, and call the constructor, appropriately inserting the implicit self param.
			// try resolve it to a type.

			std::string text;
			for(auto s : origList)
				text += (s + "::");

			text += fc->name;

			if(fir::Type* ltype = this->getExprTypeFromStringType(ma, text))
			{
				TypePair_t* tp = this->getType(ltype);
				iceAssert(tp);

				std::vector<fir::Value*> args;
				for(Expr* e : fc->params)
					args.push_back(e->codegen(this).result.first);

				return { ltype, this->callTypeInitialiser(tp, ma, args) };
			}
			else
			{
				GenError::noFunctionTakingParams(this, fc, "namespace " + ftree->nsName, fc->name, fc->params);
			}
		}

		// call that sucker.
		// but first set the cached target.

		fir::Type* ltype = this->getExprType(fc, res);
		if(actual)
		{
			fc->cachedResolveTarget = res;
			Result_t res = fc->codegen(this);

			return { ltype, res };
		}
		else
		{
			return { ltype, Result_t(0, 0) };
		}
	}
	else if(VarRef* vr = dynamic_cast<VarRef*>(ma->right))
	{
		if(curType == 0)
		{
			fir::Value* ptr = 0;

			if(ftree->vars.find(vr->name) != ftree->vars.end())
			{
				SymbolPair_t sp = ftree->vars.at(vr->name);
				ptr = sp.first;
			}
			else
			{
				error(vr, "namespace %s does not contain a variable %s",
					ftree->nsName.c_str(), vr->name.c_str());
			}



			return
			{
				ptr->getType()->getPointerElementType(),
				actual ? Result_t(this->builder.CreateLoad(ptr), ptr) : Result_t(0, 0)
			};
		}
		else
		{
			// check static members
			// note: check for enum comes first since enum : class, so it's more specific.
			if(dynamic_cast<Enumeration*>(curType))
			{
				TypePair_t* tpair = this->getType(curType->mangledName);
				if(!tpair)
					error(vr, "Invalid class '%s'", vr->name.c_str());

				Result_t res = this->getEnumerationCaseValue(vr, tpair, vr->name, actual ? true : false);
				return { res.result.first->getType(), res };
			}
			else if(Class* cls = dynamic_cast<Class*>(curType))
			{
				for(auto v : cls->members)
				{
					if(v->isStatic && v->name == vr->name)
					{
						fir::Type* ltype = this->getExprType(v);
						return { ltype, actual ? this->getStaticVariable(vr, cls, v->name) : Result_t(0, 0) };
					}
				}
			}

			error(vr, "Class '%s' does not contain a static variable or class named '%s'", curType->name.c_str(), vr->name.c_str());
		}
	}
	else
	{
		error(ma, "Invalid expression type (%s) on right hand of dot operator", typeid(*ma->right).name());
	}
}






































Func* CodegenInstance::getFunctionFromMemberFuncCall(Class* str, FuncCall* fc)
{
	std::string full;
	std::deque<std::string> fs = this->getFullScope();
	for(auto f : fs)
		full += f + "::";

	full += str->name;

	std::deque<Expr*> params = fc->params;
	DummyExpr* fake = new DummyExpr(fc->pin);
	fake->type = full + "*";

	params.push_front(fake);

	std::deque<FuncPair_t> fns;
	for(auto f : str->funcs)
		fns.push_back({ 0, f->decl });

	Resolved_t res = this->resolveFunctionFromList(fc, fns, fc->name, params);

	if(!res.resolved)
	{
		error(fc, "Function '%s' is not a member of struct '%s'", fc->name.c_str(), str->name.c_str());
	}


	for(auto f : str->funcs)
	{
		if(f->decl == res.t.second)
			return f;
	}

	iceAssert(0);
}

Expr* CodegenInstance::getStructMemberByName(StructBase* str, VarRef* var)
{
	Expr* found = 0;

	if(Class* cls = dynamic_cast<Class*>(str))
	{
		for(auto c : cls->cprops)
		{
			if(c->name == var->name)
			{
				found = c;
				break;
			}
		}
	}

	if(!found)
	{
		for(auto m : str->members)
		{
			if(m->name == var->name)
			{
				found = m;
				break;
			}
		}
	}

	if(!found)
	{
		GenError::noSuchMember(this, var, str->name, var->name);
	}

	return found;
}

































