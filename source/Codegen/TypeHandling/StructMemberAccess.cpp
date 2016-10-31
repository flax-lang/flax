// DotOperatorCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



static Result_t doVariable(CodegenInstance* cgi, VarRef* var, fir::Value* ref, StructBase* str, int i);
static Result_t callComputedPropertyGetter(CodegenInstance* cgi, VarRef* var, ComputedProperty* cp, fir::Value* ref);
static Result_t getStaticVariable(CodegenInstance* cgi, Expr* user, ClassDef* cls, std::string name)
{
	auto tmp = cls->ident.scope;
	tmp.push_back(cls->ident.name);

	Identifier vid = Identifier(name, tmp, IdKind::Variable);

	if(fir::GlobalVariable* gv = cgi->module->getGlobalVariable(vid))
	{
		// todo: another kinda hacky thing.
		// this is present in some parts of the code, i don't know how many.
		// basically, if the thing is supposed to be immutable, we're not going to return
		// the ptr/ref value.

		return Result_t(cgi->irb.CreateLoad(gv), gv);
	}

	error(user, "Class '%s' has no such static member '%s'", cls->ident.name.c_str(), name.c_str());
}

static Result_t doTupleAccess(CodegenInstance* cgi, fir::Value* selfPtr, Number* num)
{
	iceAssert(selfPtr);
	iceAssert(num);

	fir::Type* type = selfPtr->getType()->getPointerElementType();
	iceAssert(type->isTupleType());

	// quite simple, just get the number (make sure it's a Ast::Number)
	// and do a structgep.

	if((size_t) num->ival >= type->toTupleType()->getElementCount())
		error(num, "Tuple does not have %d elements, only %zd", (int) num->ival + 1, type->toTupleType()->getElementCount());

	fir::Value* gep = cgi->irb.CreateStructGEP(selfPtr, num->ival);
	return Result_t(cgi->irb.CreateLoad(gep), gep, selfPtr->isImmutable() ? ValueKind::RValue : ValueKind::LValue);
}

// returns: Ast::Func, function, return type of function, return value of function
static std::tuple<Func*, fir::Function*, fir::Type*, fir::Value*> callMemberFunction(CodegenInstance* cgi, MemberAccess* ma,
	ClassDef* cls, FuncCall* fc, fir::Value* ref);




Result_t ComputedProperty::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// handled elsewhere.
	return Result_t(0, 0);
}

fir::Type* ComputedProperty::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return cgi->getTypeFromParserType(this, this->ptype);
}









