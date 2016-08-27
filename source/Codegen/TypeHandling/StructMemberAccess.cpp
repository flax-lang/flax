// DotOperatorCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


static Result_t doFunctionCall(CodegenInstance* cgi, MemberAccess* ma, FuncCall* fc, fir::Value* ref, ClassDef* cls, bool isStaticFunctionCall);
static Result_t doVariable(CodegenInstance* cgi, VarRef* var, fir::Value* ref, StructBase* str, int i);
static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cp, fir::Value* _rhs, fir::Value* ref);


Result_t ComputedProperty::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// handled elsewhere.
	return Result_t(0, 0);
}

Result_t CodegenInstance::getStaticVariable(Expr* user, ClassDef* cls, std::string name)
{
	auto tmp = cls->ident.scope;
	tmp.push_back(cls->ident.name);

	Identifier vid = Identifier(name, tmp, IdKind::Variable);

	if(fir::GlobalVariable* gv = this->module->getGlobalVariable(vid))
	{
		// todo: another kinda hacky thing.
		// this is present in some parts of the code, i don't know how many.
		// basically, if the thing is supposed to be immutable, we're not going to return
		// the ptr/ref value.

		return Result_t(this->builder.CreateLoad(gv), gv);
	}

	error(user, "Class '%s' has no such static member '%s'", cls->ident.name.c_str(), name.c_str());
}


