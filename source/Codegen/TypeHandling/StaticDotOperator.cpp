// StaticDotOperator.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

using namespace Ast;
using namespace Codegen;

Result_t doVariable(CodegenInstance* cgi, VarRef* var, llvm::Value* ref, Struct* str, int i);
Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma, llvm::Value* ref, llvm::Value* rhs, bool actual);
Result_t doFunctionCall(CodegenInstance* cgi, FuncCall* fc, llvm::Value* ref, Struct* str, bool isStaticFunctionCall);
Result_t doNamespaceAccess(CodegenInstance* cgi, MemberAccess* ma, std::deque<Expr*> flat, llvm::Value* rhs, bool actual);
Result_t checkForStaticAccess(CodegenInstance* cgi, MemberAccess* ma, llvm::Value* lhsPtr, llvm::Value* _rhs, bool actual = true);
Result_t doComputedProperty(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop,	llvm::Value* _rhs, llvm::Value* ref, Struct* str);


static Result_t getStaticVariable(CodegenInstance* cgi, Expr* user, StructBase* str, std::string name)
{
	std::string mangledName = cgi->mangleMemberFunction(str, name, std::deque<Ast::Expr*>());
	if(llvm::GlobalVariable* gv = cgi->module->getGlobalVariable(mangledName))
	{
		// todo: another kinda hacky thing.
		// this is present in some parts of the code, i don't know how many.
		// basically, if the thing is supposed to be immutable, we're not going to return
		// the ptr/ref value.

		return Result_t(cgi->builder.CreateLoad(gv), gv->isConstant() ? 0 : gv);
	}

	error(cgi, user, "Struct '%s' has no such static member '%s'", str->name.c_str(), name.c_str());
}



static Result_t _doStaticAccess(CodegenInstance* cgi, StructBase* str, llvm::Value* ref,
	llvm::Value* rhs, std::deque<Expr*>& list, bool actual)
{
	// what is the next one?
	Result_t res = Result_t(0, 0);
	if(list.size() == 0)
	{
		if(ref)	return Result_t(llvm::Constant::getNullValue(ref->getType()->getPointerElementType()), ref);
		else	return Result_t(0, 0);
	}

	if(VarRef* vr = dynamic_cast<VarRef*>(list.front()))
	{
		// check static members.
		bool found = false;
		for(auto vd : str->members)
		{
			if(vd->name == vr->name)
			{
				found = true;

				if(actual)
				{
					if(vd->isStatic)
					{
						res = getStaticVariable(cgi, vr, str, vd->name);
					}
					else
					{
						int i = str->nameMap[vd->name];
						iceAssert(i >= 0);

						res = doVariable(cgi, vr, ref, (Struct*) str, i);
					}
				}
				else
				{
					return Result_t(llvm::Constant::getNullValue(cgi->getLlvmType(vd)), 0);
				}
			}
		}

		for(auto cp : str->cprops)
		{
			if(cp->name == vr->name)
			{
				found = true;

				if(actual)
					res = doComputedProperty(cgi, vr, cp, rhs, ref, (Struct*) str);

				else
					return Result_t(llvm::Constant::getNullValue(cgi->getLlvmType(cp)), 0);
			}
		}

		for(auto n : str->nestedTypes)
		{
			if(n->name == vr->name)
			{
				// hack? maybe.
				std::string mangled = cgi->mangleWithNamespace(n->name, n->scope, false);
				TypePair_t* tp = cgi->getType(mangled);
				iceAssert(tp);


				list.pop_front();

				if(list.size() > 0)
				{
					return _doStaticAccess(cgi, (Struct*) tp->second.first, ref, rhs, list, actual);
				}
				else
				{
					return Result_t(llvm::Constant::getNullValue(tp->first), 0);
				}
			}
		}

		if(Enumeration* enr = dynamic_cast<Enumeration*>(str))
		{
			for(auto c : enr->cases)
			{
				if(c.first == vr->name)
				{
					found = true;

					if(actual)
					{
						res = enumerationAccessCodegen(cgi, new VarRef(vr->posinfo, enr->name), vr);
					}
					else
						res = Result_t(llvm::Constant::getNullValue(cgi->getLlvmType(enr)), 0);

					break;
				}
			}
		}

		if(found)
		{
			list.pop_front();
		}
		else
		{
			error(cgi, vr, "Struct '%s' has no such static member '%s'", str->name.c_str(), vr->name.c_str());
		}
	}
	else if(FuncCall* fc = dynamic_cast<FuncCall*>(list.front()))
	{
		list.pop_front();

		if(actual)
			res = doFunctionCall(cgi, fc, ref, (Struct*) str, true);

		else
			res = Result_t(llvm::Constant::getNullValue(cgi->getLlvmType(cgi->getFunctionFromStructFuncCall(str, fc))), 0);
	}
	else
	{
		error(cgi, list.front(), "???!!!");
	}



	// use 'res' to call more stuff.
	llvm::Value* newref = res.result.second;
	if(actual && !newref && !res.result.first->getType()->isVoidTy())
	{
		iceAssert(res.result.first);
		llvm::Value* _ref = cgi->allocateInstanceInBlock(res.result.first->getType());

		cgi->builder.CreateStore(res.result.first, _ref);
		newref = _ref;
	}


	// change 'str' if we need to
	// ie. when we go deeper, like if the current vr is a struct.
	if(actual && newref && newref->getType()->getPointerElementType()->isStructTy())
	{
		TypePair_t* tp = cgi->getType(newref->getType()->getPointerElementType());
		iceAssert(tp);

		str = dynamic_cast<StructBase*>(tp->second.first);
		iceAssert(str);
	}


	if(list.size() > 0)
		return _doStaticAccess(cgi, str, newref, rhs, list, actual);

	return Result_t(res.result.first, newref);
}


