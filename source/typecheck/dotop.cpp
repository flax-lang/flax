// dotop.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "gluecode.h"

#include "ir/type.h"

static sst::Expr* doExpressionDotOp(sst::TypecheckState* fs, ast::DotOperator* dotop, fir::Type* infer)
{
	auto lhs = dotop->left->typecheck(fs).expr();

	// first we got to find the struct defn based on the type
	auto type = lhs->type;
	if(type->isTupleType())
	{
		ast::LitNumber* ln = dcast(ast::LitNumber, dotop->right);
		if(!ln)
			error(dotop->right, "Right-hand side of dot-operator on tuple type ('%s') must be a number literal", type);

		if(ln->num.find(".") != std::string::npos || ln->num.find("-") != std::string::npos)
			error(dotop->right, "Tuple indices must be non-negative integer numerical literals");

		size_t n = std::stoul(ln->num);
		auto tup = type->toTupleType();

		if(n >= tup->getElementCount())
			error(dotop->right, "Tuple only has %zu elements, cannot access wanted element %zu", tup->getElementCount(), n);

		auto ret = new sst::TupleDotOp(dotop->loc, tup->getElementN(n));
		ret->lhs = lhs;
		ret->index = n;

		return ret;
	}
	else if(type->isPointerType() && (type->getPointerElementType()->isStructType() || type->getPointerElementType()->isClassType()))
	{
		type = type->getPointerElementType();

		//* note: it's in the else-if here because we currently don't support auto deref-ing (ie. use '.' on pointers)
		//*       for builtin types, only for classes and structs.

		// TODO: reevaluate this decision?
	}
	else if(type->isStringType())
	{
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			// TODO: Extension support here
			fir::Type* res = 0;
			if(util::match(vr->name, BUILTIN_SAA_FIELD_LENGTH, BUILTIN_SAA_FIELD_CAPACITY, BUILTIN_SAA_FIELD_REFCOUNT, BUILTIN_STRING_FIELD_COUNT))
				res = fir::Type::getInt64();

			else if(vr->name == BUILTIN_SAA_FIELD_POINTER)
				res = fir::Type::getInt8Ptr();


			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = vr->name;

				return tmp;
			}
			// else: break out below, for extensions
		}
		else if(auto fc = dcast(ast::FunctionCall, rhs))
		{
			fir::Type* res = 0;
			std::vector<sst::Expr*> args;
			if(fc->name == BUILTIN_SAA_FN_CLONE)
			{
				res = type;
				if(fc->args.size() != 0)
					error(fc, "Builtin string method 'clone' expects exactly 0 arguments, found %d instead", fc->args.size());
			}
			else if(fc->name == BUILTIN_SAA_FN_APPEND)
			{
				res = fir::Type::getVoid();
				if(fc->args.size() != 1)
					error(fc, "Builtin string method 'append' expects exactly 1 argument, found %d instead", fc->args.size());

				else if(!fc->args[0].first.empty())
					error(fc, "Argument to builtin method 'append' cannot be named");

				args.push_back(fc->args[0].second->typecheck(fs).expr());
				if(!args[0]->type->isCharType() && !args[0]->type->isStringType() && !args[0]->type->isCharSliceType())
				{
					error(fc, "Invalid argument type '%s' to builtin string method 'append'; expected one of '%s', '%s', or '%s'",
						args[0]->type, fir::Type::getInt8(), (fir::Type*) fir::Type::getCharSlice(false), (fir::Type*) fir::Type::getString());
				}
			}


			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->args = args;
				tmp->name = fc->name;
				tmp->isFunctionCall = true;

				return tmp;
			}
		}
	}
	else if(type->isDynamicArrayType() || type->isArraySliceType() || type->isArrayType())
	{
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			fir::Type* res = 0;
			if(vr->name == BUILTIN_SAA_FIELD_LENGTH || (type->isDynamicArrayType()
				&& util::match(vr->name, BUILTIN_SAA_FIELD_CAPACITY, BUILTIN_SAA_FIELD_REFCOUNT)))
			{
				res = fir::Type::getInt64();
			}
			else if(vr->name == BUILTIN_SAA_FIELD_POINTER)
			{
				res = type->getArrayElementType()->getPointerTo();
				if(type->isDynamicArrayType())
					res = res->getMutablePointerVersion();

				else if(type->isArraySliceType() && type->toArraySliceType()->isMutable())
					res = res->getMutablePointerVersion();
			}



			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = vr->name;

				return tmp;
			}
			// else: break out below, for extensions
		}
		else if(auto fc = dcast(ast::FunctionCall, rhs))
		{
			fir::Type* res = 0;
			std::vector<sst::Expr*> args;

			if(fc->name == BUILTIN_SAA_FN_CLONE)
			{
				res = type;
				if(fc->args.size() != 0)
					error(fc, "Builtin array method 'clone' expects exactly 0 arguments, found %d instead", fc->args.size());
			}
			else if(fc->name == BUILTIN_SAA_FN_APPEND)
			{
				if(type->isArrayType())
					error(fc, "'append' method cannot be called on arrays");

				res = fir::DynamicArrayType::get(type->getArrayElementType());

				if(fc->args.size() != 1)
					error(fc, "Builtin array method 'append' expects exactly 1 argument, found %d instead", fc->args.size());

				else if(!fc->args[0].first.empty())
					error(fc, "Argument to builtin method 'append' cannot be named");

				args.push_back(fc->args[0].second->typecheck(fs).expr());
				if(args[0]->type != type->getArrayElementType()     //? vv logic below looks a little sketch.
					&& ((args[0]->type->isArraySliceType() && args[0]->type->getArrayElementType() != type->getArrayElementType()))
					&& args[0]->type != type)
				{
					error(fc, "Invalid argument type '%s' to builtin array method 'append'; expected one of '%s', '%s', or '%s'",
						args[0]->type, type, type->getArrayElementType(), (fir::Type*) fir::ArraySliceType::get(type->getArrayElementType(), false));
				}
			}

			// fallthrough

			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->args = args;
				tmp->name = fc->name;
				tmp->isFunctionCall = true;

				return tmp;
			}
		}
	}
	else if(type->isEnumType())
	{
		// allow getting name, raw and value
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			// TODO: Extension support here
			fir::Type* res = 0;
			if(vr->name == BUILTIN_ENUM_FIELD_NAME)
				res = fir::Type::getString();

			else if(vr->name == BUILTIN_ENUM_FIELD_INDEX)
				res = fir::Type::getInt64();

			else if(vr->name == BUILTIN_ENUM_FIELD_VALUE)
				res = type->toEnumType()->getCaseType();


			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = vr->name;

				return tmp;
			}
			// else: break out below, for extensions
		}
	}
	else if(type->isRangeType())
	{
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			// TODO: extension support here
			fir::Type* res = 0;
			if(vr->name == BUILTIN_RANGE_FIELD_BEGIN || vr->name == BUILTIN_RANGE_FIELD_END || vr->name == BUILTIN_RANGE_FIELD_STEP)
				res = fir::Type::getInt64();

			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = vr->name;

				return tmp;
			}
		}

		// else: fallthrough;
	}
	else if(type->isAnyType())
	{
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			// TODO: extension support here
			fir::Type* res = 0;
			if(vr->name == BUILTIN_ANY_FIELD_TYPEID)
				res = fir::Type::getUint64();

			else if(vr->name == BUILTIN_ANY_FIELD_REFCOUNT)
				res = fir::Type::getInt64();

			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = vr->name;

				return tmp;
			}
		}

		// else: fallthrough
	}

	// TODO: plug in extensions here.


	if(!type->isStructType() && !type->isClassType())
	{
		error(dotop->right, "Unsupported right-side expression for dot operator on type '%s'", type);
	}


	// ok.
	auto defn = fs->typeDefnMap[type];
	iceAssert(defn);

	if(auto str = dcast(sst::StructDefn, defn))
	{
		// right.
		if(auto fc = dcast(ast::FunctionCall, dotop->right))
		{
			// check methods first
			using Param = sst::FunctionDefn::Param;
			std::vector<FnCallArgument> arguments = util::map(fc->args, [fs](auto arg) -> FnCallArgument {
				return FnCallArgument(arg.second->loc, arg.first, arg.second->typecheck(fs).expr(), arg.second);
			});

			std::vector<Param> ts = util::map(arguments, [](FnCallArgument e) -> auto { return Param(e); });

			auto search = [fs, fc, str](std::vector<sst::Defn*> cands, std::vector<Param> ts, bool meths) -> sst::Defn* {

				//! note: here we always presume it's mutable, since:
				//* 1. we right now cannot overload based on the mutating aspect of the method
				//* 2. mutable pointers can implicitly convert to immutable ones, but not vice versa.
				if(meths) ts.insert(ts.begin(), Param(str->type->getMutablePointerTo()));

				return fs->resolveFunctionCallFromCandidates(cands, ts, fs->convertParserTypeArgsToFIR(fc->mappings), false).defn();
			};

			std::vector<sst::Defn*> mcands;
			{
				auto base = str;
				while(base)
				{
					auto cds = util::filter(util::map(base->methods, [](sst::FunctionDefn* fd) -> sst::Defn* { return fd; }),
						[fc](const sst::Defn* d) -> bool { return d->id.name == fc->name; });

					mcands.insert(mcands.end(), cds.begin(), cds.end());

					if(auto cls = dcast(sst::ClassDefn, base))
						base = cls->baseClass;

					else
						base = 0;
				}
			}

			std::vector<sst::Defn*> vcands;
			{
				auto base = str;
				while(base)
				{
					auto cds = util::filter(util::map(str->fields, [](sst::VarDefn* fd) -> sst::Defn* { return fd; }),
						[fc](const sst::Defn* d) -> bool { return d->id.name == fc->name; });

					vcands.insert(vcands.end(), cds.begin(), cds.end());

					if(auto cls = dcast(sst::ClassDefn, base))
						base = cls->baseClass;

					else
						base = 0;
				}
			}


			if(mcands.empty() && vcands.empty())
				error(fc, "No method or field named '%s' in struct/class '%s'", fc->name, str->id.name);

			sst::Defn* resolved = search(mcands, ts, true);


			sst::Expr* finalCall = 0;
			if(resolved)
			{
				auto c = new sst::FunctionCall(fc->loc, resolved->type->toFunctionType()->getReturnType());
				c->arguments = arguments;
				c->name = fc->name;
				c->target = resolved;
				c->isImplicitMethodCall = false;

				finalCall = c;
			}
			else
			{
				resolved = search(vcands, ts, false);

				// else
				iceAssert(resolved);

				auto c = new sst::ExprCall(fc->loc, resolved->type->toFunctionType()->getReturnType());
				c->arguments = util::map(arguments, [](FnCallArgument e) -> sst::Expr* { return e.value; });

				auto tmp = new sst::FieldDotOp(fc->loc, resolved->type);
				tmp->lhs = lhs;
				tmp->rhsIdent = fc->name;

				c->callee = tmp;

				finalCall = c;
			}

			auto ret = new sst::MethodDotOp(fc->loc, resolved->type->toFunctionType()->getReturnType());
			ret->lhs = lhs;
			ret->call = finalCall;

			return ret;
		}
		else if(auto fld = dcast(ast::Ident, dotop->right))
		{
			auto name = fld->name;

			{
				auto copy = str;

				while(copy)
				{
					for(auto f : copy->fields)
					{
						if(f->id.name == name)
						{
							auto ret = new sst::FieldDotOp(dotop->loc, f->type);
							ret->lhs = lhs;
							ret->rhsIdent = name;

							return ret;
						}
					}

					// ok, we didn't find it.
					if(auto cls = dcast(sst::ClassDefn, copy); cls && cls->baseClass)
						copy = cls->baseClass;
				}
			}


			// check for method references
			std::vector<sst::FunctionDefn*> meths;
			for(auto m : str->methods)
			{
				if(m->id.name == name)
					meths.push_back(m);
			}

			if(meths.empty())
			{
				error(dotop->right, "No such instance field or method named '%s' in struct '%s'", name, str->id.name);
			}
			else
			{
				fir::Type* retty = 0;

				// ok, disambiguate if we need to
				if(meths.size() == 1)
				{
					retty = meths[0]->type;
				}
				else
				{
					// ok, we need to.
					if(infer == 0)
					{
						auto err = SimpleError::make(dotop->right, "Ambiguous reference to method '%s' in struct '%s'", name, str->id.name);
						for(auto m : meths)
							err.append(SimpleError::make(MsgType::Note, m, "Potential target here:"));

						err.postAndQuit();
					}

					// else...
					if(!infer->isFunctionType())
					{
						error(dotop->right, "Non-function type '%s' inferred for reference to method '%s' of struct '%s'",
							infer, name, str->id.name);
					}

					// ok.
					for(auto m : meths)
					{
						if(m->type == infer)
						{
							retty = m->type;
							break;
						}
					}

					// hm, okay
					error(dotop->right, "No matching method named '%s' with signature '%s' to match inferred type",
						name, infer);
				}

				auto ret = new sst::FieldDotOp(dotop->loc, retty);
				ret->lhs = lhs;
				ret->rhsIdent = name;
				ret->isMethodRef = true;

				return ret;
			}
		}
		else
		{
			error(dotop->right, "Unsupported right-side expression for dot-operator on struct/class '%s'", str->id.name);
		}
	}
	else
	{
		error(lhs, "Unsupported left-side expression (with type '%s') for dot-operator", lhs->type);
	}
}