Result_t MemberAccess::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->matype != MAType::LeftVariable && this->matype != MAType::LeftFunctionCall)
	{
		if(this->matype == MAType::Invalid) error(this, "??");
		return cgi->resolveStaticDotOperator(this, true).first.second;
	}



	// gen the var ref on the left.
	Result_t res = this->cachedCodegenResult;
	if(res.result.first == 0 && res.result.second == 0)
	{
		res = this->left->codegen(cgi);
	}
	else
	{
		error("wtf");
	}

	ValPtr_t p = res.result;

	fir::Value* self = p.first;
	fir::Value* selfPtr = p.second;


	bool isPtr = false;
	bool isWrapped = false;

	fir::Type* ftype = self->getType();
	if(!ftype)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);


	if(cgi->isTypeAlias(type))
	{
		iceAssert(ftype->isStructType());
		iceAssert(ftype->toStructType()->getElementCount() == 1);
		ftype = ftype->toStructType()->getElementN(0);

		warn(this, "typealias encountered");
		isWrapped = true;
	}


	if(!ftype->isStructType() && !ftype->isClassType() && !ftype->isTupleType())
	{
		if(ftype->isPointerType() && (ftype->getPointerElementType()->isStructType() || ftype->getPointerElementType()->isClassType()))
		{
			ftype = ftype->getPointerElementType(), isPtr = true;
		}
		else if(ftype->isLLVariableArrayType())
		{
			// lol, some magic.
			if(VarRef* vr = dynamic_cast<VarRef*>(this->right))
			{
				if(vr->name != "length")
					error(this, "Variadic array only has one member, 'length'. %s is invalid.", vr->name.c_str());

				// lol, now do the thing.
				return cgi->getLLVariableArrayLength(selfPtr);
			}
			else
			{
				error(this, "Variadic array only has one member, 'length'. Invalid operator.");
			}
		}
		else if(cgi->getExtensionsForBuiltinType(ftype).size() > 0)
		{
			// nothing was built to handle this
			// so we basically need to do it manually

			// todo.

			if(FuncCall* fc = dynamic_cast<FuncCall*>(this->right))
			{
				std::map<FuncDecl*, std::pair<Func*, fir::Function*>> fcands;
				std::deque<FuncPair_t> fpcands;

				for(auto ext : cgi->getExtensionsForBuiltinType(ftype))
				{
					for(auto f : ext->funcs)
					{
						fcands[f->decl] = { f, ext->functionMap[f] };
					}
				}


				for(auto it = fcands.begin(); it != fcands.end(); it++)
				{
					if((*it).first->ident.name != fc->name)
						it = fcands.erase(it);
				}

				for(auto p : fcands)
					fpcands.push_back({ p.second.second, p.second.first->decl });


				std::deque<fir::Type*> fpars = { ftype->getPointerTo() };
				for(auto e : fc->params) fpars.push_back(cgi->getExprType(e));


				Resolved_t res = cgi->resolveFunctionFromList(fc, fpcands, fc->name, fpars);
				if(!res.resolved)
					GenError::prettyNoSuchFunctionError(cgi, fc, fc->name, fc->params);

				iceAssert(res.t.first);


				std::deque<fir::Value*> args;
				for(auto e : fc->params)
					args.push_back(e->codegen(cgi).result.first);


				// make a new self
				iceAssert(self);

				fir::Value* newSelfP = cgi->builder.CreateImmutStackAlloc(self->getType(), self);
				args.push_front(newSelfP);

				// might not be a good thing to always do.
				// TODO: check this.
				// makes sure we call the function in our own module, because llvm only allows that.

				fir::Function* target = res.t.first;
				auto thistarget = cgi->module->getOrCreateFunction(target->getName(), target->getType(), target->linkageType);

				fir::Value* ret = cgi->builder.CreateCall(thistarget, args);
				return Result_t(ret, 0);
			}
			else if(VarRef* vr = dynamic_cast<VarRef*>(this->right))
			{
				std::deque<ComputedProperty*> ccands;

				for(auto ext : cgi->getExtensionsForBuiltinType(ftype))
				{
					for(auto c : ext->cprops)
					{
						if(c->ident.name == vr->name)
							ccands.push_back(c);
					}
				}

				if(ccands.size() > 1)
					error(vr, "Ambiguous access to property '%s' -- extensions declared duplicates?", vr->name.c_str());

				else if(ccands.size() == 0)
					error(vr, "Property '%s' for type '%s' not defined in any extensions", vr->name.c_str(), self->getType()->str().c_str());


				// do it
				ComputedProperty* prop = ccands[0];
				doComputedProperty(cgi, vr, prop, 0, cgi->builder.CreateImmutStackAlloc(self->getType(), self));
			}
			else
			{
				error(this->right, "enotsup");
			}
		}
		else
		{
			error(this, "Cannot do member access on non-struct type '%s'", cgi->getReadableType(ftype).c_str());
		}
	}


	// find out whether we need self or selfptr.
	if(selfPtr == nullptr && !isPtr)
	{
		selfPtr = cgi->getStackAlloc(ftype);
		cgi->builder.CreateStore(self, selfPtr);
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






	TypePair_t* pair = cgi->getType(ftype);

	if(!pair && !ftype->isClassType() && !ftype->isStructType() && !ftype->isTupleType())
	{
		error("(%s:%d) -> Internal check failed: failed to retrieve type (%s)", __FILE__, __LINE__, cgi->getReadableType(ftype).c_str());
	}


	if(ftype->isTupleType())
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
	else if(ftype->isStructType() && pair->second.second == TypeKind::Struct)
	{
		StructDef* str = dynamic_cast<StructDef*>(pair->second.first);
		fir::StructType* st = ftype->toStructType();

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
			if(st->hasElementWithName(var->name))
			{
				i = st->getElementIndex(var->name);
			}
			else
			{
				error(var, "Struct '%s' has no such member '%s'", str->ident.name.c_str(), var->name.c_str());
			}
		}
		else if(!var && !fc)
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(this, "Type '%s' is not a tuple", str->ident.name.c_str());
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
		else if(fc)
		{
			error(rhs, "calling func on struct type?");
		}
		else
		{
			error(rhs, "Unsupported operation on RHS of dot operator (%s)", typeid(*rhs).name());
		}
	}
	else if(ftype->isClassType() && pair->second.second == TypeKind::Class)
	{
		ClassDef* cls = dynamic_cast<ClassDef*>(pair->second.first);
		fir::ClassType* ct = ftype->toClassType();

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
			if(ct->hasElementWithName(var->name))
			{
				i = ct->getElementIndex(var->name);
			}
			else
			{
				if(!cgi->getStructMemberByName(cls, var))
					error(this, "Class '%s' has no such member %s", cls->ident.str().c_str(), var->name.c_str());
			}
		}
		else if(!var && !fc)
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(this, "Type '%s' is not a tuple", cls->ident.name.c_str());
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
				if(f->decl->ident.name == fc->name && f->decl->isStatic)
					candidates.push_back(fp);

				i++;
			}

			// Resolved_t res = cgi->resolveFunctionFromList(fc, candidates, fc->name, fc->params);
			// if(res.resolved)
			// {
			// 	error("what");
			// 	// // now we need to determine if it exists, and its params.
			// 	// auto pair = cgi->resolveMemberFuncCall(ma, cls, fc);
			// 	// Func* callee = pair.first;
			// 	// iceAssert(callee);

			// 	// if(callee->decl->isStatic)
			// 	// {
			// 	// 	// remove the 'self' parameter
			// 	// 	args.erase(args.begin());
			// 	// }


			// 	// if(callee->decl->isStatic != isStaticFunctionCall)
			// 	// {
			// 	// 	error(fc, "Cannot call instance method '%s' without an instance", callee->decl->ident.name.c_str());
			// 	// }

			// 	// fir::Function* lcallee = pair.second;
			// 	// iceAssert(lcallee);

			// 	// lcallee = cgi->module->getFunction(lcallee->getName());
			// 	// iceAssert(lcallee);

			// 	// return Result_t(cgi->builder.CreateCall(lcallee, args), 0);

			// 	// return doFunctionCall(cgi, this, fc, isPtr ? self : selfPtr, cls, true);
			// }
			// else
			// {
				return doFunctionCall(cgi, this, fc, isPtr ? self : selfPtr, cls, false);
			// }
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
					if(c->ident.name == var->name)
					{
						cprop = c;
						break;
					}
				}

				if(cprop == 0)
				{
					auto exts = cgi->getExtensionsForType(cls);
					for(auto ext : exts)
					{
						for(auto cp : ext->cprops)
						{
							if(cp->attribs & Attr_VisPublic || ext->parentRoot == cgi->rootNode)
							{
								if(cp->ident.name == var->name)
								{
									cprop = cp;
									break;
								}
							}
						}
					}
				}


				iceAssert(cprop);
				return doComputedProperty(cgi, var, cprop, 0, isPtr ? self : selfPtr);
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