static Result_t attemptDotOperatorOnBuiltinTypeOrFail(CodegenInstance* cgi, fir::Type* type, MemberAccess* ma, bool actual,
	fir::Value* val, fir::Value* ptr, fir::Type** resultType)
{
	if(type->isParameterPackType())
	{
		// lol, some magic.
		if(VarRef* vr = dynamic_cast<VarRef*>(ma->right))
		{
			if(vr->name != "length")
				error(ma, "Variadic array only has one member, 'length'. %s is invalid.", vr->name.c_str());

			if(!actual)
			{
				*resultType = fir::Type::getInt64();
				return Result_t(0, 0);
			}

			iceAssert(ptr);
			return Result_t(cgi->irb.CreateGetParameterPackLength(ptr), 0);
		}
		else
		{
			error(ma, "Variadic array only has one member, 'length'. Invalid operator.");
		}
	}

	if(type->isStringType() && dynamic_cast<VarRef*>(ma->right))
	{
		// handle builtin ones: 'raw' and 'length'
		// raw is basically just the string
		// length is basically just the length.

		// lol

		auto vr = dynamic_cast<VarRef*>(ma->right);
		iceAssert(vr);

		if(vr->name == "raw")
		{
			if(!actual)
			{
				*resultType = fir::Type::getInt8Ptr();
				return Result_t(0, 0);
			}
			else
			{
				iceAssert(ptr);
				return Result_t(cgi->irb.CreateGetStringData(ptr), 0);
			}
		}
		else if(vr->name == "length")
		{
			if(!actual)
			{
				*resultType = fir::Type::getInt64();
				return Result_t(0, 0);
			}
			else
			{
				iceAssert(ptr);
				return Result_t(cgi->irb.CreateGetStringLength(ptr), 0);
			}
		}
	}

	if(cgi->getExtensionsForBuiltinType(type).size() > 0)
	{
		// nothing was built to handle this
		if(FuncCall* fc = dynamic_cast<FuncCall*>(ma->right))
		{
			std::map<FuncDecl*, std::pair<Func*, fir::Function*>> fcands;
			std::deque<FuncDefPair> fpcands;

			for(auto ext : cgi->getExtensionsForBuiltinType(type))
			{
				for(auto f : ext->funcs)
				{
					if(f->decl->ident.name == fc->name)
						fcands[f->decl] = { f, ext->functionMap[f] };
				}
			}

			for(auto p : fcands)
				fpcands.push_back(FuncDefPair(p.second.second, p.second.first->decl, p.second.first));

			std::deque<fir::Type*> fpars = { type->getPointerTo() };
			for(auto e : fc->params) fpars.push_back(e->getType(cgi));

			Resolved_t res = cgi->resolveFunctionFromList(fc, fpcands, fc->name, fpars);
			if(!res.resolved)
				GenError::prettyNoSuchFunctionError(cgi, fc, fc->name, fc->params);

			iceAssert(res.t.firFunc);

			if(!actual)
			{
				*resultType = res.t.firFunc->getReturnType();
				return Result_t(0, 0);
			}

			std::deque<fir::Value*> args;
			for(auto e : fc->params)
				args.push_back(e->codegen(cgi).value);

			// make a new self (that is immutable)
			iceAssert(val);

			fir::Value* newSelfP = cgi->irb.CreateImmutStackAlloc(val->getType(), val);
			args.push_front(newSelfP);

			fir::Function* target = res.t.firFunc;
			auto thistarget = cgi->module->getOrCreateFunction(target->getName(), target->getType(), target->linkageType);

			fir::Value* ret = cgi->irb.CreateCall(thistarget, args);
			return Result_t(ret, 0);
		}
		else if(VarRef* vr = dynamic_cast<VarRef*>(ma->right))
		{
			std::deque<ComputedProperty*> ccands;

			for(auto ext : cgi->getExtensionsForBuiltinType(type))
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
				error(vr, "Property '%s' for type '%s' not defined in any extensions", vr->name.c_str(), type->str().c_str());

			// do it
			if(!actual)
			{
				*resultType = ccands[0]->getterFFn->getReturnType();
				return Result_t(0, 0);
			}

			ComputedProperty* prop = ccands[0];
			return callComputedPropertyGetter(cgi, vr, prop, cgi->irb.CreateImmutStackAlloc(val->getType(), val));
		}
		else
		{
			error(ma->right, "Unsupported RHS for dot-operator on builtin type '%s'", type->str().c_str());
		}
	}
	else
	{
		error(ma, "Cannot do member access on non-struct type '%s'", type->str().c_str());
	}
}