Result_t doStaticAccess(CodegenInstance* cgi, MemberAccess* ma, llvm::Value* ref, llvm::Value* rhs, bool actual)
{
	std::deque<Expr*> flattened = cgi->flattenDotOperators(ma);

	VarRef* vl = dynamic_cast<VarRef*>(flattened.front());
	iceAssert(vl);

	TypePair_t* tp = cgi->getType(cgi->mangleWithNamespace(vl->name));
	iceAssert(tp);

	if(!tp) GenError::unknownSymbol(cgi, vl, vl->name, SymbolType::Type);

	Struct* str = dynamic_cast<Struct*>(tp->second.first);
	iceAssert(str);


	flattened.pop_front();
	return _doStaticAccess(cgi, str, ref, rhs, flattened, actual);
}







static Result_t doRecursiveNSResolution(CodegenInstance* cgi, std::deque<Expr*> flat, std::deque<MemberAccess*> mas,
	bool actual, std::deque<std::string> nsstrs, Result_t prevRes, bool isFirst)
{
	if(flat.size() == 0)
		return prevRes;


	Expr* fr = flat.front();
	flat.pop_front();

	VarRef* vr = dynamic_cast<VarRef*>(fr);
	FuncCall* fc = dynamic_cast<FuncCall*>(fr);


	if(mas.size() > 0 && !isFirst)
		mas.pop_front();

	if(vr)
	{
		// we need to try some stuff.
		// variable declaration checker *SHOULD* have already checked for conflicts between
		// namespaces and variable decls... so. if no var, should be namespace.

		if(nsstrs.size() > 0)
		{
			FunctionTree* ft = cgi->getCurrentFuncTree(&nsstrs);
			for(auto var : ft->vars)
			{
				if(var.second->name == vr->name)
				{
					// get it.
					if(actual)
					{
						prevRes = Result_t(cgi->builder.CreateLoad(var.first.first), var.first.first);
					}
					else
					{
						// not actual.
						// TODO(ASAP, BROKEN, MUSTFIX):
						// this is dumb, and creates side effects, especially for functions
						// since they'll be called multiple times.
						// we need a way to be able to determine the final type of the entire expression tree
						// without actually doing the codegen.

						// potential (SHITTY) solution: have a way to push a "nongeneration block/scope"
						// and have anything codegened within that scope not appear in the final IR.
						// this means we can call codegen() as many times as we want, as long as
						// it's within the scope. (ie. between a set of push/popscope calls)
						// this is clearly not an optimal solution, but it is a solution nontheless.
						// NOTE: can be a fallback if we cannot find a better solution.

						prevRes = Result_t(cgi->builder.CreateLoad(var.first.first), var.first.first);
					}

					// hand off control to non-static resolution and codegen.
					// mas.front() contains the member access -- the left side which we just generated,
					// and the right side that we hand off.

					iceAssert(mas.size() > 0);
					MemberAccess* curma = mas.front();

					curma->cachedCodegenResult = prevRes;
					for(auto m : mas) m->disableStaticChecking = true;

					return mas.back()->codegen(cgi);
				}
			}
		}


		if(flat.size() == 0)
		{
			error(cgi, vr, "Unexpected end of namespace chain (last bit = %s)", vr->name.c_str());
		}

		nsstrs.push_back(vr->name);
		return doRecursiveNSResolution(cgi, flat, mas, actual, nsstrs, Result_t(0, 0), false);
	}
	else if(fc)
	{
		FunctionTree* ftree = cgi->getCurrentFuncTree(&nsstrs);
		if(!ftree)
		{
			error(cgi, fc, "No such namespace %s", nsstrs.back().c_str());
		}

		Resolved_t rs = cgi->resolveFunctionFromList(fc, ftree->funcs, fc->name, fc->params);

		if(!rs.resolved)
		{
			error(cgi, fc, "No such function %s in namespace %s", fc->name.c_str(), nsstrs.back().c_str());
		}

		// done.
		if(actual)
		{
			fc->cachedResolveTarget = rs;
			prevRes = fc->codegen(cgi);
			fc->cachedResolveTarget.resolved = false;	// clear it.
		}
		else
		{
			prevRes = Result_t(llvm::Constant::getNullValue(cgi->getLlvmType(fc, rs)), 0);
		}

		return doRecursiveNSResolution(cgi, flat, mas, actual, nsstrs, prevRes, false);
	}
	else
	{
		error(cgi, fr, "Unknown shit");
	}
}

