// DotOperatorCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

using namespace Ast;
using namespace Codegen;





static Result_t doVariable(CodegenInstance* cgi, VarRef* var, fir::Value* ref, StructBase* str, int i);
static Result_t callComputedPropertyGetter(CodegenInstance* cgi, VarRef* var, ComputedProperty* cp, fir::Value* ref);


static mpark::variant<fir::Type*, Result_t> getStaticVariable(CodegenInstance* cgi, Expr* user, ClassDef* cls, std::string name, bool actual)
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

		if(actual)
			return Result_t(cgi->irb.CreateLoad(gv), gv);

		else
			return gv->getType()->getPointerElementType();
	}

	error(user, "Class '%s' has no such static member '%s'", cls->ident.name.c_str(), name.c_str());
}

// static Result_t doTupleAccess(CodegenInstance* cgi, fir::Value* selfPtr, Number* num)
// {
// 	iceAssert(selfPtr);
// 	iceAssert(num);

// 	fir::Type* type = selfPtr->getType()->getPointerElementType();
// 	iceAssert(type->isTupleType());

// 	// quite simple, just get the number (make sure it's a Ast::Number)
// 	// and do a structgep.

// 	if((size_t) num->ival >= type->toTupleType()->getElementCount())
// 		error(num, "Tuple does not have %d elements, only %zd (type '%s')", (int) num->ival + 1, type->toTupleType()->getElementCount(),
// 			type->str().c_str());

// 	fir::Value* gep = cgi->irb.CreateStructGEP(selfPtr, num->ival);
// 	return Result_t(cgi->irb.CreateLoad(gep), gep, selfPtr->isImmutable() ? ValueKind::RValue : ValueKind::LValue);
// }

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