fir::Type* MemberAccess::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->matype == MAType::LeftNamespace || this->matype == MAType::LeftTypename)
		return cgi->resolveStaticDotOperator(this, false).first.first;

	// first, get the type of the lhs
	fir::Type* lhs = this->left->getType(cgi);
	TypePair_t* pair = cgi->getType(lhs->isPointerType() ? lhs->getPointerElementType() : lhs);




	if(lhs->isTupleType())
	{
		// values are 1, 2, 3 etc.
		// for now, assert this.

		fir::TupleType* tt = lhs->toTupleType();
		iceAssert(tt);

		Number* n = dynamic_cast<Number*>(this->right);
		iceAssert(n);

		if((size_t) n->ival >= tt->getElementCount())
		{
			error(this, "Tuple does not have %d elements, only %zd", (int) n->ival + 1, tt->getElementCount());
		}

		return tt->getElementN(n->ival);
	}
	else if(!pair && (!lhs->isStructType() && !lhs->isClassType() && !lhs->isTupleType()))
	{
		fir::Type* ret = 0;
		attemptDotOperatorOnBuiltinTypeOrFail(cgi, lhs, this, false, 0, 0, &ret);

		return ret;
	}
	else if(pair->second.second == TypeKind::Class)
	{
		ClassDef* cls = dynamic_cast<ClassDef*>(pair->second.first);
		iceAssert(cls);

		VarRef* memberVr = dynamic_cast<VarRef*>(this->right);
		FuncCall* memberFc = dynamic_cast<FuncCall*>(this->right);

		if(memberVr)
		{
			for(VarDecl* mem : cls->members)
			{
				if(mem->ident.name == memberVr->name)
					return mem->getType(cgi);
			}
			for(ComputedProperty* c : cls->cprops)
			{
				if(c->ident.name == memberVr->name)
					return c->getType(cgi);
			}

			auto exts = cgi->getExtensionsForType(cls);
			for(auto ext : exts)
			{
				for(auto cp : ext->cprops)
				{
					if(cp->attribs & Attr_VisPublic || ext->parentRoot == cgi->rootNode)
					{
						if(cp->ident.name == memberVr->name)
							return cp->getType(cgi);
					}
				}
			}

			auto ret = cgi->tryGetMemberFunctionOfClass(cls, memberVr, memberVr->name, extra);
			if(ret.isEmpty()) error(memberVr, "Class '%s' has no member named '%s'", cls->ident.name.c_str(), memberVr->name.c_str());
			return ret.firFunc->getType();
		}
		else if(memberFc)
		{
			return std::get<2>(callMemberFunction(cgi, this, cls, memberFc, 0));
		}
	}
	else if(pair->second.second == TypeKind::Struct)
	{
		StructDef* str = dynamic_cast<StructDef*>(pair->second.first);
		iceAssert(str);

		VarRef* memberVr = dynamic_cast<VarRef*>(this->right);
		FuncCall* memberFc = dynamic_cast<FuncCall*>(this->right);

		if(memberVr)
		{
			for(VarDecl* mem : str->members)
			{
				if(mem->ident.name == memberVr->name)
					return mem->getType(cgi);
			}

			auto exts = cgi->getExtensionsForType(str);
			for(auto ext : exts)
			{
				for(auto cp : ext->cprops)
				{
					if(cp->attribs & Attr_VisPublic || ext->parentRoot == cgi->rootNode)
					{
						if(cp->ident.name == memberVr->name)
							return cp->getType(cgi);
					}
				}
			}

			error(memberVr, "Struct '%s' has no member '%s'", str->ident.name.c_str(), memberVr->name.c_str());
		}
		else if(memberFc)
		{
			error(memberFc, "Tried to call method on struct");
		}
	}
	else
	{
		error(this->left, "Invalid expression type for dot-operator access");
	}

	iceAssert(0);
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
	if(res.value == 0 && res.pointer == 0)
	{
		res = this->left->codegen(cgi);
	}
	else
	{
		error("wtf");
	}

	fir::Value* self = res.value;
	fir::Value* selfPtr = res.pointer;


	bool isPtr = false;
	bool isWrapped = false;

	fir::Type* ftype = self->getType();
	if(!ftype)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);




	if(!ftype->isStructType() && !ftype->isClassType() && !ftype->isTupleType())
	{
		if(ftype->isPointerType() && (ftype->getPointerElementType()->isStructType() || ftype->getPointerElementType()->isClassType()))
		{
			ftype = ftype->getPointerElementType(), isPtr = true;
		}
		else
		{
			fir::Type* _ = 0;
			return attemptDotOperatorOnBuiltinTypeOrFail(cgi, ftype, this, true, self, selfPtr, &_);
		}
	}


	// find out whether we need self or selfptr.
	if(selfPtr == nullptr && !isPtr)
	{
		selfPtr = cgi->getStackAlloc(ftype);
		cgi->irb.CreateStore(self, selfPtr);
	}


	// handle type aliases
	if(isWrapped)
	{
		bool wasSelfPtr = false;

		if(selfPtr)
		{
			wasSelfPtr = true;
			isPtr = false;
		}


		// if we're faced with a double pointer, we need to load it once
		if(wasSelfPtr)
		{
			if(selfPtr->getType()->isPointerType() && selfPtr->getType()->getPointerElementType()->isPointerType())
				selfPtr = cgi->irb.CreateLoad(selfPtr);
		}
		else
		{
			if(self->getType()->isPointerType() && self->getType()->getPointerElementType()->isPointerType())
				self = cgi->irb.CreateLoad(self);
		}
	}






	TypePair_t* pair = cgi->getType(ftype);

	if(!pair && !ftype->isClassType() && !ftype->isStructType() && !ftype->isTupleType())
	{
		error("(%s:%d) -> Internal check failed: failed to retrieve type (%s)", __FILE__, __LINE__, ftype->str().c_str());
	}


	if(ftype->isTupleType())
	{
		Number* n = dynamic_cast<Number*>(this->right);
		iceAssert(n);

		// if the lhs is immutable, don't give a pointer.
		// todo: fix immutability (actually across the entire compiler)
		return doTupleAccess(cgi, selfPtr, n);
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

				iceAssert(i >= 0);
				return doVariable(cgi, var, isPtr ? self : selfPtr, str, i);
			}
			else
			{
				error(var, "Struct '%s' has no such member '%s'", str->ident.name.c_str(), var->name.c_str());
			}
		}
		else if(fc)
		{
			error(rhs, "Cannot call non-existent method '%s' on struct '%s'", fc->name.c_str(), str->ident.name.c_str());
		}
		else
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(this, "Type '%s' is not a tuple", str->ident.name.c_str());
			}
			else
			{
				error(rhs, "Unsupported operation on RHS of dot operator (%s)", typeid(*rhs).name());
			}
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

				iceAssert(i >= 0);
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

				if(!cprop)
				{
					auto ret = cgi->tryGetMemberFunctionOfClass(cls, var, var->name, extra);
					if(ret.isEmpty()) error(var, "Class '%s' has no member named '%s'", cls->ident.name.c_str(), var->name.c_str());

					return Result_t(ret.firFunc, 0);
				}
				else
				{
					iceAssert(cprop);
					return callComputedPropertyGetter(cgi, var, cprop, isPtr ? self : selfPtr);
				}
			}
		}
		else if(fc)
		{
			size_t i = 0;
			std::deque<FuncDefPair> candidates;

			for(auto f : cls->funcs)
			{
				FuncDefPair fp(cls->lfuncs[i], f->decl, f);
				if(f->decl->ident.name == fc->name && f->decl->isStatic)
					candidates.push_back(fp);

				i++;
			}

			// return doFunctionCall(cgi, this, fc, isPtr ? self : selfPtr, cls, false);
			auto result = callMemberFunction(cgi, this, cls, fc, isPtr ? self : selfPtr);
			return Result_t(std::get<3>(result), 0);
		}
		else
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(this, "Type '%s' is not a tuple", cls->ident.name.c_str());
			}
			else
			{
				error(rhs, "Unsupported operation on RHS of dot operator (%s)", typeid(*rhs).name());
			}
		}
	}
	else if(pair->second.second == TypeKind::Enum)
	{
		iceAssert(0 && "what? no.");
		// return cgi->getEnumerationCaseValue(this->left, this->right);
	}

	iceAssert(!"Encountered invalid expression");
}







