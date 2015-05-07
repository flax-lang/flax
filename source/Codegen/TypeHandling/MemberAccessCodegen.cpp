// DotOperatorCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#include "ast.h"
#include "codegen.h"
#include "llvm_all.h"

using namespace Ast;
using namespace Codegen;


static Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str,
	bool isStaticFunctionCall);

static Result_t doVariable(CodegenInstance* cgi, VarRef* var, llvm::Value* _rhs, llvm::Value* self, llvm::Value* selfPtr,
	bool isPtr, Struct* str, int i);

static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, llvm::Value* _rhs, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str);

static Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma);














Result_t ComputedProperty::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// handled elsewhere.
	return Result_t(0, 0);
}



Result_t MemberAccess::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* _rhs)
{
	// check for special cases -- static calling and enums.
	VarRef* _vr = dynamic_cast<VarRef*>(this->target);
	if(_vr)
	{
		// check for type function access
		TypePair_t* tp = 0;
		if((tp = cgi->getType(cgi->mangleWithNamespace(_vr->name, false))))
		{
			if(tp->second.second == TypeKind::Enum)
			{
				return enumerationAccessCodegen(cgi, this->target, this->member);
			}
			else if(tp->second.second == TypeKind::Struct)
			{
				return doStaticAccess(cgi, this);
			}
		}
	}

	// gen the var ref on the left.
	Result_t res = this->target->codegen(cgi);
	ValPtr_t p = res.result;

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;


	bool isPtr = false;
	bool isWrapped = false;

	llvm::Type* type = self->getType();
	if(!type)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);



	// if(!self)
	// 	warn(cgi, this, "self is null! (%s, %s)", (typeid(*this->target)).name(), cgi->getReadableType(type).c_str());

	// if(!selfPtr)
	// 	warn(cgi, this, "selfptr is null! (%s, %s)", (typeid(*this->target)).name(), cgi->getReadableType(type).c_str());




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
			error(cgi, this, "Cannot do member access on non-struct types");
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
			cgi->mainBuilder.CreateStore(self, selfPtr);
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
				selfPtr = cgi->mainBuilder.CreateLoad(selfPtr);
		}
		else
		{
			if(self->getType()->isPointerTy() && self->getType()->getPointerElementType()->isPointerTy())
				self = cgi->mainBuilder.CreateLoad(self);
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
		// todo: maybe move this to another file?
		// like tuplecodegen.cpp

		// quite simple, just get the number (make sure it's a Ast::Number)
		// and do a structgep.

		Number* n = dynamic_cast<Number*>(this->member);
		iceAssert(n);

		if(n->ival >= type->getStructNumElements())
			error(cgi, this, "Tuple does not have %d elements, only %d", (int) n->ival + 1, type->getStructNumElements());

		llvm::Value* gep = cgi->mainBuilder.CreateStructGEP(selfPtr, n->ival);

		// if the lhs is immutable, don't give a pointer.
		bool immut = false;
		if(VarRef* vr = dynamic_cast<VarRef*>(this->target))
		{
			VarDecl* vd = cgi->getSymDecl(this, vr->name);
			iceAssert(vd);

			immut = vd->immutable;
		}

		return Result_t(cgi->mainBuilder.CreateLoad(gep), immut ? 0 : gep);
	}
	else if(pair->second.second == TypeKind::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);

		iceAssert(str);
		iceAssert(self);

		// transform
		Expr* rhs = this->member;


		// get the index for the member
		// Expr* rhs = this->member;
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
			return doFunctionCall(cgi, fc, self, selfPtr, isPtr, str, false);
		}
		else if(var)
		{
			if(i >= 0)
			{
				return doVariable(cgi, var, _rhs, self, selfPtr, isPtr, str, i);
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
					return doComputedProperty(cgi, var, cprop, _rhs, self, selfPtr, isPtr, str);
				}
				else
				{
					return doStaticAccess(cgi, this);
				}
			}
		}
		else
		{
			iceAssert(!"Not var or function?!");
		}
	}

	iceAssert(!"Encountered invalid expression");
}






















namespace Codegen
{
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
		Expr* left = base->target;
		Expr* right = base->member;

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
}







static Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str, bool isStaticFunctionCall)
{
	// make the args first.
	// since getting the llvm type of a MemberAccess can't be done without codegening the Ast itself,
	// we codegen first, then use the llvm version.
	std::vector<llvm::Value*> args;

	args.push_back(isPtr ? self : selfPtr);
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

	lcallee = cgi->mainModule->getFunction(lcallee->getName());
	iceAssert(lcallee);

	return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), 0);
}


