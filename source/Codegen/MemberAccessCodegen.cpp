// DotOperatorCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


static Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str,
	bool isStaticFunctionCall);

static Result_t doVariable(CodegenInstance* cgi, VarRef* var, llvm::Value* _rhs, llvm::Value* self, llvm::Value* selfPtr,
	bool isPtr, Struct* str, int i);

static Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, llvm::Value* _rhs, llvm::Value* self, llvm::Value* selfPtr, bool isPtr, Struct* str);

static Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma);
static Expr* rearrangeNonStaticAccess(CodegenInstance* cgi, MemberAccess* ma);















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


	Expr* re = rearrangeNonStaticAccess(cgi, this);
	(void) re;

	// gen the var ref on the left.
	Result_t res = this->target->codegen(cgi);
	ValPtr_t p = res.result;

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;


	if(!self)
		warn(cgi, this, "self is null! (%s)", (typeid(*this->target)).name());

	if(!selfPtr)
		warn(cgi, this, "selfptr is null! (%s)", (typeid(*this->target)).name());


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

		if(lhsPtr)
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
		// a.(b.(c.(d.e))) into (((a.b).c).d).e
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
				bool found = false;
				for(auto c : str->cprops)
				{
					if(c->name == var->name)
					{
						found = true;
						break;
					}
				}

				if(!found)
				{
					error(cgi, this, "Type '%s' does not have a member '%s'", str->name.c_str(), var->name.c_str());
				}
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
			return doVariable(cgi, var, _rhs, self, selfPtr, isPtr, str, i);
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

	static Expr* _recursivelyResolveNested(MemberAccess* base, std::deque<std::string>& scopes)
	{
		VarRef* left = dynamic_cast<VarRef*>(base->target);
		iceAssert(left);

		scopes.push_back(left->name);

		MemberAccess* maR = dynamic_cast<MemberAccess*>(base->member);
		FuncCall* fcR = dynamic_cast<FuncCall*>(base->member);

		if(maR)
		{
			return _recursivelyResolveNested(maR, scopes);
		}
		else
		{
			return fcR;
		}
	}

	Expr* CodegenInstance::recursivelyResolveNested(MemberAccess* base, std::deque<std::string>* __scopes)
	{
		VarRef* left = dynamic_cast<VarRef*>(base->target);
		iceAssert(left);

		std::deque<std::string> tmpscopes;
		std::deque<std::string>* _scopes = nullptr;

		// fuck this shit. we need to know if we were originally passed a non-null.
		if(!__scopes)
			_scopes = &tmpscopes;

		else
			_scopes = __scopes;


		std::deque<std::string>& scopes = *_scopes;


		MemberAccess* maR = dynamic_cast<MemberAccess*>(base->member);
		FuncCall* fcR = dynamic_cast<FuncCall*>(base->member);

		scopes.push_back(left->name);
		if(maR)
		{
			// kinda hacky behaviour.
			// if we call with _scopes != 0, that means
			// we're interested in the function call.

			// if not, then we're only interested in the type.


			Expr* ret = _recursivelyResolveNested(maR, scopes);

			// todo: handle static vars
			FuncCall* fc = dynamic_cast<FuncCall*>(ret);
			iceAssert(fc);

			if(__scopes != nullptr)
			{
				return fc;
			}
			else
			{
				Struct* str = this->getNestedStructFromScopes(base, scopes);
				return this->getFunctionFromStructFuncCall(str, fc);
			}
		}
		else
		{
			return fcR;
		}
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
		iceAssert(i >= 0);

		// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
		llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + var->name);
		llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);

		if(str->members[i]->immutable)
			ptr = 0;

		return Result_t(val, ptr);
	}
}



static Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma)
{
	std::deque<std::string> scopes;
	Expr* rightmost = cgi->recursivelyResolveNested(ma, &scopes);
	FuncCall* fc = dynamic_cast<FuncCall*>(rightmost);

	// todo: static vars
	iceAssert(fc);

	Struct* str = cgi->getNestedStructFromScopes(ma, scopes);
	return doFunctionCall(cgi, fc, 0, 0, false, str, true);
}








static Expr* rearrangeNonStaticAccess(CodegenInstance* cgi, MemberAccess* base)
{
	std::vector<Expr*> stack;
	std::vector<MemberAccess*> mas;
	Expr* expr = base;
	Expr* rightmost = base->member;

	MemberAccess* curr = nullptr;

	size_t i = 0;
	while(dynamic_cast<MemberAccess*>(expr))
	{
		MemberAccess* ma = dynamic_cast<MemberAccess*>(expr);
		stack.push_back(ma->target);
		mas.push_back(ma);

		expr = ma->member;

		curr = ma;
		i++;
	}

	stack.push_back(expr);

	// now that everything is in a nice list
	// transform a.(b.(c.d)) into ((a.b).c).d
	// start at the front of the list, and the last ma.

	iceAssert(mas.size() == stack.size() - 1);
	iceAssert(stack.size() > 1);
	mas.back()->target = stack[0];
	mas.back()->member = stack[1];

	i = 2;
	while(mas.size() > 1)
	{
		MemberAccess* mbr = mas.back();
		mas.pop_back();

		MemberAccess* newback = mas.back();
		newback->target = mbr;
		newback->member = stack[i];
		i++;
	}


	return rightmost;
}


