static Result_t callComputedPropertyGetter(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, fir::Value* ref)
{
	fir::Function* lcallee = cprop->getterFFn;
	iceAssert(lcallee);

	lcallee = cgi->module->getFunction(lcallee->getName());
	return Result_t(cgi->irb.CreateCall1(lcallee, ref), 0);
}

static Result_t doVariable(CodegenInstance* cgi, VarRef* var, fir::Value* ref, StructBase* str, int i)
{
	iceAssert(i >= 0);

	// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
	iceAssert(ref);

	fir::Value* ptr = cgi->irb.CreateStructGEP(ref, i);
	fir::Value* val = cgi->irb.CreateLoad(ptr);

	if(str->members[i]->immutable)
		ptr = 0;

	return Result_t(val, ptr, ValueKind::LValue);
}










std::tuple<FunctionTree*, std::deque<std::string>, std::deque<std::string>, Ast::StructBase*, fir::Type*>
CodegenInstance::unwrapStaticDotOperator(Ast::MemberAccess* ma)
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


	// now we go left-to-right.
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
			if(ftree->subMap.find(front) != ftree->subMap.end())
			{
				// yes.
				nsstrs.push_back(front);
				ftree = this->getCurrentFuncTree(&nsstrs);
				iceAssert(ftree);

				found = true;
			}

			// for(auto sub : ftree->subs)
			// {
			// 	iceAssert(sub);
			// 	if(sub->nsName == front)
			// 	{
			// 		// yes.
			// 		nsstrs.push_back(front);
			// 		ftree = this->getCurrentFuncTree(&nsstrs);
			// 		iceAssert(ftree);

			// 		found = true;
			// 		break;
			// 	}
			// }

			if(found) continue;

			if(TypePair_t* tp = this->getType(Identifier(front, nsstrs, IdKind::Struct)))
			{
				iceAssert(tp->second.first);
				curType = dynamic_cast<StructBase*>(tp->second.first);
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
						curType = dynamic_cast<StructBase*>(t.second.second.first);
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

	return { ftree, nsstrs, origList, curType, curFType };
}









