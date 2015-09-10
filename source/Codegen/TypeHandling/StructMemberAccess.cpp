// DotOperatorCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

using namespace Ast;
using namespace Codegen;


Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, llvm::Value* ref, Struct* str, bool isStaticFunctionCall);
Result_t doVariable(CodegenInstance* cgi, VarRef* var, llvm::Value* ref, Struct* str, int i);
Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cp, llvm::Value* _rhs, llvm::Value* ref, Struct* str);
Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma, llvm::Value* ref, llvm::Value* rhs, bool actual = true);
Result_t doNamespaceAccess(CodegenInstance* cgi, MemberAccess* ma, std::deque<Expr*> flat, llvm::Value* rhs, bool actual = true);
Result_t checkForStaticAccess(CodegenInstance* cgi, MemberAccess* ma, llvm::Value* lhsPtr, llvm::Value* _rhs, bool actual = true);


Result_t ComputedProperty::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// handled elsewhere.
	return Result_t(0, 0);
}


Result_t MemberAccess::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* _rhs)
{
	if(this->matype != MAType::LeftVariable && this->matype != MAType::LeftFunctionCall)
	{
		iceAssert(this->matype != MAType::Invalid);

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
		std::deque<std::string> nsstrs;

		StructBase* curType = 0;
		TypePair_t* curTPair = 0;

		MemberAccess* cur = this;
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

		FunctionTree* ftree = cgi->getCurrentFuncTree(&nsstrs);
		while(list.size() > 0)
		{
			std::string front = list.front();
			list.pop_front();

			bool found = false;

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
						ftree = cgi->getCurrentFuncTree(&nsstrs);
						iceAssert(ftree);

						found = true;
						break;
					}
				}

				if(found)
					continue;


				if(TypePair_t* tp = cgi->getType(front))
				{
					iceAssert(tp->second.first);
					curType = dynamic_cast<StructBase*>(tp->second.first);
					curTPair = tp;
					iceAssert(curType);

					found = true;
					continue;
				}
			}
			else
			{
				for(auto sb : curType->nestedTypes)
				{
					if(sb->name == front)
					{
						curType = sb;

						found = true;
						break;
					}
				}

				if(found) continue;
			}

			std::string lscope = this->matype == MAType::LeftNamespace ? "namespace" : "type";
			error(cgi, this, "No such member %s in %s %s", front.c_str(), lscope.c_str(),
				lscope == "namespace" ? ftree->nsName.c_str() : (curType ? curType->name.c_str() : "uhm..."));
		}

		// what is the right side?
		if(FuncCall* fc = dynamic_cast<FuncCall*>(this->right))
		{
			if(curType == 0)
			{
				Resolved_t res = cgi->resolveFunctionFromList(this, ftree->funcs, fc->name, fc->params);
				if(!res.resolved)
					GenError::noFunctionTakingParams(cgi, fc, ftree->nsName, fc->name, fc->params);

				// call that sucker.
				// but first set the cached target.
				fc->cachedResolveTarget = res;
				return fc->codegen(cgi);
			}
			else
			{
				std::deque<FuncPair_t> flist;

				iceAssert(curType->funcs.size() == curType->lfuncs.size());

				for(size_t i = 0; i < curType->funcs.size(); i++)
				{
					if(curType->funcs[i]->decl->name == fc->name)
						flist.push_back(FuncPair_t(curType->lfuncs[i], curType->funcs[i]->decl));
				}


				Resolved_t res = cgi->resolveFunctionFromList(this, flist, fc->name, fc->params);
				if(!res.resolved)
					GenError::noFunctionTakingParams(cgi, fc, curType->name, fc->name, fc->params);


				fc->cachedResolveTarget = res;
				return fc->codegen(cgi);
			}
		}
		else if(VarRef* vr = dynamic_cast<VarRef*>(this->right))
		{
			if(curType == 0)
			{
				llvm::Value* ptr = 0;
				for(auto v : ftree->vars)
				{
					if(v.second->name == vr->name)
					{
						ptr = v.first.first;
						break;
					}
				}

				if(!ptr)
				{
					error(cgi, vr, "namespace %s does not contain a variable %s",
						ftree->nsName.c_str(), vr->name.c_str());
				}

				return Result_t(cgi->builder.CreateLoad(ptr), ptr);
			}
			else
			{
				// check static members
				if(dynamic_cast<Struct*>(curType))
				{
					for(auto v : curType->members)
					{
						if(v->isStatic && v->name == vr->name)
							return cgi->getStaticVariable(vr, curType, v->name);
					}
				}
				else if(dynamic_cast<Enumeration*>(curType))
				{
					return cgi->getEnumerationCaseValue(vr, curTPair, vr->name);
				}

				error(cgi, vr, "struct %s does not contain a static variable %s", curType->name.c_str(), vr->name.c_str());
			}
		}
		else
		{
			error(cgi, this, "Invalid expression type (%s) on right hand of dot operator", typeid(*this->right).name());
		}
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

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;


	bool isPtr = false;
	bool isWrapped = false;

	llvm::Type* type = self->getType();
	if(!type)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);


	if(cgi->isTypeAlias(type))
	{
		iceAssert(type->isStructTy());
		iceAssert(type->getStructNumElements() == 1);
		type = type->getStructElementType(0);

		warn(cgi, this, "typealias encountered");
		isWrapped = true;
	}


	if(!type->isStructTy())
	{
		if(type->isPointerTy() && type->getPointerElementType()->isStructTy())
		{
			type = type->getPointerElementType(), isPtr = true;
		}
		else
		{
			error(cgi, this, "Cannot do member access on non-struct type %s", cgi->getReadableType(type).c_str());
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
			if(selfPtr->getType()->isPointerTy() && selfPtr->getType()->getPointerElementType()->isPointerTy())
				selfPtr = cgi->builder.CreateLoad(selfPtr);
		}
		else
		{
			if(self->getType()->isPointerTy() && self->getType()->getPointerElementType()->isPointerTy())
				self = cgi->builder.CreateLoad(self);
		}
	}






	llvm::StructType* st = llvm::cast<llvm::StructType>(type);

	TypePair_t* pair = cgi->getType(type);
	if(!pair && (!st || (st && !st->isLiteral())))
	{
		error("(%s:%d) -> Internal check failed: failed to retrieve type (%s)", __FILE__, __LINE__, cgi->getReadableType(type).c_str());
	}
	else if(st && st->isLiteral())
	{
		type = st;
	}


	if((st && st->isLiteral()) || (pair->second.second == TypeKind::Tuple))
	{
		Number* n = dynamic_cast<Number*>(this->right);
		iceAssert(n);

		// if the lhs is immutable, don't give a pointer.
		bool immut = true;
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
				iceAssert(cgi->getStructMemberByName(str, var));
			}
		}
		else if(!var && !fc)
		{
			if(dynamic_cast<Number*>(rhs))
			{
				error(cgi, this, "Type '%s' is not a tuple", str->name.c_str());
			}
			else
			{
				error(cgi, this, "(%s:%d) -> Internal check failed: no comprehendo (%s)", __FILE__, __LINE__, typeid(*rhs).name());
			}
		}

		if(fc)
		{
			size_t i = 0;
			std::deque<FuncPair_t> candidates;
			for(auto f : str->funcs)
			{
				FuncPair_t fp = { str->lfuncs[i], f->decl };
				if(f->decl->name == fc->name && f->decl->isStatic)
					candidates.push_back(fp);

				i++;
			}

			Resolved_t res = cgi->resolveFunctionFromList(fc, candidates, fc->name, fc->params);
			if(res.resolved) return doFunctionCall(cgi, fc, isPtr ? self : selfPtr, str, true);


			return doFunctionCall(cgi, fc, isPtr ? self : selfPtr, str, false);
		}
		else if(var)
		{
			if(i >= 0)
			{
				return doVariable(cgi, var, isPtr ? self : selfPtr, str, i);
			}
			else
			{
				ComputedProperty* cprop = nullptr;
				for(ComputedProperty* c : str->cprops)
				{
					if(c->name == var->name)
					{
						cprop = c;
						break;
					}
				}

				if(cprop)
				{
					return doComputedProperty(cgi, var, cprop, _rhs, isPtr ? self : selfPtr, str);
				}
				else
				{
					return doStaticAccess(cgi, this, isPtr ? self : selfPtr, _rhs);
				}
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
		return doStaticAccess(cgi, this, isPtr ? self : selfPtr, _rhs);
	}

	iceAssert(!"Encountered invalid expression");
}







Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop,
	llvm::Value* _rhs, llvm::Value* ref, Struct* str)
{
	if(_rhs)
	{
		if(!cprop->setter)
		{
			error(var, "Property '%s' of type has no setter and is readonly", cprop->name.c_str());
		}

		llvm::Function* lcallee = 0;
		for(llvm::Function* lf : str->lfuncs)
		{
			// printf("candidate: %s vs %s\n", cprop->setterFunc->mangledName.c_str(), lf->getName().str().c_str());
			if(lf->getName() == cprop->setterFunc->mangledName)
			{
				lcallee = lf;
				break;
			}
		}

		if(!lcallee)
			error(cgi, var, "?!??!!");


		std::vector<llvm::Value*> args { ref, _rhs };

		// todo: rather large hack. since the nature of computed properties
		// is that they don't have a backing storage in the struct itself, we need
		// to return something. We're still used in a binOp though, so...

		// create a fake alloca to return to them.
		lcallee = cgi->module->getFunction(lcallee->getName());

		llvm::Value* val = cgi->builder.CreateCall(lcallee, args);
		llvm::Value* fake = cgi->allocateInstanceInBlock(_rhs->getType());

		cgi->builder.CreateStore(val, fake);

		return Result_t(val, fake);
	}
	else
	{
		llvm::Function* lcallee = 0;
		for(llvm::Function* lf : str->lfuncs)
		{
			if(lf->getName() == cprop->getterFunc->mangledName)
			{
				lcallee = lf;
				break;
			}
		}

		if(!lcallee)
			error(cgi, var, "?!??!!???");

		lcallee = cgi->module->getFunction(lcallee->getName());
		std::vector<llvm::Value*> args { ref };
		return Result_t(cgi->builder.CreateCall(lcallee, args), 0);
	}
}