// todo: this function is a little... dirty.
// lmao: every function is *very* dirty
static Result_t attemptDotOperatorOnBuiltinTypeOrFail(CodegenInstance* cgi, fir::Type* type, MemberAccess* ma, bool actual,
	fir::Value* val, fir::Value* ptr, fir::Type** resultType)
{
	if(type->isParameterPackType())
	{
		// lol, some magic.
		if(VarRef* vr = dynamic_cast<VarRef*>(ma->right))
		{
			if(vr->name != "length")
				error(ma, "Variadic arrays only have one member, 'length'. '%s' is invalid.", vr->name.c_str());

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
			error(ma, "Variadic arrays only have one member, 'length'. Invalid operator.");
		}
	}
	else if(type->isArrayType())
	{
		if(dynamic_cast<VarRef*>(ma->right) && dynamic_cast<VarRef*>(ma->right)->name == "length")
		{
			if(!actual)
			{
				*resultType = fir::Type::getInt64();
				return Result_t(0, 0);
			}

			return Result_t(fir::ConstantInt::getInt64(type->toArrayType()->getArraySize()), 0);
		}
		else
		{
			error(ma->right, "Unsupported dot-operator on array type '%s'", type->str().c_str());
		}
	}
	else if(type->isDynamicArrayType())
	{
		// lol, some magic.
		if(VarRef* vr = dynamic_cast<VarRef*>(ma->right))
		{
			if(vr->name == "length")
			{
				if(!actual)
				{
					*resultType = fir::Type::getInt64();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				return Result_t(cgi->irb.CreateGetDynamicArrayLength(ptr), 0);
			}
			else if(vr->name == "capacity")
			{
				if(!actual)
				{
					*resultType = fir::Type::getInt64();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				return Result_t(cgi->irb.CreateGetDynamicArrayCapacity(ptr), 0);
			}
			else if(vr->name == "pointer")
			{
				if(!actual)
				{
					*resultType = type->toDynamicArrayType()->getElementType()->getPointerTo();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				return Result_t(cgi->irb.CreateGetDynamicArrayData(ptr), 0);
			}
			else
			{
				error(ma->right, "Unknown property '%s' on dynamic array type ('%s')", vr->name.c_str(), type->str().c_str());
			}
		}
		else if(FuncCall* fc = dynamic_cast<FuncCall*>(ma->right))
		{
			if(fc->name == "append")
			{
				if(!actual)
				{
					*resultType = fir::Type::getVoid();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				if(fc->params.size() != 1)
					error(fc, "Expected exactly 1 parameter in appending to dynamic array, have %zu", fc->params.size());

				fir::Value* rval = 0; fir::Value* rptr = 0;
				std::tie(rval, rptr) = fc->params[0]->codegen(cgi);

				fir::DynamicArrayType* lt = type->toDynamicArrayType();
				fir::DynamicArrayType* rt = 0;

				if(rval->getType()->isDynamicArrayType()) rt = rval->getType()->toDynamicArrayType();

				if(rt)
				{
					if(lt->getElementType() != rt->getElementType())
					{
						error(fc->params[0], "Incompatible element types in call to append; cannot append array of element "
							"type '%s' to one of element type '%s'", rt->getElementType()->str().c_str(),
							lt->getElementType()->str().c_str());
					}

					// ok.
					iceAssert(rptr);

					fir::Function* apf = RuntimeFuncs::Array::getAppendFunction(cgi, lt);
					cgi->irb.CreateCall2(apf, ptr, rptr);
				}
				else if(!rt)
				{
					rval = cgi->autoCastType(lt->getElementType(), rval, rptr);

					if(rval->getType() == lt->getElementType())
					{
						fir::Function* apf = RuntimeFuncs::Array::getElementAppendFunction(cgi, lt);
						cgi->irb.CreateCall2(apf, ptr, rval);
					}
					else
					{
						error(fc->params[0], "Cannot append a value of type '%s' to an array of element type '%s'",
							rval->getType()->str().c_str(), lt->getElementType()->str().c_str());
					}
				}

				return Result_t(0, 0);
			}
			else if(fc->name == "clone")
			{
				if(!actual)
				{
					*resultType = type->toDynamicArrayType();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				if(fc->params.size() > 0)
					error(fc, "Array clone() expects exactly 0 parameters, have %zu", fc->params.size());

				fir::Function* clonef = RuntimeFuncs::Array::getCloneFunction(cgi, type->toDynamicArrayType());
				iceAssert(clonef);

				fir::Value* clone = cgi->irb.CreateCall1(clonef, ptr);
				return Result_t(clone, 0);
			}
			else if(fc->name == "clear")
			{
				if(!actual)
				{
					*resultType = fir::Type::getVoid();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				if(fc->params.size() > 0)
					error(fc, "Array clear() expects exactly 0 parameters, have %zu", fc->params.size());

				// set length to 0 -- that's it
				cgi->irb.CreateSetDynamicArrayLength(ptr, fir::ConstantInt::getInt64(0));

				return Result_t(0, 0);
			}
			else if(fc->name == "back")
			{
				if(!actual)
				{
					*resultType = type->toDynamicArrayType()->getElementType();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				if(fc->params.size() > 0)
					error(fc, "Array back() expects exactly 0 parameters, have %zu", fc->params.size());


				// get the data pointer
				fir::Value* data = cgi->irb.CreateGetDynamicArrayData(ptr);

				// sub 1 from the len
				fir::Value* len = cgi->irb.CreateGetDynamicArrayLength(ptr);

				// trigger an abort if length is 0
				fir::Function* rangef = RuntimeFuncs::Array::getBoundsCheckFunction(cgi);
				iceAssert(rangef);

				cgi->irb.CreateCall2(rangef, len, fir::ConstantInt::getInt64(0));

				// ok.
				fir::Value* ind = cgi->irb.CreateSub(len, fir::ConstantInt::getInt64(1));
				fir::Value* mem = cgi->irb.CreatePointerAdd(data, ind);

				if(ptr->isImmutable())
					mem->makeImmutable();

				return Result_t(cgi->irb.CreateLoad(mem), mem, ValueKind::LValue);
			}
			else if(fc->name == "popBack")
			{
				if(!actual)
				{
					*resultType = type->toDynamicArrayType()->getElementType();
					return Result_t(0, 0);
				}

				iceAssert(ptr);
				if(fc->params.size() > 0)
					error(fc, "Array back() expects exactly 0 parameters, have %zu", fc->params.size());

				// get the data pointer
				fir::Value* data = cgi->irb.CreateGetDynamicArrayData(ptr);

				// sub 1 from the len
				fir::Value* len = cgi->irb.CreateGetDynamicArrayLength(ptr);

				// trigger an abort if length is 0
				fir::Function* rangef = RuntimeFuncs::Array::getBoundsCheckFunction(cgi);
				iceAssert(rangef);

				cgi->irb.CreateCall2(rangef, len, fir::ConstantInt::getInt64(0));

				// ok.
				fir::Value* ind = cgi->irb.CreateSub(len, fir::ConstantInt::getInt64(1));
				fir::Value* mem = cgi->irb.CreatePointerAdd(data, ind);

				fir::Value* ret = cgi->irb.CreateLoad(mem);

				// shrink the length
				cgi->irb.CreateSetDynamicArrayLength(ptr, ind);

				return Result_t(ret, 0);
			}
			else
			{
				error(ma->right, "Unknown method '%s' on dynamic array type ('%s')", fc->name.c_str(), type->str().c_str());
			}
		}
		else
		{
			error(ma->right, "Unknown operator on dynamic array (type '%s')", type->str().c_str());
		}
	}
	else if((type->isStringType() || type == fir::Type::getStringType()->getPointerTo()) && dynamic_cast<VarRef*>(ma->right))
	{
		// handle builtin ones: 'raw' and 'length'

		if(type->isPointerType() && actual)
			val = cgi->irb.CreateLoad(val);

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
				iceAssert(val);
				return Result_t(cgi->irb.CreateGetStringData(val), 0);
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
				iceAssert(val);
				return Result_t(cgi->irb.CreateGetStringLength(val), 0);
			}
		}
		else if(vr->name == "rc")
		{
			if(!actual)
			{
				*resultType = fir::Type::getInt64();
				return Result_t(0, 0);
			}
			else
			{
				iceAssert(val);
				return Result_t(cgi->irb.CreateGetStringRefCount(val), 0);
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
			error(ma->right, "Unsupported builtin type '%s' for member access", type->str().c_str());
		}
	}
	else
	{
		error(ma, "Cannot do member access on aggregate type '%s'", type->str().c_str());
	}
}

















// new plan: instead of doing a pre-codegen pass where we 'discover' (buggily) the type of the dot operator (static or not),
// we can do it ad-hoc, kind of. getType() will recursively attempt to get the type of the leftmost dot operator.
/*
	note(?): based on my crude understanding of the 'code' in this file, here's how the dot operator resolution works:
	A. the dot operator classification system basically finds the rightmost static occurrence in the chain, and in the process
		marks that dot-op, as well as all those to its left, as 'LeftStatic'.

	B. due to the left-associativity of the dot-op, we get (((a.b).c).d).e
		hence, say (a.b).c was determined to be the rightmost static access -- ie. A.B is a namespace/type, and C is some kind of instance
		member or something.

		then, since the whole thing is left-recursive, at the top-level, .e calls .d recursively. then, .d calls .c recursively. this
		causes the appropriate resolution to give the type/value of .c, which is returned to .d. After that, normal (non-static)
		dot-op resolution continues -- since ).d has type 'LeftVariable'.

	C. the problem lies in the fact that, without knowing the *exact* position of the leftmost static access, we cannot call
		resolveStaticDotOperator, because its rightmost access must resolve to a value -- eg. NS1.NS2.NS3.Type1 will error, saying that
		there's no such variable Type1 in NS3 -- because Type1 is a type, not a value.

		possible solution is tryResolveStaticDotOperator, but that quickly becomes messy...
		and, we would need to repeatedly call it every time we increase the chain length...



	The following change is proposed:

	using ((((a.b).c).d).e).f as an example, with A, B, C, and D being static names:


	1. create a new FIR Type, "NamespaceType". it would replace the current FunctionTree system we have now, probably.
	2. in getType(), just return the appropriate type. A.B returns a NamespaceType of A.B.
		a. .C sees that return value, and checks its rhs and does the approriate action -- in this case C is a static name,
			so it looks in the namespace B for either a type, or another namespace, in B.
		b. assuming it is found, it returns the appropriate thing. Let's say C is a type -- it'd return either ClassType,
			StructType, or EnumType, depending.
		c. D is also a static name, say it's a static field inside C. resolution looks through static fields, nested types and function names
			to resolve D, and returns the appropriate type.
		d. E is a non-static name -- let's say E is a function call, and D was a class type. Resolution looks through the functions
			in D (limiting itself to non-static methods), and returns the appropriate (return) type.
		e. F is has the same resolution path as E, essentially. recursion ends, and the correct type is returned.

	3. in codegen(), instead of unrolling the entire operator chain into a list, we just recurse leftwards again. This time, we stop
		recursing when encountering the first static dot-op.
		- as an optimisation, we can set the matype field to LeftStatic during getType().

		a. Since we know that everything to the left of a static dot-op must also be static, and if it's the rightmost static op then
			everything to its left is non-static, we can call resolveStaticDotOperator() on this rightmost op. In this case,
			it's (...).E -- E is the first non-static incantation, so the thing is called on that.

		b. further codegen happens normally (recursively).



			auto result = mpark::get<Result_t>(cgi->tryResolveVarRef(vr, extra, true));
			iceAssert(result.pointer);
*/


using variant = mpark::variant<fir::Type*, FunctionTree*, TypePair_t, Result_t>;
static variant resolveLeftNonStaticMA(CodegenInstance* cgi, MemberAccess* ma, fir::Type* lhs, Result_t result,
	fir::Value* extra, bool actual)
{
	// at this stage, we can stick to checking for instance things
	// since the leftmost (first) expression is a variable, there can be no more static things afterwards,
	// so we don't need to return the FuncTree or StructBase in this branch.

	TypePair_t* pair = cgi->getType(lhs->isPointerType() ? lhs->getPointerElementType() : lhs);
	if(lhs->isTupleType())
	{
		fir::TupleType* tt = lhs->toTupleType();
		iceAssert(tt);

		Number* n = dynamic_cast<Number*>(ma->right);
		if(!n || n->decimal)
			error(ma->right, "Expected integer number after dot-operator for tuple access");

		if((size_t) n->ival >= tt->getElementCount())
		{
			error(ma, "Tuple does not have %d elements, only %zd (type '%s')", (int) n->ival + 1,
				tt->getElementCount(), tt->str().c_str());
		}

		if(actual)
		{
			if(!result.pointer)
				result.pointer = cgi->irb.CreateImmutStackAlloc(lhs, result.value);

			iceAssert(result.pointer);
			fir::Value* vp = cgi->irb.CreateStructGEP(result.pointer, n->ival);

			return Result_t(cgi->irb.CreateLoad(vp), vp);
		}
		else
		{
			return tt->getElementN(n->ival);
		}
	}
	else if(!pair && (!lhs->isStructType() && !lhs->isClassType() && !lhs->isTupleType()))
	{
		fir::Type* ret = 0;

		fir::Value* val = 0;
		fir::Value* ptr = 0;

		if(actual)
			std::tie(val, ptr) = result;

		auto result = attemptDotOperatorOnBuiltinTypeOrFail(cgi, lhs, ma, actual, val, ptr, &ret);

		if(actual)	return result;
		else		return ret;
	}
	else if(pair->second.second == TypeKind::Class || pair->second.second == TypeKind::Struct)
	{
		StructBase* sb = dynamic_cast<StructBase*>(pair->second.first);
		iceAssert(sb);

		ClassDef* maybeCls = dynamic_cast<ClassDef*>(sb);

		VarRef* memberVr = dynamic_cast<VarRef*>(ma->right);
		FuncCall* memberFc = dynamic_cast<FuncCall*>(ma->right);

		bool isPtr = false;
		if(actual)
		{
			if(result.value->getType()->isPointerType())
				isPtr = true;

			if(!isPtr && !result.pointer)
				result.pointer = cgi->irb.CreateImmutStackAlloc(result.value->getType(), result.value);

			iceAssert(result.pointer || (isPtr && result.value));
		}

		if(memberVr)
		{
			for(VarDecl* mem : sb->members)
			{
				if(mem->ident.name == memberVr->name && !mem->isStatic)
				{
					if(actual)
					{
						auto p = cgi->irb.CreateGetStructMember(isPtr ? result.value : result.pointer, mem->ident.name);
						return Result_t(cgi->irb.CreateLoad(p), p);
					}
					else
					{
						return mem->getType(cgi);
					}
				}
			}

			if(maybeCls)
			{
				for(ComputedProperty* c : maybeCls->cprops)
				{
					if(c->ident.name == memberVr->name)
					{
						if(actual)
							return callComputedPropertyGetter(cgi, memberVr, c, isPtr ? result.value : result.pointer);

						else
							return c->getType(cgi);
					}
				}
			}

			auto exts = cgi->getExtensionsForType(sb);
			for(auto ext : exts)
			{
				for(auto cp : ext->cprops)
				{
					if(cp->attribs & Attr_VisPublic || ext->parentRoot == cgi->rootNode)
					{
						if(cp->ident.name == memberVr->name)
						{
							if(actual)
								return callComputedPropertyGetter(cgi, memberVr, cp, isPtr ? result.value : result.pointer);

							else
								return cp->getType(cgi);
						}
					}
				}
			}

			if(maybeCls)
			{
				auto ret = cgi->tryGetMemberFunctionOfClass(maybeCls, memberVr, memberVr->name, extra);
				if(!ret.isEmpty())
				{
					iceAssert(ret.firFunc);

					if(actual)
						return Result_t(ret.firFunc, 0);

					else
						return ret.firFunc->getType();
				}
			}

			error(memberVr, "Type '%s' has no member named '%s'", sb->ident.name.c_str(), memberVr->name.c_str());
		}
		else if(memberFc)
		{
			if(ClassDef* maybeCls = dynamic_cast<ClassDef*>(sb))
			{
				if(actual)
				{
					auto r = callMemberFunction(cgi, ma, maybeCls, memberFc, isPtr ? result.value : result.pointer);
					return Result_t(std::get<3>(r), 0);
				}
				else
				{
					return std::get<2>(callMemberFunction(cgi, ma, maybeCls, memberFc, 0));
				}
			}
			else
			{
				error(ma->right, "Cannot call methods on structs, since they do not have any");
			}
		}
		else
		{
			error(ma->right, "Invalid expression type for dot-operator access");
		}
	}
	else
	{
		error(ma->left, "Invalid expression type for dot-operator access (on type '%s')", lhs->str().c_str());
	}
}

static variant resolveLeftNamespaceMA(CodegenInstance* cgi, MemberAccess* ma, FunctionTree* ftree, fir::Value* extra, bool actual)
{
	// we're a namespace here.
	// check what the right side is
	if(auto vr = dynamic_cast<VarRef*>(ma->right))
	{
		// check vars first
		{
			auto it = ftree->vars.find(vr->name);
			if(it != ftree->vars.end())
			{
				// bingo

				if(actual)
					return Result_t(cgi->irb.CreateLoad((*it).second.first), (*it).second.first, ValueKind::LValue);

				else
					return (*it).second.first->getType()->getPointerElementType();
			}
		}

		// ok, try functions
		{
			std::deque<fir::Function*> fns;

			for(auto f : ftree->funcs)
			{
				if(f.funcDecl->ident.name == vr->name && f.funcDecl->genericTypes.size() == 0)
				{
					if(!f.firFunc)
						f.funcDecl->codegen(cgi);

					fns.push_back(f.firFunc);
				}
			}

			for(auto gf : ftree->genericFunctions)
			{
				if(gf.first->ident.name == vr->name)
				{
					if(!gf.first->generatedFunc)
						gf.first->codegen(cgi);

					fir::Function* fn = gf.first->generatedFunc;
					iceAssert(fn);

					fns.push_back(fn);
				}
			}

			auto fn = cgi->tryDisambiguateFunctionVariableUsingType(vr, vr->name, fns, extra);
			if(fn)
			{
				if(actual)
					return Result_t(fn, 0);

				else
					return fn->getType();
			}
		}

		// ok, try namespaces
		{
			auto sub = ftree->subMap[vr->name];
			if(sub) return sub;
		}

		// ok, try types
		{
			if(ftree->types.find(vr->name) == ftree->types.end())
				error(ma->right, "No entity named '%s' in namespace '%s'", vr->name.c_str(), ftree->nsName.c_str());

			auto ret = ftree->types[vr->name].second.first;
			if(dynamic_cast<StructBase*>(ret))
				return ftree->types[vr->name];

			else
				error(ma->right, "'%s' is some kind of invalid type? ('%s')", vr->name.c_str(), ftree->types[vr->name].first->str().c_str());
		}
	}
	else if(auto fc = dynamic_cast<FuncCall*>(ma->right))
	{
		// check functions.

		std::map<Func*, std::pair<std::string, Expr*>> errs;
		auto res = cgi->resolveFunctionFromList(ma, ftree->funcs, fc->name, fc->params);
		if(!res.resolved)
		{
			std::deque<Func*> flist;
			for(auto f : ftree->genericFunctions)
			{
				iceAssert(f.first->genericTypes.size() > 0);

				if(f.first->ident.name == fc->name)
					flist.push_back({ f.second });
			}

			FuncDefPair fp = cgi->tryResolveGenericFunctionCallUsingCandidates(fc, flist, &errs);
			if(!fp.isEmpty()) res = Resolved_t(fp);
		}

		// try variables
		if(!res.resolved)
		{
			for(auto v : ftree->vars)
			{
				auto var = v.second.second;
				if(v.first == fc->name && var->concretisedType && var->concretisedType->isFunctionType())
				{
					if(var->concretisedType->toFunctionType()->isGenericFunction())
						error(fc, "this is impossible");

					if(actual)
					{
						// get the thing
						iceAssert(v.second.first);
						fir::Value* fnptr = cgi->irb.CreateLoad(v.second.first);
						iceAssert(fnptr->getType()->isFunctionType());

						// call it.
						fir::FunctionType* ft = fnptr->getType()->toFunctionType();
						auto args = cgi->checkAndCodegenFunctionCallParameters(fc, ft, fc->params,
							ft->isVariadicFunc(), ft->isCStyleVarArg());

						// call it.
						return Result_t(cgi->irb.CreateCallToFunctionPointer(fnptr, ft, args), 0);
					}
					else
					{
						// get the type
						fir::Type* t = v.second.first->getType();
						iceAssert(t->isPointerType());

						t = t->getPointerElementType();
						iceAssert(t->isFunctionType());

						return t->toFunctionType()->getReturnType();
					}
				}
			}
		}

		if(!res.resolved)
		{
			// try types initialisers
			// again, we can call gettypebystring without (too much) scope messiness
			if(auto pair = cgi->getType(Identifier(fc->name, cgi->getNSFromFuncTree(ftree), IdKind::Name)))
			{
				fir::Type* ltype = pair->first;
				iceAssert(ltype);

				if(actual)
				{
					std::vector<fir::Value*> args;
					for(Expr* e : fc->params)
						args.push_back(e->codegen(cgi).value);

					return cgi->callTypeInitialiser(pair, ma, args);
				}
				else
				{
					return ltype;
				}
			}
			else
			{
				if(errs.size() > 0)
					GenError::prettyNoSuchFunctionError(cgi, fc, fc->name, fc->params, errs);

				else
					GenError::noFunctionTakingParams(cgi, fc, "namespace " + ftree->nsName, fc->name, fc->params);
			}
		}


		if(!res.resolved)
			error(ma->right, "No function named '%s' in namespace '%s'", fc->name.c_str(), ftree->nsName.c_str());

		iceAssert(res.t.firFunc);

		if(actual)
		{
			// call that shit
			return fc->codegen(cgi, res.t.firFunc);
		}
		else
		{
			return res.t.firFunc->getReturnType();
		}
	}
	else
	{
		error(ma->right, "Invalid expression on namespace");
	}
}


static variant resolveLeftTypenameMA(CodegenInstance* cgi, MemberAccess* ma, TypePair_t pair, fir::Value* extra, bool actual)
{
	if(dynamic_cast<StructDef*>(pair.second.first) || dynamic_cast<ClassDef*>(pair.second.first))
	{
		auto base = dynamic_cast<StructBase*>(pair.second.first);
		auto maybecls = dynamic_cast<ClassDef*>(pair.second.first);

		if(auto vr = dynamic_cast<VarRef*>(ma->right))
		{
			// try members first -- structs don't have static members btw
			if(maybecls)
			{
				for(auto m : base->members)
				{
					if(m->isStatic && m->ident.name == vr->name)
					{
						auto ret = getStaticVariable(cgi, vr, maybecls, vr->name, actual);

						// sadly the variant does not auto unwrap
						if(actual)
							return mpark::get<Result_t>(ret);

						else
							return mpark::get<fir::Type*>(ret);
					}
				}
			}

			// however, structs can have static cprop extensions as well.
			auto exts = cgi->getExtensionsForType(base);
			for(auto ext : exts)
			{
				for(auto cp : ext->cprops)
				{
					if(cp->attribs & Attr_VisPublic || ext->parentRoot == cgi->rootNode)
					{
						if(cp->ident.name == vr->name && cp->isStatic)
						{
							if(actual)
								return callComputedPropertyGetter(cgi, vr, cp, 0);

							else
								return cp->getType(cgi);
						}
					}
				}
			}

			auto ret = cgi->tryGetMemberFunctionOfClass(maybecls, vr, vr->name, extra);
			{
				if(!ret.isEmpty())
				{
					iceAssert(ret.firFunc);

					if(actual)	return Result_t(ret.firFunc, 0);
					else		return ret.firFunc->getType();
				}


				// ok, try nested classes.
				// in the above, A.B does not need to resolve to value -- B can be a nested class, in which case we
				// return the structbase* associated with it.

				for(auto n : base->nestedTypes)
				{
					if(n.first->ident.name == vr->name)
					{
						// regardless of `actual` or not.
						if(dynamic_cast<StructDef*>(n.first))
						{
							return TypePair_t(n.second, { n.first, TypeKind::Struct });
						}
						else if(dynamic_cast<ClassDef*>(n.first))
						{
							return TypePair_t(n.second, { n.first, TypeKind::Class });
						}
						else if(dynamic_cast<EnumDef*>(n.first))
						{
							return TypePair_t(n.second, { n.first, TypeKind::Enum });
						}
						else
						{
							error(n.first, "what??");
						}
					}
				}
			}

				// ok, there's nothing else here.
			error(vr, "Entity '%s' (function, type, field, or property) does not exist in type '%s'", vr->name.c_str(),
				base->ident.name.c_str());
		}
		else if(auto fc = dynamic_cast<FuncCall*>(ma->right))
		{
			// ok, look for static functions and stuff
			// christ almighty

			if(ClassDef* clsd = dynamic_cast<ClassDef*>(base))
			{
				iceAssert(clsd->funcs.size() == clsd->lfuncs.size());

				std::deque<FuncDefPair> flist;
				for(size_t i = 0; i < clsd->funcs.size(); i++)
				{
					if(clsd->funcs[i]->decl->ident.name == fc->name && clsd->funcs[i]->decl->isStatic)
						flist.push_back(FuncDefPair(clsd->lfuncs[i], clsd->funcs[i]->decl, clsd->funcs[i]));
				}

				for(auto e : cgi->getExtensionsForType(clsd))
				{
					for(size_t i = 0; i < clsd->funcs.size(); i++)
					{
						if(e->funcs[i]->decl->ident.name == fc->name && e->funcs[i]->decl->isStatic)
							flist.push_back(FuncDefPair(e->lfuncs[i], e->funcs[i]->decl, e->funcs[i]));
					}
				}

				std::map<Func*, std::pair<std::string, Expr*>> errs;
				auto res = cgi->resolveFunctionFromList(ma, flist, fc->name, fc->params);

				if(!res.resolved)
				{
					std::deque<Func*> flist;
					for(auto f : clsd->funcs)
					{
						if(f->decl->ident.name == fc->name && f->decl->genericTypes.size() > 0)
							flist.push_back(f);
					}

					FuncDefPair fp = cgi->tryResolveGenericFunctionCallUsingCandidates(fc, flist, &errs);
					if(!fp.isEmpty()) res = Resolved_t(fp);
				}


				if(!res.resolved)
				{
					// look in nested types for type inits
					for(auto nest : clsd->nestedTypes)
					{
						if(nest.first->ident.name == fc->name)
						{
							fir::Type* ltype = nest.second;
							iceAssert(ltype);

							if(actual)
							{
								std::vector<fir::Value*> args;
								for(Expr* e : fc->params)
									args.push_back(e->codegen(cgi).value);

								return cgi->callTypeInitialiser(&pair, ma, args);
							}
							else
							{
								return ltype;
							}
						}
					}

					if(errs.size() > 0)
						GenError::prettyNoSuchFunctionError(cgi, fc, fc->name, fc->params, errs);

					else
						GenError::noFunctionTakingParams(cgi, fc, "type " + base->ident.name, fc->name, fc->params);
				}



				// call that shit
				iceAssert(res.t.firFunc);

				if(actual)
				{
					return fc->codegen(cgi, res.t.firFunc);
				}
				else
				{
					return res.t.firFunc->getReturnType();
				}
			}
			else if(dynamic_cast<StructDef*>(base))
			{
				error(fc, "Cannot call methods on structs, since they do not have any");
			}
			else
			{
				error("what??");
			}
		}
		else
		{
			error(ma->right, "Invalid expression in rhs of dot operator on typename");
		}
	}
	else if(auto enr = dynamic_cast<EnumDef*>(pair.second.first))
	{
		// ok.
		if(auto vr = dynamic_cast<VarRef*>(ma->right))
		{
			if(enr->createdType == 0)
				enr->createType(cgi);

			iceAssert(enr->createdType);
			fir::EnumType* ety = enr->createdType->toEnumType();

			if(!ety->hasCaseWithName(vr->name))
				error(vr, "Enum '%s' has no such case named '%s'", ety->getEnumName().name.c_str(), vr->name.c_str());

			if(actual)
			{
				fir::Value* val = ety->getCaseWithName(vr->name);
				iceAssert(val);

				return Result_t(cgi->irb.CreateBitcast(val, ety), 0);
			}
			else
			{
				// return ety->getCaseType();
				return ety;
			}
		}
		else
		{
			error(ma->right, "Invalid expression on right side of dot operator with enumeration on left; expected identifier");
		}
	}
	else
	{
		error(ma->left, "What? invalid type ('%s')", pair.first->str().c_str());
	}
}












// using variant = mpark::variant<fir::Type*, FunctionTree*, TypePair_t, Result_t>;
variant resolveTypeOfMA(CodegenInstance* cgi, MemberAccess* ma, fir::Value* extra, bool actual)
{
	if(auto l = dynamic_cast<MemberAccess*>(ma->left))
	{
		variant left = resolveTypeOfMA(cgi, l, extra, actual);

		// get what kind of shit we are.
		if(left.index() == 0 || left.index() == 3)
		{
			// type / value
			fir::Type* type = 0;

			if(actual)
			{
				iceAssert(left.index() == 3);
				type = mpark::get<Result_t>(left).value->getType();
			}
			else
			{
				iceAssert(left.index() == 0);
				type = mpark::get<fir::Type*>(left);
			}

			iceAssert(type);

			// thankfully this has been pulled out into a function on its own.
			auto res = (actual ? mpark::get<Result_t>(left) : Result_t(0, 0));
			return resolveLeftNonStaticMA(cgi, ma, type, res, extra, actual);
		}
		else if(left.index() == 1)
		{
			// ok, namespace
			FunctionTree* ftree = mpark::get<FunctionTree*>(left);
			return resolveLeftNamespaceMA(cgi, ma, ftree, extra, actual);
		}
		else if(left.index() == 2)
		{
			TypePair_t pair = mpark::get<TypePair_t>(left);
			return resolveLeftTypenameMA(cgi, ma, pair, extra, actual);
		}
		else
		{
			iceAssert(false && "¿¿¿ index out of bounds ???");
		}
	}
	else if(auto v = dynamic_cast<VarRef*>(ma->left))
	{
		// this branch should only be entered at the root level (ie. A.B)
		// so we just see if there's such a namespace.

		// first check if there's such a variable
		auto var = cgi->tryResolveVarRef(v, extra, false);
		if(auto lhs = mpark::get<fir::Type*>(var))
		{
			auto res = (actual ? mpark::get<Result_t>(cgi->tryResolveVarRef(v, extra, true)) : Result_t(0, 0));
			return resolveLeftNonStaticMA(cgi, ma, lhs, res, extra, actual);
		}
		else
		{
			// ok, it's not a variable.

			// it's static.
			// -- note -- NS is just v->name, since, again, this is only called in the leftmost mode.
			// once we get `using namespaces` we'll have to modify this a bit, i think.
			auto ft = cgi->getFuncTreeFromNS({ v->name });

			// not a namespace
			if(ft == 0)
			{
				// try a type
				// again, this path is only taken at the root (deepest) level, so we can just do a getTypeByString() without any
				// scoping nonsense
				if(auto pair = cgi->getTypeByString(v->name))
				{
					return resolveLeftTypenameMA(cgi, ma, *pair, extra, actual);
				}
				else
				{
					error(ma->left, "Entity (namespace or type) '%s' does not exist", v->name.c_str());
				}
			}
			else
			{
				return resolveLeftNamespaceMA(cgi, ma, ft, extra, actual);
			}
		}
	}
	else if(auto fc = dynamic_cast<FuncCall*>(ma->left))
	{
		// first... resolve the function
		fir::Type* type = 0;
		auto result = Result_t(0, 0);


		if((type = cgi->getExprTypeOfBuiltin(fc->name)))
		{
			// ok, we can call this straightaway -- funccall::codegen() knows not to do stupid things when encountering
			// a builtin type name.
			// it'll just treat it as initialiser syntax.
			result = fc->codegen(cgi);

			// just return early.
			// easier than if-ing all the code below.
			return resolveLeftNonStaticMA(cgi, ma, type, result, extra, actual);
		}


		auto res = cgi->resolveFunction(ma, fc->name, fc->params);
		if(!res.resolved)
		{
			std::map<Func*, std::pair<std::string, Expr*>> errs;
			FuncDefPair fp = cgi->tryResolveGenericFunctionCall(fc, &errs);
			if(!fp.isEmpty()) res = Resolved_t(fp);
		}

		if(res.resolved)
		{
			iceAssert(res.t.firFunc);
			type = res.t.firFunc->getReturnType();

			// call that shit
			result = fc->codegen(cgi, res.t.firFunc);
		}


		// check type inits
		if(!res.resolved)
		{
			if(auto pair = cgi->getType(Identifier(fc->name, IdKind::Name)))
			{
				fir::Type* type = pair->first;
				iceAssert(type);

				if(actual)
				{
					std::vector<fir::Value*> args;
					for(Expr* e : fc->params)
						args.push_back(e->codegen(cgi).value);

					result =  cgi->callTypeInitialiser(pair, ma, args);
				}
			}
		}

		// check variables.
		if(!res.resolved)
		{
			if(fir::Value* var = cgi->getSymInst(fc, fc->name))
			{
				if(var->getType()->isFunctionType())
				{
					if(var->getType()->toFunctionType()->isGenericFunction())
						error(fc, "this is impossible");


					iceAssert(var->getType()->isPointerType());
					iceAssert(var->getType()->getPointerElementType()->isFunctionType());

					type = var->getType()->getPointerElementType()->toFunctionType();

					if(actual)
					{
						// get the thing
						fir::Value* fnptr = cgi->irb.CreateLoad(var);
						iceAssert(fnptr->getType()->isFunctionType());

						// call it.
						fir::FunctionType* ft = fnptr->getType()->toFunctionType();
						auto args = cgi->checkAndCodegenFunctionCallParameters(fc, ft, fc->params,
							ft->isVariadicFunc(), ft->isCStyleVarArg());

						// call it.
						result = Result_t(cgi->irb.CreateCallToFunctionPointer(fnptr, ft, args), 0);
					}
				}
			}
		}



		if(!res.resolved)
		{
			// die
			GenError::prettyNoSuchFunctionError(cgi, fc, fc->name, fc->params);
		}

		iceAssert(type);
		return resolveLeftNonStaticMA(cgi, ma, type, result, extra, actual);
	}
	else
	{
		// ok, fuck this -- just get the type on the left.
		// chances are it's some kind of literal expression

		fir::Type* ltype = ma->left->getType(cgi);

		if(ltype->isTupleType())
		{
			// values are 1, 2, 3 etc.
			// for now, assert this.

			fir::TupleType* tt = ltype->toTupleType();
			iceAssert(tt);

			Number* n = dynamic_cast<Number*>(ma->right);
			if(!n || n->decimal)
			{
				error(ma->right, "Expected integer number after dot-operator for tuple access");
			}


			if((size_t) n->ival >= tt->getElementCount())
			{
				error(ma, "Tuple does not have %d elements, only %zd (type '%s')", (int) n->ival + 1, tt->getElementCount(), tt->str().c_str());
			}


			if(actual)
			{
				auto result = ma->left->codegen(cgi);
				if(!result.pointer)
					result.pointer = cgi->irb.CreateImmutStackAlloc(ltype, result.value);

				iceAssert(result.pointer);
				fir::Value* vp = cgi->irb.CreateStructGEP(result.pointer, n->ival);

				return Result_t(cgi->irb.CreateLoad(vp), vp);
			}
			else
			{
				return tt->getElementN(n->ival);
			}
		}
		else if(!ltype->isStructType() && !ltype->isClassType() && !ltype->isTupleType())
		{
			fir::Type* ret = 0;

			fir::Value* val = 0;
			fir::Value* ptr = 0;

			if(actual)
				std::tie(val, ptr) = ma->left->codegen(cgi);

			auto result = attemptDotOperatorOnBuiltinTypeOrFail(cgi, ltype, ma, actual, val, ptr, &ret);

			if(actual)	return result;
			else		return ret;
		}

		error(ma->left, "Invalid left-hand expression for dot-operator (type '%s')", ltype->str().c_str());
	}
}
























fir::Type* MemberAccess::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	#if 1

	auto ret = resolveTypeOfMA(cgi, this, extra, false);
	iceAssert(ret.index() == 0);

	return mpark::get<fir::Type*>(ret);

	#else
	if(this->matype == MAType::LeftStatic)
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
		if(!n || n->decimal)
		{
			error(this->right, "Expected integer number after dot-operator for tuple access");
		}


		if((size_t) n->ival >= tt->getElementCount())
		{
			error(this, "Tuple does not have %d elements, only %zd (type '%s')", (int) n->ival + 1, tt->getElementCount(), tt->str().c_str());
		}

		return tt->getElementN(n->ival);
	}
	else if(!pair && (!lhs->isStructType() && !lhs->isClassType() && !lhs->isTupleType()))
	{
		fir::Type* ret = 0;
		attemptDotOperatorOnBuiltinTypeOrFail(cgi, lhs, this, false, 0, 0, &ret);

		return ret;
	}
	else if(pair->second.second == TypeKind::Class || pair->second.second == TypeKind::Struct)
	{
		StructBase* sb = dynamic_cast<StructBase*>(pair->second.first);
		iceAssert(sb);

		ClassDef* maybeCls = dynamic_cast<ClassDef*>(sb);


		VarRef* memberVr = dynamic_cast<VarRef*>(this->right);
		FuncCall* memberFc = dynamic_cast<FuncCall*>(this->right);

		if(memberVr)
		{
			for(VarDecl* mem : sb->members)
			{
				if(mem->ident.name == memberVr->name)
					return mem->getType(cgi);
			}

			if(maybeCls)
			{
				for(ComputedProperty* c : maybeCls->cprops)
				{
					if(c->ident.name == memberVr->name)
						return c->getType(cgi);
				}
			}

			auto exts = cgi->getExtensionsForType(sb);
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

			if(maybeCls)
			{
				auto ret = cgi->tryGetMemberFunctionOfClass(maybeCls, memberVr, memberVr->name, extra);
				if(!ret.isEmpty()) return ret.firFunc->getType();
			}

			error(memberVr, "Type '%s' has no member named '%s'", sb->ident.name.c_str(), memberVr->name.c_str());
		}
		else if(memberFc)
		{
			if(ClassDef* maybeCls = dynamic_cast<ClassDef*>(sb))
			{
				return std::get<2>(callMemberFunction(cgi, this, maybeCls, memberFc, 0));
			}
			else
			{
				error(this->right, "Cannot call methods on structs, since they do not have any");
			}
		}
		else
		{
			error(this->right, "Invalid expression type for dot-operator access");
		}
	}
	else
	{
		error(this->left, "Invalid expression type for dot-operator access (on type '%s')", lhs->str().c_str());
	}
	#endif
}





































Result_t MemberAccess::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	#if 1

	auto ret = resolveTypeOfMA(cgi, this, extra, true);
	if(ret.index() != 3)
	{

	}

	iceAssert(ret.index() == 3);

	return mpark::get<Result_t>(ret);

	#else
	if(this->matype != MAType::LeftVariable && this->matype != MAType::LeftFunctionCall)
	{
		if(this->matype == MAType::Invalid) error(this, "invalid ma type??");
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

	fir::Value* self = 0; fir::Value* selfptr = 0;
	std::tie(self, selfptr) = res;


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
			return attemptDotOperatorOnBuiltinTypeOrFail(cgi, ftype, this, true, self, selfptr, &_);
		}
	}


	// find out whether we need self or selfptr.
	if(selfptr == nullptr && !isPtr)
	{
		selfptr = cgi->getStackAlloc(ftype);
		cgi->irb.CreateStore(self, selfptr);
	}


	// handle type aliases
	if(isWrapped)
	{
		bool wasSelfPtr = false;

		if(selfptr)
		{
			wasSelfPtr = true;
			isPtr = false;
		}


		// if we're faced with a double pointer, we need to load it once
		if(wasSelfPtr)
		{
			if(selfptr->getType()->isPointerType() && selfptr->getType()->getPointerElementType()->isPointerType())
				selfptr = cgi->irb.CreateLoad(selfptr);
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
		if(!n) error(this->right, "Expected integer number after dot-operator for tuple access");


		// if the lhs is immutable, don't give a pointer.
		// todo: fix immutability (actually across the entire compiler)
		return doTupleAccess(cgi, selfptr, n);
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
				return doVariable(cgi, var, isPtr ? self : selfptr, str, i);
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
				error(rhs, "Unsupported operation on RHS of dot operator");
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
				return doVariable(cgi, var, isPtr ? self : selfptr, cls, i);
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
					return callComputedPropertyGetter(cgi, var, cprop, isPtr ? self : selfptr);
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
			auto result = callMemberFunction(cgi, this, cls, fc, isPtr ? self : selfptr);
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
				error(rhs, "Unsupported operation on RHS of dot operator");
			}
		}
	}
	else if(pair->second.second == TypeKind::Enum)
	{
		iceAssert(0 && "what? no.");
		// return cgi->getEnumerationCaseValue(this->left, this->right);
	}

	iceAssert(!"Encountered invalid expression");
	#endif
}







static Result_t callComputedPropertyGetter(CodegenInstance* cgi, VarRef* var, ComputedProperty* cprop, fir::Value* ref)
{
	fir::Function* lcallee = cprop->getterFFn;
	iceAssert(lcallee);

	lcallee = cgi->module->getFunction(lcallee->getName());

	if(cprop->isStatic)
		return Result_t(cgi->irb.CreateCall0(lcallee), 0);

	else
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










static std::tuple<FunctionTree*, std::deque<std::string>, std::deque<std::string>, StructBase*, fir::Type*>
unwrapStaticDotOperator(CodegenInstance* cgi, MemberAccess* ma)
{
	iceAssert(ma->matype == MAType::LeftStatic);

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
	// FunctionTree* ftree = cgi->getCurrentFuncTree(&nsstrs);
	auto ftree = cgi->getFuncTreeFromNS(nsstrs);

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
				// ftree = cgi->getCurrentFuncTree(&nsstrs);

				ftree = cgi->getFuncTreeFromNS(nsstrs);
				iceAssert(ftree);

				found = true;
			}


			if(found) continue;

			if(TypePair_t* tp = cgi->getType(Identifier(front, nsstrs, IdKind::Struct)))
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
			cgi->pushNestedTypeScope(curType);
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
			cgi->popNestedTypeScope();

			if(found) continue;
		}

		std::string lscope = ma->matype == MAType::LeftStatic ? "namespace" : "type";
		error(ma, "No such member %s in %s %s", front.c_str(), lscope.c_str(),
			lscope == "namespace" ? ftree->nsName.c_str() : (curType ? curType->ident.name.c_str() : "uhm..."));
	}

	return std::make_tuple(ftree, nsstrs, origList, curType, curFType);
}