std::pair<std::pair<fir::Type*, Ast::Result_t>, fir::Type*> CodegenInstance::resolveStaticDotOperator(MemberAccess* ma, bool actual)
{
	iceAssert(ma->matype == MAType::LeftNamespace || ma->matype == MAType::LeftTypename);

	FunctionTree* ftree = 0;
	StructBase* curType = 0;
	fir::Type* curFType = 0;
	std::deque<std::string> nsstrs;
	std::deque<std::string> origList;

	std::tie(ftree, nsstrs, origList, curType, curFType) = this->unwrapStaticDotOperator(ma);




	// what is the right side?
	if(FuncCall* fc = dynamic_cast<FuncCall*>(ma->right))
	{
		Resolved_t res;
		if(curType == 0)
		{
			res = this->resolveFunctionFromList(ma, ftree->funcs, fc->name, fc->params);
			if(!res.resolved)
			{
				std::deque<Func*> flist;
				for(auto f : ftree->genericFunctions)
				{
					iceAssert(f.first->genericTypes.size() > 0);

					if(f.first->ident.name == fc->name)
						flist.push_back({ f.second });
				}

				FuncDefPair fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, flist);
				if(!fp.isEmpty()) res = Resolved_t(fp);
			}
		}
		else
		{
			if(ClassDef* clsd = dynamic_cast<ClassDef*>(curType))
			{
				iceAssert(clsd->funcs.size() == clsd->lfuncs.size());

				std::deque<FuncDefPair> flist;
				for(size_t i = 0; i < clsd->funcs.size(); i++)
				{
					if(clsd->funcs[i]->decl->ident.name == fc->name && clsd->funcs[i]->decl->isStatic)
						flist.push_back(FuncDefPair(clsd->lfuncs[i], clsd->funcs[i]->decl, clsd->funcs[i]));
				}

				for(auto e : this->getExtensionsForType(clsd))
				{
					for(size_t i = 0; i < clsd->funcs.size(); i++)
					{
						if(e->funcs[i]->decl->ident.name == fc->name && e->funcs[i]->decl->isStatic)
							flist.push_back(FuncDefPair(e->lfuncs[i], e->funcs[i]->decl, e->funcs[i]));
					}
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

					FuncDefPair fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, flist);
					if(!fp.isEmpty()) res = Resolved_t(fp);
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

			if(fir::Type* ltype = this->getTypeFromParserType(ma, pts::NamedType::create(text), true))
			{
				TypePair_t* tp = this->getType(ltype);
				iceAssert(tp);

				std::vector<fir::Value*> args;
				for(Expr* e : fc->params)
					args.push_back(e->codegen(this).value);

				return { { ltype, this->callTypeInitialiser(tp, ma, args) }, curFType };
			}
			else
			{
				GenError::noFunctionTakingParams(this, fc, "namespace " + ftree->nsName, fc->name, fc->params);
			}
		}

		// call that sucker.
		// but first set the cached target.

		fir::Type* ltype = res.t.firFunc->getReturnType();
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

				return
				{
					{
						ptr->getType()->getPointerElementType(),
						actual ? Result_t(this->irb.CreateLoad(ptr), ptr, ValueKind::LValue) : Result_t(0, 0)
					},
					curFType
				};
			}
			else
			{
				for(auto f : ftree->funcs)
				{
					if(f.funcDecl->ident.name == vr->name && f.funcDecl->genericTypes.size() == 0)
						return { { f.firFunc->getType(), Result_t(f.firFunc, 0) }, 0 };
				}


				for(auto gf : ftree->genericFunctions)
				{
					if(gf.first->ident.name == vr->name)
					{
						if(!gf.first->generatedFunc)
							gf.first->codegen(this);

						fir::Function* fn = gf.first->generatedFunc;
						iceAssert(fn);

						return { { fn->getType(), Result_t(fn, 0) }, 0 };
					}
				}
			}

			error(vr, "Namespace '%s' does not contain a variable '%s'",
				ftree->nsName.c_str(), vr->name.c_str());
		}
		else if(EnumDef* enr = dynamic_cast<EnumDef*>(curType))
		{
			iceAssert(enr->createdType);
			fir::EnumType* et = enr->createdType->toEnumType();
			iceAssert(et);

			fir::ConstantValue* cv = et->getCaseWithName(vr->name);
			if(!cv) error(vr, "Enum '%s' has no case named '%s'", enr->ident.str().c_str(), vr->name.c_str());

			Result_t res(0, 0);

			if(actual) res = Result_t(this->irb.CreateBitcast(cv, et), 0);

			return { { et, res }, curFType };
		}
		else if(ClassDef* cls = dynamic_cast<ClassDef*>(curType))
		{
			for(auto v : cls->members)
			{
				if(v->isStatic && v->ident.name == vr->name)
				{
					fir::Type* ltype = v->getType(this);
					auto r = actual ? getStaticVariable(this, vr, cls, v->ident.name) : Result_t(0, 0);

					return { { ltype, Result_t(r.value, r.pointer, ValueKind::LValue) }, curFType };
				}
			}

			for(auto f : cls->funcs)
			{
				if(f->decl->ident.name == vr->name)
				{
					fir::Type* ltype = cls->functionMap[f]->getType();
					return { { ltype, Result_t(cls->functionMap[f], 0) }, curFType };
				}
			}
		}

		error(vr, "Class '%s' does not contain a static variable or class named '%s'", curType->ident.name.c_str(), vr->name.c_str());
	}
	else
	{
		error(ma, "Invalid expression type (%s) on right hand of dot operator", typeid(*ma->right).name());
	}
}