Result_t doVariable(CodegenInstance* cgi, VarRef* var, llvm::Value* ref, Struct* str, int i)
{
	iceAssert(i >= 0);

	// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
	iceAssert(ref);

	llvm::Value* ptr = cgi->builder.CreateStructGEP(ref, i, "memberPtr_" + var->name);
	llvm::Value* val = cgi->builder.CreateLoad(ptr);

	if(str->members[i]->immutable)
		ptr = 0;

	return Result_t(val, ptr);
}

Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, llvm::Value* ref, Struct* str, bool isStaticFunctionCall)
{
	// make the args first.
	// since getting the llvm type of a MemberAccess can't be done without codegening the Ast itself,
	// we codegen first, then use the llvm version.
	std::vector<llvm::Value*> args { ref };

	for(Expr* e : fc->params)
		args.push_back(e->codegen(cgi).result.first);


	// now we need to determine if it exists, and its params.
	Func* callee = cgi->getFunctionFromStructFuncCall(str, fc);
	iceAssert(callee);

	if(callee->decl->isStatic)
	{
		// remove the 'self' parameter
		args.erase(args.begin());
	}


	if(callee->decl->isStatic != isStaticFunctionCall)
	{
		error(cgi, fc, "Cannot call instance method '%s' without an instance", callee->decl->name.c_str());
	}




	llvm::Function* lcallee = 0;
	for(llvm::Function* lf : str->lfuncs)
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








std::tuple<llvm::Type*, llvm::Value*, Ast::Expr*>
CodegenInstance::resolveDotOperator(MemberAccess* _ma, bool doAccess, std::deque<std::string>* _scp)
{
	auto flat = this->flattenDotOperators(_ma);
	if(flat.size() > 0)
	{
		if(VarRef* vr = dynamic_cast<VarRef*>(flat.front()))
		{
			// TODO: copy-pasta
			// check for type function access
			TypePair_t* tp = 0;
			if((tp = this->getType(this->mangleWithNamespace(vr->name, false))))
			{
				if(tp->second.second == TypeKind::Enum)
				{
					error(this, _ma, "enosup");
				}
				else if(tp->second.second == TypeKind::Struct)
				{
					flat.pop_front();

					Result_t res = doStaticAccess(this, _ma, 0, 0, false);
					return std::make_tuple(res.result.first->getType(), (llvm::Value*) 0, flat.back());
				}
			}

			// todo: do something with this
			std::deque<NamespaceDecl*> nses = this->resolveNamespace(vr->name);
			if(nses.size() > 0)
			{
				auto res = doNamespaceAccess(this, _ma, flat, 0, false);
				return std::make_tuple(res.result.first->getType(), (llvm::Value*) 0, flat.back());
			}
		}
	}










	TypePair_t* tp = 0;
	StructBase* sb = 0;

	std::deque<std::string>* scp = 0;
	if(_scp == 0)
		scp = new std::deque<std::string>();		// todo: this will leak.

	else
		scp = _scp;


	iceAssert(scp);
	if(MemberAccess* ma = dynamic_cast<MemberAccess*>(_ma->left))
	{
		// (d)
		auto ret = this->resolveDotOperator(ma, false, scp);
		tp = this->getType(std::get<0>(ret));

		iceAssert(tp);
	}
	else if(VarRef* vr = dynamic_cast<VarRef*>(_ma->left))
	{
		// (e)

		std::string mname;
		if(scp != 0)
			mname = this->mangleWithNamespace(vr->name, *scp, false);

		else
			mname = this->mangleWithNamespace(vr->name, false);


		tp = this->getType(mname);

		if(!tp)
		{
			// (b)
			llvm::Type* lt = this->getLlvmType(vr);
			iceAssert(lt);

			tp = this->getType(lt);
			iceAssert(tp);
		}
	}
	else if(FuncCall* fc = dynamic_cast<FuncCall*>(_ma->left))
	{
		llvm::Type* lt = this->parseTypeFromString(_ma->left, fc->type.strType);
		iceAssert(lt);

		tp = this->getType(lt);
		iceAssert(tp);
	}

	sb = dynamic_cast<StructBase*>(tp->second.first);
	iceAssert(sb);


	// (b)
	scp->push_back(sb->name);

	VarRef* var = dynamic_cast<VarRef*>(_ma->right);
	FuncCall* fc = dynamic_cast<FuncCall*>(_ma->right);

	if(var)
	{
		iceAssert(this->getStructMemberByName(sb, var));
	}
	else if(fc)
	{
		iceAssert(this->getFunctionFromStructFuncCall(sb, fc));
	}
	else
	{
		if(dynamic_cast<Number*>(_ma->right))
		{
			error(this, _ma->right, "Type '%s' is not a tuple", sb->name.c_str());
		}
		else
		{
			error(this, _ma->right, "(%s:%d) -> Internal check failed: no comprehendo (%s)", __FILE__, __LINE__, typeid(*_ma->right).name());
		}
	}

	llvm::Type* type = 0;
	if(var)
	{
		for(auto vd : sb->members)
		{
			if(var->name == vd->name)
			{
				type = this->getLlvmType(vd);
				break;
			}
		}
		iceAssert(type);
	}
	else if(fc)
	{
		Func* fn = getFunctionFromStructFuncCall(sb, fc);
		type = this->parseTypeFromString(_ma->left, fn->decl->type.strType);
		iceAssert(type);
	}

	return std::make_tuple(type, (llvm::Value*) 0, _ma->right);
}



Func* CodegenInstance::getFunctionFromStructFuncCall(StructBase* str, FuncCall* fc)
{
	// now we need to determine if it exists, and its params.
	Func* callee = nullptr;
	for(Func* f : str->funcs)
	{
		std::string match = this->mangleMemberFunction(str, fc->name, fc->params, str->scope);
		std::string funcN = this->mangleMemberFunction(str, f->decl->name, f->decl->params, str->scope, f->decl->isStatic);

		#if 0
		printf("func %s vs %s, orig %s\n", match.c_str(), funcN.c_str(), f->decl->name.c_str());
		#endif

		if(funcN == match)
		{
			callee = f;
			break;
		}
	}

	if(!callee)
		error(this, fc, "Function '%s' is not a member of struct '%s'", fc->name.c_str(), str->name.c_str());

	return callee;
}


Struct* CodegenInstance::getNestedStructFromScopes(Expr* user, std::deque<std::string> scopes)
{
	iceAssert(scopes.size() > 0);

	std::string last = scopes.back();
	scopes.pop_back();

	TypePair_t* tp = this->getType(this->mangleWithNamespace(last, scopes.size() > 0 ? scopes : this->namespaceStack, false));
	if(!tp)
		GenError::unknownSymbol(this, user, last, SymbolType::Type);

	Struct* str = dynamic_cast<Struct*>(tp->second.first);
	iceAssert(str);

	return str;
}

Expr* CodegenInstance::getStructMemberByName(StructBase* str, VarRef* var)
{
	Expr* found = 0;
	for(auto c : str->cprops)
	{
		if(c->name == var->name)
		{
			found = c;
			break;
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


static void _flattenDotOperators(MemberAccess* base, std::deque<Expr*>& list)
{
	Expr* left = base->left;
	Expr* right = base->right;

	if(MemberAccess* ma = dynamic_cast<MemberAccess*>(left))
		_flattenDotOperators(ma, list);

	else
		list.push_back(left);


	list.push_back(right);
}

std::deque<Expr*> CodegenInstance::flattenDotOperators(MemberAccess* base)
{
	std::deque<Expr*> list;
	_flattenDotOperators(base, list);

	return list;
}




