static sst::Expr* doStaticDotOp(sst::TypecheckState* fs, ast::DotOperator* dot, sst::Expr* left, fir::Type* infer)
{
	auto checkRhs = [](sst::TypecheckState* fs, ast::DotOperator* dot, const std::vector<std::string>& olds, const std::vector<std::string>& news,
		fir::Type* infer) -> sst::Expr* {

		if(auto id = dcast(ast::Ident, dot->right))
			id->traverseUpwards = false;

		else if(auto fc = dcast(ast::FunctionCall, dot->right))
			fc->traverseUpwards = false;


		// note: for function/expr calls, we typecheck the arguments *before* we teleport to the scope, so that we don't conflate
		// the scope of the argument (which is the current scope) with the scope of the call target (which is in whatever namespace)

		sst::Expr* ret = 0;
		if(auto fc = dcast(ast::FunctionCall, dot->right))
		{
			auto args = util::map(fc->args, [fs](auto e) -> FnCallArgument { return FnCallArgument(e.second->loc, e.first,
				e.second->typecheck(fs).expr(), e.second);
			});

			fs->teleportToScope(news);
			ret = fc->typecheckWithArguments(fs, args, infer);
		}
		else if(auto ec = dcast(ast::ExprCall, dot->right))
		{
			auto args = util::map(fc->args, [fs](auto e) -> FnCallArgument { return FnCallArgument(e.second->loc, e.first,
				e.second->typecheck(fs).expr(), e.second);
			});

			fs->teleportToScope(news);
			ret = ec->typecheckWithArguments(fs, args, infer);
		}
		else if(dcast(ast::Ident, dot->right) || dcast(ast::DotOperator, dot->right))
		{
			fs->teleportToScope(news);
			ret = dot->right->typecheck(fs).expr();

			//* special-case this thing. if we don't do this, then 'ret' is just a normal VarRef,
			//* which during codegen will try to trigger the codegne for the UnionVariantDefn,
			//* which returns 0 (because there's really nothing to define at code-gen time)

			//? note: ret->type can be null if we're in the middle of a namespace dot-op,
			//? and 'ret' is a ScopeExpr.
			if(ret->type && ret->type->isUnionVariantType())
				ret = new sst::TypeExpr(ret->loc, ret->type);
		}
		else
		{
			error(dot->right, "Unexpected %s on right-side of dot-operator following static scope '%s' on the left", dot->right->readableName,
				util::serialiseScope(news));
		}


		iceAssert(ret);

		fs->teleportToScope(olds);
		return ret;
	};



	// if we get a type expression, then we want to dig up the definition from the type.
	if(auto ident = dcast(sst::VarRef, left); ident || dcast(sst::TypeExpr, left))
	{
		sst::Defn* def = 0;
		if(ident)
		{
			def = ident->def;
		}
		else
		{
			auto te = dcast(sst::TypeExpr, left);
			iceAssert(te);

			warn(dot->left, "%s", te->type);
			def = fs->typeDefnMap[te->type];
		}

		iceAssert(def);

		if(auto td = dcast(sst::TreeDefn, def))
		{
			auto newscope = td->tree->getScope();
			auto oldscope = fs->getCurrentScope();

			auto expr = checkRhs(fs, dot, oldscope, newscope, infer);

			// check the thing
			if(auto vr = dcast(sst::VarRef, expr); vr && dcast(sst::TreeDefn, vr->def))
			{
				newscope.push_back(vr->name);
				auto ret = new sst::ScopeExpr(dot->loc, fir::Type::getVoid());
				ret->scope = newscope;

				return ret;
			}
			else
			{
				return expr;
			}
		}
		else if(dcast(sst::TypeDefn, def))
		{
			if(auto cls = dcast(sst::ClassDefn, def))
			{
				auto oldscope = fs->getCurrentScope();
				auto newscope = cls->id.scope;
				newscope.push_back(cls->id.name);

				return checkRhs(fs, dot, oldscope, newscope, infer);
			}
			else if(auto str = dcast(sst::StructDefn, def))
			{
				auto oldscope = fs->getCurrentScope();
				auto newscope = str->id.scope;
				newscope.push_back(str->id.name);

				return checkRhs(fs, dot, oldscope, newscope, infer);
			}
			else if(auto unn = dcast(sst::UnionDefn, def))
			{
				std::string name;
				if(auto fc = dcast(ast::FunctionCall, dot->right))
				{
					name = fc->name;
				}
				else if(auto id = dcast(ast::Ident, dot->right))
				{
					name = id->name;
				}
				else
				{
					error(dot->right, "unexpected right-hand expression on dot-operator on union '%s'",
						unn->id.name);
				}


				if(!unn->type->toUnionType()->hasVariant(name))
				{
					SimpleError::make(dot->right, "Union '%s' has no variant '%s'", unn->id.name, name)
						.append(SimpleError::make(MsgType::Note, unn, "Union was defined here:"))
						.postAndQuit();
				}

				// dot-op on the union to access its variants; we need constructor stuff for it.
				auto oldscope = fs->getCurrentScope();
				auto newscope = unn->id.scope;
				newscope.push_back(unn->id.name);

				return checkRhs(fs, dot, oldscope, newscope, infer);
			}
			else if(auto enm = dcast(sst::EnumDefn, def))
			{
				auto oldscope = fs->getCurrentScope();
				auto newscope = enm->id.scope;
				newscope.push_back(enm->id.name);

				auto rhs = checkRhs(fs, dot, oldscope, newscope, infer);

				if(auto vr = dcast(sst::VarRef, rhs))
				{
					iceAssert(vr->def && enm->cases[vr->name] == vr->def);
					return vr;
				}
				else
				{
					error(rhs, "Unsupported right-hand expression on enum '%s'", enm->id.name);
				}
			}
			else
			{
				error(dot, "Static access is not supported on type '%s'", def->type);
			}
		}
		else
		{
			// note: fallthrough to call to doExpressionDotOp()
		}
	}
	else if(auto scp = dcast(sst::ScopeExpr, left))
	{
		auto oldscope = fs->getCurrentScope();
		auto newscope = scp->scope;

		auto expr = checkRhs(fs, dot, oldscope, newscope, infer);

		if(auto vr = dcast(sst::VarRef, expr); vr && dcast(sst::TreeDefn, vr->def))
		{
			newscope.push_back(vr->name);
			auto ret = new sst::ScopeExpr(dot->loc, fir::Type::getVoid());
			ret->scope = newscope;

			return ret;
		}
		else
		{
			return expr;
		}
	}

	return 0;
}



TCResult ast::DotOperator::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = doStaticDotOp(fs, this, this->left->typecheck(fs).expr(), infer);
	if(ret) return TCResult(ret);

	// catch-all, probably.
	return TCResult(doExpressionDotOp(fs, this, infer));
}