static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, llvm::Value* _rhs, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str)
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
			if(lf->getName() == cprop->generatedFunc->mangledName)
			{
				lcallee = lf;
				break;
			}
		}

		if(!lcallee)
			error(var, "?!??!!");


		std::vector<llvm::Value*> args { isPtr ? self : selfPtr, _rhs };

		// todo: rather large hack. since the nature of computed properties
		// is that they don't have a backing storage in the struct itself, we need
		// to return something. We're still used in a binOp though, so...

		// create a fake alloca to return to them.
		lcallee = cgi->mainModule->getFunction(lcallee->getName());
		return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), cgi->allocateInstanceInBlock(_rhs->getType()));
	}
	else
	{
		llvm::Function* lcallee = 0;
		for(llvm::Function* lf : str->lfuncs)
		{
			if(lf->getName() == cprop->generatedFunc->mangledName)
			{
				lcallee = lf;
				break;
			}
		}

		if(!lcallee)
			error(var, "?!??!!");

		lcallee = cgi->mainModule->getFunction(lcallee->getName());
		std::vector<llvm::Value*> args { isPtr ? self : selfPtr };
		return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), 0);
	}
}

static Result_t doVariable(CodegenInstance* cgi, VarRef* var, llvm::Value* _rhs, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str, int i)
{
	iceAssert(i >= 0);

	// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
	iceAssert(self || selfPtr);

	// printf("*** self: %s\n*** selfptr: %s\n*** isPtr: %d\n", cgi->getReadableType(self).c_str(), cgi->getReadableType(selfPtr).c_str(), isPtr);
	llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + var->name);
	llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);

	if(str->members[i]->immutable)
		ptr = 0;

	return Result_t(val, ptr);
}



static Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma)
{
	std::deque<Expr*> flattened = cgi->flattenDotOperators(ma);

	return Result_t(0, 0);












	// std::deque<std::string> scopes;
	// Expr* rightmost = std::get<2>(cgi->resolveDotOperator(ma->target, ma->member, false, &scopes));
	// iceAssert(rightmost);

	// Struct* str = cgi->getNestedStructFromScopes(ma, scopes);


	// if(FuncCall* fc = dynamic_cast<FuncCall*>(rightmost))
	// {
	// 	return doFunctionCall(cgi, fc, 0, 0, false, str, true);
	// }
	// else if(VarRef* vr = dynamic_cast<VarRef*>(rightmost))
	// {
	// 	for(auto mem : str->members)
	// 	{
	// 		if(mem->isStatic && mem->name == vr->name)
	// 		{
	// 			std::string mangledName = cgi->mangleMemberFunction(str, mem->name, std::deque<Ast::Expr*>());
	// 			if(llvm::GlobalVariable* gv = cgi->mainModule->getGlobalVariable(mangledName))
	// 			{
	// 				// todo: another kinda hacky thing.
	// 				// this is present in some parts of the code, i don't know how many.
	// 				// basically, if the thing is supposed to be immutable, we're not going to return
	// 				// the ptr/ref value.

	// 				return Result_t(cgi->mainBuilder.CreateLoad(gv), gv->isConstant() ? 0 : gv);
	// 			}
	// 		}
	// 	}

	// 	error(cgi, ma, "Struct '%s' has no such static member '%s'", str->name.c_str(), vr->name.c_str());
	// }

	// error(cgi, ma, "Invalid static access (%s)", typeid(*rightmost).name());
}







std::tuple<llvm::Type*, llvm::Value*, Ast::Expr*>
CodegenInstance::resolveDotOperator(Expr* lhs, Expr* rhs, bool doAccess, std::deque<std::string>* _scp)
{
	TypePair_t* tp = 0;
	StructBase* sb = 0;

	std::deque<std::string>* scp = 0;
	if(_scp == 0)
		scp = new std::deque<std::string>();		// todo: this will leak.

	else
		scp = _scp;


	iceAssert(scp);
	if(MemberAccess* ma = dynamic_cast<MemberAccess*>(lhs))
	{
		// (d)
		auto ret = this->resolveDotOperator(ma->target, ma->member, false, scp);
		tp = this->getType(std::get<0>(ret));

		iceAssert(tp);
	}
	else if(VarRef* vr = dynamic_cast<VarRef*>(lhs))
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
	else if(FuncCall* fc = dynamic_cast<FuncCall*>(lhs))
	{
		llvm::Type* lt = this->parseTypeFromString(lhs, fc->type.strType);
		iceAssert(lt);

		tp = this->getType(lt);
		iceAssert(tp);
	}

	sb = dynamic_cast<StructBase*>(tp->second.first);
	iceAssert(sb);


	// (b)
	scp->push_back(sb->name);

	VarRef* var = dynamic_cast<VarRef*>(rhs);
	FuncCall* fc = dynamic_cast<FuncCall*>(rhs);

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
		if(dynamic_cast<Number*>(rhs))
		{
			error(this, rhs, "Type '%s' is not a tuple", sb->name.c_str());
		}
		else
		{
			error(this, rhs, "(%s:%d) -> Internal check failed: no comprehendo (%s)", __FILE__, __LINE__, typeid(*rhs).name());
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
				iceAssert(type);
				break;
			}
		}
	}
	else if(fc)
	{
		Func* fn = getFunctionFromStructFuncCall(sb, fc);
		type = this->parseTypeFromString(lhs, fn->decl->type.strType);
		iceAssert(type);
	}


	return std::make_tuple(type, (llvm::Value*) 0, rhs);
}


