fir::Function* CodegenInstance::tryDisambiguateFunctionVariableUsingType(Expr* usr, std::string name,
	std::deque<fir::Function*> cands, fir::Value* extra)
{
	if(cands.size() == 0)
	{
		return 0;
	}
	else if(cands.size() > 1 && (extra == 0 || (!extra->getType()->isPointerType()
					|| extra->getType()->getPointerTo()->isFunctionType())))
	{
		error(usr, "Ambiguous reference to function with name '%s' (multiple overloads)", name.c_str());
	}
	else if(cands.size() > 1)
	{
		fir::FunctionType* ft = extra->getType()->toPointerType()->toFunctionType();
		iceAssert(ft);

		for(auto c : cands)
		{
			if(c->getType() == ft)
				return c;
		}

		// candidates
		std::string cstr;
		for(auto c : cands)
		{
			auto s = c->getType()->str();
			cstr += "func " + c->getName().str() + s.substr(1, s.length() - 2) + "\n";
		}

		error(usr, "No matching function with name '%s' with type '%s', have %zu candidates:\n%s",
			name.c_str(), ft->str().c_str(), cands.size(), cstr.c_str());
	}
	else
	{
		// normal.
		iceAssert(cands.size() == 1);
		return cands.front();
	}
}

FuncDefPair CodegenInstance::tryGetMemberFunctionOfClass(ClassDef* cls, Expr* user, std::string name, fir::Value* extra)
{
	// find functions
	std::deque<fir::Function*> cands;
	std::map<fir::Function*, std::pair<FuncDecl*, Func*>> map;

	for(auto f : cls->funcs)
	{
		if(f->decl->ident.name == name)
			cands.push_back(cls->functionMap[f]), map[cls->functionMap[f]] = { f->decl, f };
	}

	for(auto ext : this->getExtensionsForType(cls))
	{
		for(auto f : ext->funcs)
		{
			if(f->decl->ident.name == name)
				cands.push_back(ext->functionMap[f]), map[ext->functionMap[f]] = { f->decl, f };
		}
	}

	fir::Function* ret = this->tryDisambiguateFunctionVariableUsingType(user, name, cands, extra);
	if(ret == 0) return FuncDefPair::empty();

	auto p = map[ret];
	return FuncDefPair(ret, p.first, p.second);
}