Result_t doNamespaceAccess(CodegenInstance* cgi, MemberAccess* ma, std::deque<Expr*> flat, llvm::Value* rhs, bool actual)
{
	iceAssert(flat.size() > 0);
	printf("** base MA: %s\n", cgi->printAst(ma).c_str());

	// flattened -- create the opposite rotation of MA. basically, for a.b.c.d, the parser
	// returns (((a.b).c).d), which is good for codegen.
	// instead of doing something like rotating the tree (and then having to rotate it back to
	// prevent breaking things), we keep going leftwards until we reach the left-most MemberAccess expr,
	// adding these to the front of a list.

	std::deque<MemberAccess*> mas;
	{
		MemberAccess* _ma = ma;
		MemberAccess* left = _ma;
		do
		{
			_ma = left;
			mas.push_front(left);
		}
		while((left = dynamic_cast<MemberAccess*>(_ma->left)));
	}

	return doRecursiveNSResolution(cgi, flat, mas, actual, std::deque<std::string>(), Result_t(0, 0), true);
}







Result_t checkForStaticAccess(CodegenInstance* cgi, MemberAccess* ma, llvm::Value* lhsPtr, llvm::Value* _rhs, bool actual)
{
	VarRef* _vr = 0;
	MemberAccess* _ma = ma;
	do
	{
		_vr = dynamic_cast<VarRef*>(_ma->left);
	}
	while((_ma = dynamic_cast<MemberAccess*>(_ma->left)));

	if(_vr)
	{
		// check for type function access
		TypePair_t* tp = 0;
		if((tp = cgi->getType(cgi->mangleWithNamespace(_vr->name, false))))
		{
			if(tp->second.second == TypeKind::Enum)
			{
				return enumerationAccessCodegen(cgi, ma->left, ma->right);
			}
			else if(tp->second.second == TypeKind::Struct)
			{
				return doStaticAccess(cgi, ma, lhsPtr, _rhs, actual);
			}
		}

		// todo: do something with this
		std::deque<NamespaceDecl*> nses = cgi->resolveNamespace(_vr->name);
		if(nses.size() > 0)
			return doNamespaceAccess(cgi, ma, cgi->flattenDotOperators(ma), _rhs, actual);
	}


	return Result_t(0, 0);
}
























