static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, fir::Value* _rhs, fir::Value* ref)
{
	if(_rhs)
	{
		if(!cprop->setter)
		{
			error(var, "Property '%s' of type has no setter and is readonly", cprop->ident.name.c_str());
		}

		fir::Function* lcallee = cprop->setterFFn;
		iceAssert(lcallee);

		std::vector<fir::Value*> args { ref, _rhs };

		// todo: rather large hack. since the nature of computed properties
		// is that they don't have a backing storage in the struct itself, we need
		// to return something. We're still used in a binOp though, so...

		// create a fake alloca to return to them.
		lcallee = cgi->module->getFunction(lcallee->getName());

		fir::Value* val = cgi->builder.CreateCall(lcallee, args);
		fir::Value* fake = cgi->getStackAlloc(_rhs->getType());

		cgi->builder.CreateStore(val, fake);

		return Result_t(val, fake);
	}
	else
	{
		fir::Function* lcallee = cprop->getterFFn;
		iceAssert(lcallee);

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

static Result_t doFunctionCall(CodegenInstance* cgi, MemberAccess* ma, FuncCall* fc, fir::Value* ref, ClassDef* cls, bool isStaticFunctionCall)
{
	// make the args first.
	// since getting the type of a MemberAccess can't be done without codegening the Ast itself,
	// we codegen first, then use the codegen value to get the type.
	std::vector<fir::Value*> args { ref };

	for(Expr* e : fc->params)
		args.push_back(e->codegen(cgi).result.first);


	// now we need to determine if it exists, and its params.
	auto pair = cgi->resolveMemberFuncCall(ma, cls, fc);
	Func* callee = pair.first;
	iceAssert(callee);

	if(callee->decl->isStatic)
	{
		// remove the 'self' parameter
		args.erase(args.begin());
	}


	if(callee->decl->isStatic != isStaticFunctionCall)
	{
		error(fc, "Cannot call instance method '%s' without an instance", callee->decl->ident.name.c_str());
	}

	fir::Function* lcallee = pair.second;
	iceAssert(lcallee);

	lcallee = cgi->module->getFunction(lcallee->getName());
	iceAssert(lcallee);

	return Result_t(cgi->builder.CreateCall(lcallee, args), 0);
}



std::pair<std::pair<fir::Type*, Ast::Result_t>, fir::Type*> CodegenInstance::resolveStaticDotOperator(MemberAccess* ma, bool actual)
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

	fir::Type* curFType = 0;
	StructBase* curType = 0;

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
				continue;

			if(TypePair_t* tp = this->getType(Identifier(front, nsstrs, IdKind::Struct)))
			{
				iceAssert(tp->second.first);
				curType = dynamic_cast<ClassDef*>(tp->second.first);
				curFType = tp->first;
				iceAssert(curType);

				found = true;
				continue;
			}
			else
			{
				for(auto t : ftree->types)
				{
					if(t.first == front)
					{
						iceAssert(t.second.first);
						curType = dynamic_cast<ClassDef*>(t.second.second.first);
						curFType = t.second.first;

						iceAssert(curType);

						found = true;
						break;
					}
				}

				if(found) continue;
			}
		}
		else
		{
			this->pushNestedTypeScope(curType);
			for(auto sb : curType->nestedTypes)
			{
				if(sb.first->ident.name == front)
				{
					curType = sb.first;
					curFType = sb.second;
					found = true;
					break;
				}
			}
			this->popNestedTypeScope();

			if(found) continue;
		}

		std::string lscope = ma->matype == MAType::LeftNamespace ? "namespace" : "type";
		error(ma, "No such member %s in %s %s", front.c_str(), lscope.c_str(),
			lscope == "namespace" ? ftree->nsName.c_str() : (curType ? curType->ident.name.c_str() : "uhm..."));
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

				if(!res.resolved)
				{
					std::deque<Func*> flist;
					for(auto f : ftree->genericFunctions)
					{
						iceAssert(f.first->genericTypes.size() > 0);

						if(f.first->ident.name == fc->name)
							flist.push_back({ f.second });
					}

					FuncPair_t fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, flist);
					if(fp.first && fp.second)
						res = Resolved_t(fp);
				}
			}
		}
		else
		{
			if(ClassDef* clsd = dynamic_cast<ClassDef*>(curType))
			{
				iceAssert(clsd->funcs.size() == clsd->lfuncs.size());

				std::deque<FuncPair_t> flist;
				for(size_t i = 0; i < clsd->funcs.size(); i++)
				{
					if(clsd->funcs[i]->decl->ident.name == fc->name && clsd->funcs[i]->decl->isStatic)
						flist.push_back(FuncPair_t(clsd->lfuncs[i], clsd->funcs[i]->decl));
				}

				res = this->resolveFunctionFromList(ma, flist, fc->name, fc->params);

				if(!res.resolved)
				{
					std::deque<Func*> flist;
					for(auto f : clsd->funcs)
					{
						if(f->decl->ident.name == fc->name && f->decl->genericTypes.size() > 0)
							flist.push_back(f);
					}

					FuncPair_t fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, flist);
					if(fp.first && fp.second)
						res = Resolved_t(fp);
				}
			}
			else
			{
				error(fc, "error");
			}
		}




		if(!res.resolved)
		{
			// note: we might have a type on the RHS (and namespaces/classes on the left)
			// check for this, and call the constructor, appropriately inserting the implicit self param.
			// try resolve it to a type.

			std::string text;
			for(auto s : origList)
				text += (s + ".");

			text += fc->name;

			if(fir::Type* ltype = this->getExprTypeFromStringType(ma, text, true))
			{
				TypePair_t* tp = this->getType(ltype);
				iceAssert(tp);

				std::vector<fir::Value*> args;
				for(Expr* e : fc->params)
					args.push_back(e->codegen(this).result.first);

				return { { ltype, this->callTypeInitialiser(tp, ma, args) }, curFType };
			}
			else
			{
				GenError::noFunctionTakingParams(this, fc, "namespace " + ftree->nsName, fc->name, fc->params);
			}
		}

		// call that sucker.
		// but first set the cached target.

		fir::Type* ltype = res.t.first->getReturnType();
		if(actual)
		{
			fc->cachedResolveTarget = res;
			Result_t result = fc->codegen(this);

			return { { ltype, result }, curFType };
		}
		else
		{
			return { { ltype, Result_t(0, 0) }, curFType };
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
				{
					ptr->getType()->getPointerElementType(),
					actual ? Result_t(this->builder.CreateLoad(ptr), ptr) : Result_t(0, 0)
				},
				curFType
			};
		}
		else
		{
			// check static members
			// note: check for enum comes first since enum : class, so it's more specific.
			if(dynamic_cast<EnumDef*>(curType))
			{
				TypePair_t* tpair = this->getType(curType->ident);
				if(!tpair)
					error(vr, "Invalid class '%s'", vr->name.c_str());

				Result_t res = this->getEnumerationCaseValue(vr, tpair, vr->name, actual ? true : false);
				return { { res.result.first->getType(), res }, curFType };
			}
			else if(ClassDef* cls = dynamic_cast<ClassDef*>(curType))
			{
				for(auto v : cls->members)
				{
					if(v->isStatic && v->ident.name == vr->name)
					{
						fir::Type* ltype = this->getExprType(v);
						return { { ltype, actual ? this->getStaticVariable(vr, cls, v->ident.name) : Result_t(0, 0) }, curFType };
					}
				}
			}

			error(vr, "Class '%s' does not contain a static variable or class named '%s'", curType->ident.name.c_str(), vr->name.c_str());
		}
	}
	else
	{
		error(ma, "Invalid expression type (%s) on right hand of dot operator", typeid(*ma->right).name());
	}
}






