fir::Function* CodegenInstance::resolveAndInstantiateGenericFunctionReference(Expr* user, fir::Function* oldf,
	fir::FunctionType* instantiatedFT, MemberAccess* ma)
{
	iceAssert(!instantiatedFT->isGenericFunction() && "Cannot instantiate generic function with another generic function");

	std::string name = oldf->getName().name;

	if(ma)
	{
		if(ma->matype == MAType::LeftNamespace || ma->matype == MAType::LeftTypename)
		{
			// do the thing

			FunctionTree* ftree = 0;
			StructBase* strType = 0;
			fir::Type* strFType = 0;

			std::tie(ftree, std::ignore, std::ignore, strType, strFType) = this->unwrapStaticDotOperator(ma);


			std::map<fir::Function*, Func*> map;

			if(strType != 0)
			{
				// note(?): this procedure is only called when we need to instantiate a generic method/static generic method of a type (or in
				// a namespace) with a concrete type
				// so, we don't need to look at members or anything else, just functions.
				//
				// eg.
				//
				// let foo: [(SomeClass*, int) -> int] = SomeClass.someMethod
				//
				// ... (somewhere else)
				//
				// class SomeClass
				// {
				//     func someMethod<T>(a: T) -> T { ... }
				// }
				//
				// we can't (and probably won't) have generic function types
				// (eg. something like let foo: [<T, K>(a: T, b: T) -> K] or something)
				// since there's no easy way to be type-safe about them.


				// static function
				ClassDef* cd = dynamic_cast<ClassDef*>(strType);
				iceAssert(cd);

				for(auto f : cd->funcs)
				{
					if(f->decl->ident.name == name && f->decl->genericTypes.size() > 0)
						map[cd->functionMap[f]] = f;
				}


				for(auto ext : this->getExtensionsForType(cd))
				{
					for(auto f : ext->funcs)
					{
						if(f->decl->ident.name == name && f->decl->genericTypes.size() > 0)
							map[ext->functionMap[f]] = f;
					}
				}
			}
			else
			{
				iceAssert(ftree);

				for(auto f : ftree->genericFunctions)
				{
					if(!f.first->generatedFunc)
						f.first->codegen(this);

					iceAssert(f.first->generatedFunc);
					map[f.first->generatedFunc] = f.second;
				}
			}

			// failed to find
			if(map.empty()) return 0;


			std::deque<fir::Function*> cands;

			// set up
			for(auto p : map)
				cands.push_back(p.first);

			auto res = this->tryDisambiguateFunctionVariableUsingType(user, name, cands, fir::ConstantValue::getNullValue(instantiatedFT));
			if(res == 0) return 0;

			// ok.
			Func* fnbody = map[res];
			iceAssert(fnbody);
			{
				// instantiate it.

				FuncDefPair fp = this->tryResolveGenericFunctionFromCandidatesUsingFunctionType(user, { fnbody }, instantiatedFT);

				iceAssert(fp.firFunc);
				return fp.firFunc;
			}
		}
		else
		{
			error(user, "not supported??");
		}
	}
	else
	{
		return this->tryResolveGenericFunctionFromCandidatesUsingFunctionType(user,
			this->findGenericFunctions(oldf->getName().name), instantiatedFT).firFunc;
	}
}




































