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

struct LeftSideResolved;
static Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma, LeftSideResolved* lsr);




struct LeftSideResolved : Expr
{
	LeftSideResolved() : Expr(Parser::PosInfo()) { }
	~LeftSideResolved() { }
	virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override { return Result_t(0, 0); }
};














Result_t MemberAccess::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* _rhs)
{
	VarRef* _vr = dynamic_cast<VarRef*>(this->target);
	if(_vr)
	{
		// check for type function access
		TypePair_t* tp = 0;
		if((tp = cgi->getType(cgi->mangleWithNamespace(_vr->name, false))))
		{
			if(tp->second.second == ExprType::Enum)
			{
				return enumerationAccessCodegen(cgi, this->target, this->member);
			}
			else if(tp->second.second == ExprType::Struct)
			{
				return doStaticAccess(cgi, this, nullptr);
			}
		}
	}


	// gen the var ref on the left.
	Result_t res = this->target->codegen(cgi);
	ValPtr_t p = res.result;

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;

	if(dynamic_cast<LeftSideResolved*>(res.hackyReturn))
	{
		return doStaticAccess(cgi, this, dynamic_cast<LeftSideResolved*>(res.hackyReturn));
	}





	bool isPtr = false;
	bool isWrapped = false;

	llvm::Type* type = p.first->getType();
	if(!type)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);

	if(cgi->isTypeAlias(type))
	{
		assert(type->isStructTy());
		assert(type->getStructNumElements() == 1);
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

	if(isWrapped)
	{
		bool wasSelfPtr = false;

		if(selfPtr)
		{
			selfPtr = cgi->lastMinuteUnwrapType(selfPtr);
			wasSelfPtr = true;
			isPtr = false;
		}
		else
		{
			self = cgi->lastMinuteUnwrapType(self);
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




	TypePair_t* pair = cgi->getType(type);
	if(!pair)
	{
		error("(%s:%d) -> Internal check failed: failed to retrieve type (%s)", __FILE__, __LINE__, cgi->getReadableType(type).c_str());
	}


	if(pair->second.second == ExprType::Struct)
	{
		Struct* str = dynamic_cast<Struct*>(pair->second.first);

		assert(str);
		assert(self);

		// get the index for the member
		Expr* rhs = this->member;
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
			error(cgi, this, "(%s:%d) -> Internal check failed: no comprehendo", __FILE__, __LINE__);
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
			error("(%s:%d) -> Internal check failed: not var or function?!", __FILE__, __LINE__);
		}
	}


	error("(%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __LINE__);
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

	// need to remove the dummy 'self' reference
	// now we need to determine if it exists, and its params.
	Func* callee = nullptr;
	for(Func* f : str->funcs)
	{
		std::string match = cgi->mangleMemberFunction(str, fc->name, fc->params, str->scope);
		std::string funcN = cgi->mangleMemberFunction(str, f->decl->name, f->decl->params, str->scope, f->decl->isStatic);

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
		error(fc, "Function '%s' is not a member of struct '%s'", fc->name.c_str(), str->name.c_str());

	if(callee->decl->isStatic)
	{
		// remove the 'self' parameter
		args.erase(args.begin());
	}


	if(callee->decl->isStatic != isStaticFunctionCall)
	{
		error(fc, "Cannot call instance method '%s' without an instance", callee->decl->name.c_str());
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
	assert(lcallee);

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
		assert(i >= 0);

		// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
		llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + var->name);
		llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);
		return Result_t(val, ptr);
	}
}






static void accumulateNamespace(MemberAccess* ma, std::deque<std::string>* nses)
{
	if(!ma) return;

	MemberAccess* left = dynamic_cast<MemberAccess*>(ma->target);
	if(left)
	{
		accumulateNamespace(left, nses);
	}
	else
	{
		VarRef* left = dynamic_cast<VarRef*>(ma->target);

		assert(left);
		nses->push_back(left->name);
	}

	VarRef* right = dynamic_cast<VarRef*>(ma->member);
	if(right)
		nses->push_back(right->name);
}

static Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma, LeftSideResolved* lsr)
{
	VarRef* leftVr = dynamic_cast<VarRef*>(ma->target);
	MemberAccess* leftMa = dynamic_cast<MemberAccess*>(ma->target);

	FuncCall* right = dynamic_cast<FuncCall*>(ma->member);
	if(right)
	{
		// we can stop here.
		std::deque<std::string> nses;

		if(leftMa)
		{
			accumulateNamespace(leftMa, &nses);
			assert(nses.size() > 0);
		}
		else
		{
			assert(leftVr);
			nses.push_back(leftVr->name);
		}

		std::string last = nses.back();
		nses.pop_back();

		TypePair_t* tp = cgi->getType(cgi->mangleWithNamespace(last, nses.size() > 0 ? nses : cgi->namespaceStack, false));
		if(!tp)
			GenError::unknownSymbol(cgi, right, last, SymbolType::Type);

		Struct* str = dynamic_cast<Struct*>(tp->second.first);
		assert(str);

		return doFunctionCall(cgi, right, nullptr, nullptr, false, str, true);
	}
	else if(leftVr || leftMa)
	{
		// it doesn't matter what we have inside.
		// we always have a back-reference to ma->target, which can be another MemberAccess.
		// therefore, using this method, we "trick" the codegen to sustain our tomfoolery
		// such that eventually, we'll end up back here, and once our RHS is a function call or a legit variable,
		// we can do the proper stuff.

		return Result_t(0, 0, new LeftSideResolved());
	}
	else
	{
		error(ma, "Unknown expression type %s", typeid(*ma->member).name());
	}
}




