std::pair<Ast::Func*, fir::Function*> CodegenInstance::resolveMemberFuncCall(MemberAccess* ma, ClassDef* cls, FuncCall* fc)
{
	std::deque<fir::Type*> params;
	for(auto p : fc->params)
		params.push_back(this->getExprType(p));

	if(cls->createdType == 0)
		cls->createType(this);

	iceAssert(cls->createdType);
	params.push_front(cls->createdType->getPointerTo());


	std::deque<Func*> funclist;
	std::deque<Func*> genericfunclist;
	std::deque<FuncPair_t> fns;
	for(auto f : cls->funcs)
	{
		if(f->decl->ident.name == fc->name)
		{
			fns.push_back({ cls->functionMap[f], f->decl });
			funclist.push_back(f);

			if(f->decl->genericTypes.size() > 0)
				genericfunclist.push_back(f);
		}
	}


	std::deque<ExtensionDef*> exts = this->getExtensionsForType(cls);
	for(auto ext : exts)
	{
		for(auto f : ext->funcs)
		{
			if(f->decl->ident.name == fc->name && (f->decl->attribs & Attr_VisPublic || ext->parentRoot == this->rootNode))
			{
				fns.push_back({ ext->functionMap[f], f->decl });
				funclist.push_back(f);

				if(f->decl->genericTypes.size() > 0)
					genericfunclist.push_back(f);
			}

		}
	}

	Resolved_t res = this->resolveFunctionFromList(fc, fns, fc->name, params);

	if(!res.resolved)
	{
		// look for generic ones
		FuncPair_t fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, genericfunclist);
		if(fp.first && fp.second)
		{
			res = Resolved_t(fp);
		}
		else
		{
			auto tup = GenError::getPrettyNoSuchFunctionError(this, fc->params, fns);
			std::string argstr = std::get<0>(tup);
			std::string candstr = std::get<1>(tup);
			HighlightOptions ops = std::get<2>(tup);

			ops.caret = ma->pin;

			error(fc, ops, "No such member function '%s' in class %s taking parameters (%s)\nPossible candidates (%zu):\n%s",
				fc->name.c_str(), cls->ident.name.c_str(), argstr.c_str(), fns.size(), candstr.c_str());
		}
	}

	for(auto f : funclist)
	{
		if(f->decl == res.t.second)
			return { f, res.t.first };
	}

	iceAssert("failed to find func?" && 0);
}

Expr* CodegenInstance::getStructMemberByName(StructBase* str, VarRef* var)
{
	Expr* found = 0;

	if(ClassDef* cls = dynamic_cast<ClassDef*>(str))
	{
		for(auto c : cls->cprops)
		{
			if(c->ident.name == var->name)
			{
				found = c;
				break;
			}
		}
	}


	if(!found)
	{
		auto exts = this->getExtensionsForType(str);
		for(auto ext : exts)
		{
			for(auto cp : ext->cprops)
			{
				if(cp->attribs & Attr_VisPublic || ext->parentRoot == this->rootNode)
				{
					if(cp->ident.name == var->name)
					{
						found = cp;
						break;
					}
				}
			}
		}
	}



	if(!found)
	{
		for(auto m : str->members)
		{
			if(m->ident.name == var->name)
			{
				found = m;
				break;
			}
		}
	}

	if(!found)
	{
		GenError::noSuchMember(this, var, str->ident.name, var->name);
	}

	return found;
}

