std::tuple<Func*, fir::Function*, fir::Type*, fir::Value*> callMemberFunction(CodegenInstance* cgi, MemberAccess* ma,
	ClassDef* cls, FuncCall* fc, fir::Value* ref)
{
	std::deque<fir::Type*> params;
	for(auto p : fc->params)
		params.push_back(p->getType(cgi));

	if(cls->createdType == 0)
		cls->createType(cgi);

	iceAssert(cls->createdType);
	params.push_front(cls->createdType->getPointerTo());


	std::deque<Func*> funclist;

	std::deque<Func*> genericfunclist;
	std::deque<FuncDefPair> fns;
	for(auto f : cls->funcs)
	{
		if(f->decl->ident.name == fc->name)
		{
			fns.push_back(FuncDefPair(cls->functionMap[f], f->decl, f));
			funclist.push_back(f);

			if(f->decl->genericTypes.size() > 0)
				genericfunclist.push_back(f);
		}
	}



	std::deque<ExtensionDef*> exts = cgi->getExtensionsForType(cls);
	for(auto ext : exts)
	{
		for(auto f : ext->funcs)
		{
			if(f->decl->ident.name == fc->name && (f->decl->attribs & Attr_VisPublic || ext->parentRoot == cgi->rootNode))
			{
				fns.push_back(FuncDefPair(ext->functionMap[f], f->decl, f));
				funclist.push_back(f);

				if(f->decl->genericTypes.size() > 0)
					genericfunclist.push_back(f);
			}

		}
	}

	Resolved_t res = cgi->resolveFunctionFromList(fc, fns, fc->name, params);

	if(!res.resolved)
	{
		// look for generic ones
		FuncDefPair fp = cgi->tryResolveGenericFunctionCallUsingCandidates(fc, genericfunclist);
		if(!fp.isEmpty())
		{
			res = Resolved_t(fp);
		}
		else
		{
			// try members
			{
				fir::Value* theFunction = 0;
				for(auto m : cls->members)
				{
					if(m->ident.name == fc->name && m->concretisedType && m->concretisedType->isFunctionType())
					{
						if(m->concretisedType->toFunctionType()->isGenericFunction())
							error("not sup (1)");

						if(ref == 0)
						{
							// wtf??
							return std::make_tuple((Func*) 0, (fir::Function*) 0,
								m->concretisedType->toFunctionType()->getReturnType(), (fir::Value*) 0);
						}
						else
						{
							// make the function.
							auto vr = new VarRef(fc->pin, fc->name);
							auto res = doVariable(cgi, vr, ref, cls, cls->createdType->toClassType()->getElementIndex(m->ident.name));

							// delete vr;

							iceAssert(res.value);
							iceAssert(res.value->getType()->isFunctionType());

							theFunction = res.value;
							break;
						}
					}
				}

				if(theFunction == 0)
				{
					// check properties
					for(auto p : cls->cprops)
					{
						if(p->ident.name == fc->name && p->concretisedType && p->concretisedType->isFunctionType())
						{
							if(p->concretisedType->toFunctionType()->isGenericFunction())
								error("not sup (2)");

							if(ref == 0)
							{
								return std::make_tuple((Func*) 0, (fir::Function*) 0,
									p->concretisedType->toFunctionType()->getReturnType(), (fir::Value*) 0);
							}
							else
							{
								auto vr = new VarRef(fc->pin, fc->name);
								auto res = callComputedPropertyGetter(cgi, vr, p, ref);

								// delete vr;

								iceAssert(res.value);
								iceAssert(res.value->getType()->isFunctionType());

								theFunction = res.value;
								break;
							}
						}
					}

					// check extensions
					if(theFunction == 0)
					{
						for(auto ext : cgi->getExtensionsForType(cls))
						{
							bool stop = false;
							for(auto p : ext->cprops)
							{
								if(p->concretisedType->toFunctionType()->isGenericFunction())
									error("not sup (2)");

								if(p->ident.name == fc->name && p->concretisedType && p->concretisedType->isFunctionType())
								{
									if(ref == 0)
									{
										return std::make_tuple((Func*) 0, (fir::Function*) 0,
											p->concretisedType->toFunctionType()->getReturnType(), (fir::Value*) 0);
									}
									else
									{
										auto vr = new VarRef(fc->pin, fc->name);
										auto res = callComputedPropertyGetter(cgi, vr, p, ref);

										// delete vr;

										iceAssert(res.value);
										iceAssert(res.value->getType()->isFunctionType());

										theFunction = res.value;
										stop = true;
										break;
									}
								}
							}

							if(stop) break;
						}
					}
				}


				if(theFunction && ref)
				{
					// call the function pointer
					fir::Value* result = fc->codegen(cgi, theFunction).value;
					iceAssert(result);

					return std::make_tuple((Func*) 0, (fir::Function*) 0, theFunction->getType()->toFunctionType()->getReturnType(), result);
				}
			}


			auto tup = GenError::getPrettyNoSuchFunctionError(cgi, fc->params, fns);
			std::string argstr = std::get<0>(tup);
			std::string candstr = std::get<1>(tup);
			HighlightOptions ops = std::get<2>(tup);

			ops.caret = fc->pin;

			error(fc, ops, "No such member function '%s' in class %s taking parameters (%s)\nPossible candidates (%zu):\n%s",
				fc->name.c_str(), cls->ident.name.c_str(), argstr.c_str(), fns.size(), candstr.c_str());
		}
	}


	iceAssert(res.resolved);

	// if ref is not 0, we need to call the function
	// this part handles vanilla member function calls.
	fir::Value* result = 0;
	Func* callee = 0;

	for(auto f : funclist)
	{
		if(f == res.t.funcDef)
		{
			callee = f;
			break;
		}
	}

	iceAssert(callee && "??");
	if(ref != 0)
	{
		std::vector<fir::Value*> args { ref };

		for(Expr* e : fc->params)
			args.push_back(e->codegen(cgi).value);


		// now we need to determine if it exists, and its params.
		iceAssert(callee);

		if(callee->decl->isStatic)
		{
			// remove the 'self' parameter
			args.erase(args.begin());
		}

		fir::Function* lcallee = res.t.firFunc;
		iceAssert(lcallee);

		lcallee = cgi->module->getFunction(lcallee->getName());
		iceAssert(lcallee);

		result = cgi->irb.CreateCall(lcallee, args);
	}

	return std::make_tuple(callee, res.t.firFunc, res.t.firFunc->getReturnType(), result);
}


































