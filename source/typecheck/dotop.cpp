// dotop.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)


static sst::Expr* doExpressionDotOp(TCS* fs, ast::DotOperator* dotop, fir::Type* infer)
{
	auto lhs = dotop->left->typecheck(fs);

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
	}
	else if(type->isStringType())
	{
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			// TODO: Extension support here
			fir::Type* res = 0;
			if(vr->name == "length" || vr->name == "count" || vr->name == "rc")
				res = fir::Type::getInt64();

			else if(vr->name == "ptr")
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
	}
	else if(type->isDynamicArrayType() || type->isArraySliceType() || type->isArrayType())
	{
		auto rhs = dotop->right;
		if(auto vr = dcast(ast::Ident, rhs))
		{
			// TODO: Extension support here
			fir::Type* res = 0;
			if(vr->name == "length" || vr->name == "count")
				res = fir::Type::getInt64();

			else if(type->isDynamicArrayType() && (vr->name == "capacity" || vr->name == "rc"))
				res = fir::Type::getInt64();

			else if(vr->name == "ptr")
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

			// TODO: extension support here
			if(fc->name == "clone")
			{
				res = type;
				if(fc->args.size() != 0)
					error(fc, "'clone' expects exactly 0 arguments, found '%d' instead", fc->args.size());
			}
			else if(fc->name == "pop")
			{
				res = type->getArrayElementType();
				if(fc->args.size() != 0)
					error(fc, "'pop' expects exactly 0 arguments, found '%d' instead", fc->args.size());
			}
			else if(fc->name == "back")
			{
				res = type->getArrayElementType();
				if(fc->args.size() != 0)
					error(fc, "'back' expects exactly 0 arguments, found '%d' instead", fc->args.size());
			}

			// fallthrough


			if(res)
			{
				auto tmp = new sst::BuiltinDotOp(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = fc->name;
				tmp->args = util::map(fc->args, [fs](auto e) -> sst::Expr* { return e.second->typecheck(fs); });
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
			if(vr->name == "name")
				res = fir::Type::getString();

			else if(vr->name == "index")
				res = fir::Type::getInt64();

			else if(vr->name == "value")
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
			if(vr->name == "begin" || vr->name == "end" || vr->name == "step")
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
				return FnCallArgument(arg.second->loc, arg.first, arg.second->typecheck(fs));
			});

			std::vector<Param> ts = util::map(arguments, [](FnCallArgument e) -> auto { return Param { e.name, e.loc, e.value->type }; });

			auto search = [fs, fc, str](std::vector<sst::Defn*> cands, std::vector<Param> ts, bool meths, TCS::PrettyError* errs) -> sst::Defn* {

				if(meths)
					ts.insert(ts.begin(), Param { "",fc->loc, str->type->getPointerTo() });

				return fs->resolveFunctionFromCandidates(cands, ts, errs, false);
			};

			std::vector<sst::Defn*> mcands = util::filter(util::map(str->methods, [](sst::FunctionDefn* fd) -> sst::Defn* { return fd; }),
				[fc](const sst::Defn* d) -> bool { return d->id.name == fc->name; });

			std::vector<sst::Defn*> vcands = util::filter(util::map(str->fields, [](sst::VarDefn* fd) -> sst::Defn* { return fd; }),
				[fc](const sst::Defn* d) -> bool { return d->id.name == fc->name; });


			if(mcands.empty() && vcands.empty())
				error(fc, "No method or field named '%s' in struct '%s'", fc->name, str->id.name);


			TCS::PrettyError errs;
			sst::Defn* resolved = search(mcands, ts, true, &errs);


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
				resolved = search(vcands, ts, false, &errs);
				if(!resolved)
				{
					exitless_error(fc, "%s", errs.errorStr);
					for(auto inf : errs.infoStrs)
						fprintf(stderr, "%s", inf.second.c_str());

					doTheExit();
				}

				// else

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
			for(auto f : str->fields)
			{
				if(f->id.name == name)
				{
					auto ret = new sst::FieldDotOp(dotop->loc, f->type);
					ret->lhs = lhs;
					ret->rhsIdent = name;

					return ret;
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
						exitless_error(dotop->right, "Ambiguous reference to method '%s' in struct '%s'", name, str->id.name);
						for(auto m : meths)
							info(m, "Potential target here:");

						doTheExit();
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
			error(dotop->right, "Unsupported right-side expression for dot-operator on struct '%s'", str->id.name);
		}
	}
	else
	{
		error(lhs, "Unsupported left-side expression (with type '%s') for dot-operator", lhs->type);
	}
}





sst::Expr* ast::DotOperator::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());

	// warn(this, "hi there");
	auto lhs = this->left->typecheck(fs);

	auto checkRhs = [](sst::TypecheckState* fs, ast::DotOperator* dot, const std::vector<std::string>& olds, const std::vector<std::string>& news) -> sst::Expr* {

		if(auto id = dcast(ast::Ident, dot->right))
			id->traverseUpwards = false;

		else if(auto fc = dcast(ast::FunctionCall, dot->right))
			fc->traverseUpwards = false;


		// note: for function/expr calls, we typecheck the arguments *before* we teleport to the scope, so that we don't conflate
		// the scope of the argument (which is the current scope) with the scope of the call target (which is in whatever namespace)

		sst::Expr* ret = 0;
		if(auto fc = dcast(ast::FunctionCall, dot->right))
		{
			auto args = util::map(fc->args, [fs](auto e) -> FnCallArgument { return FnCallArgument(e.second->loc, e.first, e.second->typecheck(fs)); });

			fs->teleportToScope(news);
			ret = fc->typecheckWithArguments(fs, args);
		}
		else if(auto ec = dcast(ast::ExprCall, dot->right))
		{
			auto args = util::map(fc->args, [fs](auto e) -> FnCallArgument { return FnCallArgument(e.second->loc, e.first, e.second->typecheck(fs)); });

			fs->teleportToScope(news);
			ret = ec->typecheckWithArguments(fs, args);
		}
		else
		{
			fs->teleportToScope(news);
			ret = dot->right->typecheck(fs);
		}


		iceAssert(ret);

		fs->teleportToScope(olds);
		return ret;
	};




	if(auto ident = dcast(sst::VarRef, lhs))
	{
		auto defs = fs->getDefinitionsWithName(ident->name);
		if(defs.empty())
		{
			error(lhs, "No namespace or type with name '%s' in scope '%s' / %s", ident->name, fs->serialiseCurrentScope());
		}
		else if(defs.size() > 1)
		{
			exitless_error(lhs, "Ambiguous reference to entity with name '%s'", ident->name);
			for(auto d : defs)
				info(d, "Possible reference:");

			doTheExit();
		}

		auto def = defs[0];



		if(auto td = dcast(sst::TreeDefn, def))
		{
			auto newscope = td->tree->getScope();
			auto oldscope = fs->getCurrentScope();

			auto expr = checkRhs(fs, this, oldscope, newscope);

			// check the thing
			if(auto vr = dcast(sst::VarRef, expr))
			{
				// check for global vars
				auto vrs = fs->stree->getDefinitionsWithName(vr->name);

				// must make sure it's a var/func defn and not a namespace one
				if(vrs.size() == 1 && (dynamic_cast<sst::VarDefn*>(vrs[0]) || dynamic_cast<sst::FunctionDefn*>(vrs[0])))
				{
					vr->def = vrs[0];
					return vr;
				}
				else if(vrs.size() > 1)
				{
					error(this, "Ambiguous reference to entity '%s' in namespace '%s'", vr->name, td->tree->name);
				}
				else
				{
					newscope.push_back(vr->name);
					auto ret = new sst::ScopeExpr(this->loc, fir::Type::getVoid());
					ret->scope = newscope;

					return ret;
				}
			}
			else
			{
				return expr;
			}
		}
		else if(auto typ = dcast(sst::TypeDefn, def))
		{
			if(auto cls = dcast(sst::ClassDefn, def))
			{
				auto oldscope = fs->getCurrentScope();
				auto newscope = cls->id.scope;
				newscope.push_back(cls->id.name);

				fs->teleportToScope(newscope);
				defer(fs->teleportToScope(oldscope));

				return checkRhs(fs, this, oldscope, newscope);
			}
			else if(auto enm = dcast(sst::EnumDefn, def))
			{
				auto oldscope = fs->getCurrentScope();
				auto newscope = enm->id.scope;
				newscope.push_back(enm->id.name);

				fs->teleportToScope(newscope);
				defer(fs->teleportToScope(oldscope));

				auto rhs = checkRhs(fs, this, oldscope, newscope);

				if(auto vr = dcast(sst::VarRef, rhs))
				{
					for(auto [ n, c ] : enm->cases)
					{
						if(c->id.name == vr->name)
						{
							auto ret = new sst::EnumDotOp(vr->loc, enm->type);
							ret->caseName = vr->name;
							ret->enumeration = enm;

							return ret;
						}
					}

					error(vr, "Enumeration '%s' has no case named '%s'", enm->id.name, vr->name);
				}
				else
				{
					error(rhs, "Unsupported right-hand expression on enum '%s'", enm->id.name);
				}
			}
			else
			{
				error(this, "Static access is not supported on type '%s'", def->type);
			}
		}
		else
		{
			// note: fallthrough to call to doExpressionDotOp()
		}
	}
	else if(auto scp = dcast(sst::ScopeExpr, lhs))
	{
		auto oldscope = fs->getCurrentScope();
		auto newscope = scp->scope;

		fs->teleportToScope(newscope);
		defer(fs->teleportToScope(oldscope));

		auto expr = checkRhs(fs, this, oldscope, newscope);


		if(auto vr = dcast(sst::VarRef, expr))
		{
			// if it's a global, stop with the scopeexpr and return now.
			if(dynamic_cast<sst::VarDefn*>(vr->def) || dynamic_cast<sst::FunctionDefn*>(vr->def))
				return vr;

			newscope.push_back(vr->name);
			auto ret = new sst::ScopeExpr(this->loc, fir::Type::getVoid());
			ret->scope = newscope;

			return ret;
		}
		else
		{
			return expr;
		}
	}

	// catch-all, probably.
	return doExpressionDotOp(fs, this, infer);
}