std::pair<std::pair<fir::Type*, Ast::Result_t>, fir::Type*> CodegenInstance::resolveStaticDotOperator(MemberAccess* ma, bool actual)
{
	iceAssert(ma->matype == MAType::LeftStatic);

	FunctionTree* ftree = 0;
	StructBase* curType = 0;
	fir::Type* curFType = 0;
	std::deque<std::string> nsstrs;
	std::deque<std::string> origList;

	std::tie(ftree, nsstrs, origList, curType, curFType) = unwrapStaticDotOperator(this, ma);




	// what is the right side?

	std::map<Func*, std::pair<std::string, Expr*>> errs;
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

				FuncDefPair fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, flist, &errs);
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

					FuncDefPair fp = this->tryResolveGenericFunctionCallUsingCandidates(fc, flist, &errs);
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
				if(errs.size() > 0)
				{
					GenError::prettyNoSuchFunctionError(this, fc, fc->name, fc->params, errs);
				}
				else
				{
					GenError::noFunctionTakingParams(this, fc, "namespace " + ftree->nsName, fc->name, fc->params);
				}
			}
		}

		// call that sucker.
		// but first set the cached target.

		if(actual)
		{
			if(res.t.firFunc == 0)
			{
				// iceAssert(res.t.funcDecl);
				// res.t.firFunc = dynamic_cast<fir::Function*>(res.t.funcDecl->codegen(this).value);
			}

			fir::Type* ltype = res.t.firFunc->getReturnType();
			fc->cachedResolveTarget = res;
			Result_t result = fc->codegen(this);

			return { { ltype, result }, curFType };
		}
		else
		{
			if(res.t.firFunc != 0)
			{
				return { { res.t.firFunc->getReturnType(), Result_t(0, 0) }, curFType };
			}
			else
			{
				iceAssert(res.t.funcDecl);
				return { { this->getTypeFromParserType(ma, res.t.funcDecl->ptype), Result_t(0, 0) }, curFType };
			}
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
					auto r = actual ? mpark::get<Result_t>(getStaticVariable(this, vr, cls, v->ident.name, true)) : Result_t(0, 0);

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
		error(ma, "Invalid expression type on right hand of dot operator");
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

FuncDefPair CodegenInstance::tryGetMemberFunctionOfClass(StructBase* sb, Expr* user, std::string name, fir::Value* extra)
{
	// find functions
	std::deque<fir::Function*> cands;
	std::map<fir::Function*, std::pair<FuncDecl*, Func*>> map;

	std::deque<Func*> genericBodies;
	if(auto cls = dynamic_cast<ClassDef*>(sb))
	{
		for(auto f : cls->funcs)
		{
			if(f->decl->ident.name == name)
			{
				if(f->decl->genericTypes.size() == 0)
					cands.push_back(cls->functionMap[f]), map[cls->functionMap[f]] = { f->decl, f };

				else
					genericBodies.push_back(f);
			}
		}
	}

	for(auto ext : this->getExtensionsForType(sb))
	{
		for(auto f : ext->funcs)
		{
			if(f->decl->ident.name == name)
			{
				if(f->decl->genericTypes.size() == 0)
					cands.push_back(ext->functionMap[f]), map[ext->functionMap[f]] = { f->decl, f };

				else
					genericBodies.push_back(f);
			}
		}
	}

	fir::Function* ret = this->tryDisambiguateFunctionVariableUsingType(user, name, cands, extra);
	if(ret)
	{
		auto p = map[ret];
		return FuncDefPair(ret, p.first, p.second);
	}
	else if(extra && extra->getType()->isPointerType() && extra->getType()->getPointerElementType()->isFunctionType())
	{
		std::map<Func*, std::pair<std::string, Expr*>> errs;
		return this->tryResolveGenericFunctionFromCandidatesUsingFunctionType(user, genericBodies,
			extra->getType()->getPointerElementType()->toFunctionType(), &errs);
	}
	else if(extra && extra->getType()->isFunctionType())
	{
		std::map<Func*, std::pair<std::string, Expr*>> errs;
		return this->tryResolveGenericFunctionFromCandidatesUsingFunctionType(user, genericBodies,
			extra->getType()->toFunctionType(), &errs);
	}

	return FuncDefPair::empty();
}








fir::Function* CodegenInstance::resolveAndInstantiateGenericFunctionReference(Expr* user, fir::FunctionType* oldft,
	fir::FunctionType* instantiatedFT, MemberAccess* ma, std::map<Func*, std::pair<std::string, Expr*>>* errs)
{
	iceAssert(!instantiatedFT->isGenericFunction() && "Cannot instantiate generic function with another generic function");
	iceAssert(ma);

	auto res = ma->codegen(this, fir::ConstantValue::getNullValue(instantiatedFT));
	return dynamic_cast<fir::Function*>(res.value);

	// if(ma->matype == MAType::LeftStatic)
	// {
	// 	// do the thing

	// 	FunctionTree* ftree = 0;
	// 	StructBase* strType = 0;
	// 	fir::Type* strFType = 0;

	// 	std::tie(ftree, std::ignore, std::ignore, strType, strFType) = unwrapStaticDotOperator(this, ma);

	// 	std::string name;
	// 	if(VarRef* vr = dynamic_cast<VarRef*>(ma->right))
	// 	{
	// 		name = vr->name;
	// 	}
	// 	else
	// 	{
	// 		error(user, "Unsupported use of dot-operator to get function??");
	// 	}


	// 	std::map<fir::Function*, Func*> map;

	// 	if(strType != 0)
	// 	{
	// 		// note(?): this procedure is only called when we need to instantiate a generic method/static generic method of a type (or in
	// 		// a namespace) with a concrete type
	// 		// so, we don't need to look at members or anything else, just functions.
	// 		//
	// 		// eg.
	// 		//
	// 		// let foo: [(SomeClass*, int) -> int] = SomeClass.someMethod
	// 		//
	// 		// ... (somewhere else)
	// 		//
	// 		// class SomeClass
	// 		// {
	// 		//     func someMethod<T>(a: T) -> T { ... }
	// 		// }
	// 		//
	// 		// we can't (and probably won't) have generic function types
	// 		// (eg. something like let foo: [<T, K>(a: T, b: T) -> K] or something)
	// 		// since there's no easy way to be type-safe about them.


	// 		// static function
	// 		ClassDef* cd = dynamic_cast<ClassDef*>(strType);
	// 		iceAssert(cd);

	// 		for(auto f : cd->funcs)
	// 		{
	// 			if(f->decl->ident.name == name && f->decl->genericTypes.size() > 0)
	// 				map[cd->functionMap[f]] = f;
	// 		}


	// 		for(auto ext : this->getExtensionsForType(cd))
	// 		{
	// 			for(auto f : ext->funcs)
	// 			{
	// 				if(f->decl->ident.name == name && f->decl->genericTypes.size() > 0)
	// 					map[ext->functionMap[f]] = f;
	// 			}
	// 		}
	// 	}
	// 	else
	// 	{
	// 		iceAssert(ftree);

	// 		for(auto f : ftree->genericFunctions)
	// 		{
	// 			if(f.first->ident.name == name)
	// 			{
	// 				if(!f.first->generatedFunc)
	// 					f.first->codegen(this);

	// 				iceAssert(f.first->generatedFunc);
	// 				map[f.first->generatedFunc] = f.second;
	// 			}
	// 		}
	// 	}

	// 	// failed to find
	// 	if(map.empty()) return 0;


	// 	std::deque<Func*> bodies;
	// 	for(auto m : map)
	// 		bodies.push_back(m.second);


	// 	// instantiate it.
	// 	FuncDefPair fp = this->tryResolveGenericFunctionFromCandidatesUsingFunctionType(user, bodies, instantiatedFT, errs);
	// 	return fp.firFunc;
	// }
	// else
	// {
	// 	error(user, "not supported??");
	// }
}




































static std::tuple<Func*, fir::Function*, fir::Type*, fir::Value*> callMemberFunction(CodegenInstance* cgi, MemberAccess* ma,
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
		std::map<Func*, std::pair<std::string, Expr*>> errs;
		FuncDefPair fp = cgi->tryResolveGenericFunctionCallUsingCandidates(fc, genericfunclist, &errs);

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

			exitless_error(fc, ops, "No such member function '%s' in class %s taking parameters (%s)\nPossible candidates (%zu):\n%s",
				fc->name.c_str(), cls->ident.name.c_str(), argstr.c_str(), fns.size(), candstr.c_str());

			if(errs.size() > 0)
			{
				for(auto p : errs)
					info(p.first, "Candidate not suitable: %s", p.second.first.c_str());
			}

			doTheExit();
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


































