// DotOperatorCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t MemberAccess::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* _rhs)
{
	// gen the var ref on the left.
	ValPtr_t p = this->target->codegen(cgi).result;

	llvm::Value* self = p.first;
	llvm::Value* selfPtr = p.second;

	if(selfPtr == nullptr)
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
			selfPtr = cgi->mainBuilder.CreateAlloca(self->getType());
			cgi->mainBuilder.CreateStore(self, selfPtr);
		}
	}

	bool isPtr = false;

	llvm::Type* type = p.first->getType();
	if(!type)
		error("(%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __LINE__);

	if(!type->isStructTy())
	{
		if(type->isPointerTy() && type->getPointerElementType()->isStructTy())
			type = type->getPointerElementType(), isPtr = true;

		else
			error(this, "Cannot do member access on non-struct types");
	}

	TypePair_t* pair = cgi->getType(type->getStructName());
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
					error(this, "Type '%s' does not have a member '%s'", str->name.c_str(), var->name.c_str());
			}
		}
		else if(!var && !fc)
		{
			error(this, "(%s:%d) -> Internal check failed: no comprehendo", __FILE__, __LINE__);
		}










		if(fc)
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

				#if 0
				printf("func %s vs %s\n", match.c_str(), f->decl->mangledName.c_str());
				#endif

				if(f->decl->mangledName == match)
				{
					callee = f;
					break;
				}
			}

			if(!callee)
				error(this, "Function '%s' is not a member of struct '%s'", fc->name.c_str(), str->name.c_str());

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
				error(this, "(%s:%d) -> Internal check failed: failed to find function %s", __FILE__, __LINE__, fc->name.c_str());

			lcallee = cgi->mainModule->getFunction(lcallee->getName());
			assert(lcallee);

			return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), 0);
		}
		else if(var)
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
				if(_rhs)
				{
					error("");
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
						error(this, "?!??!!");

					std::vector<llvm::Value*> args { isPtr ? self : selfPtr };
					return Result_t(cgi->mainBuilder.CreateCall(lcallee, args), 0);
				}
			}
			else
			{
				assert(i >= 0);

				// if we are a Struct* instead of just a Struct, we can just use pair.first since it's already a pointer.
				llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(isPtr ? self : selfPtr, i, "memberPtr_" + (fc ? fc->name : var->name));
				llvm::Value* val = cgi->mainBuilder.CreateLoad(ptr);
				return Result_t(val, ptr);
			}
		}
		else
		{
			error("(%s:%d) -> Internal check failed: not var or function?!", __FILE__, __LINE__);
		}
	}


	error("(%s:%d) -> Internal check failed: encountered invalid expression", __FILE__, __LINE__);
}











