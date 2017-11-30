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
			std::vector<sst::Expr*> arguments = util::map(fc->args, [fs](ast::Expr* arg) -> sst::Expr* { return arg->typecheck(fs); });
			std::vector<Param> ts = util::map(arguments, [](sst::Expr* e) -> auto { return Param { "", e->loc, e->type }; });

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
				c->arguments = arguments;

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

	if(auto ident = dcast(sst::VarRef, lhs))
	{
		auto defs = fs->getDefinitionsWithName(ident->name);
		if(defs.empty())
		{
			#if 0
			// check the state tree.
			auto st = fs->stree->searchForName(ident->name);
			if(st == 0)
				error(lhs, "No namespace or type with name '%s' in scope '%s' / %s", ident->name, fs->serialiseCurrentScope());

			// ok, then...
			auto oldscope = fs->getCurrentScope();
			auto newscope = st->getScope();

			fs->teleportToScope(newscope);
			defer(fs->teleportToScope(oldscope));

			if(auto id = dcast(ast::Ident, this->right))
				id->traverseUpwards = false;

			else if(auto fc = dcast(ast::FunctionCall, this->right))
				fc->traverseUpwards = false;

			// check what the right side is
			auto expr = this->right->typecheck(fs);
			iceAssert(expr);

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
					error(this, "Ambiguous reference to entity '%s' in namespace '%s'", vr->name, fs->stree->name);
				}
				else
				{
					newscope.push_back(vr->name);
					auto ret = new sst::ScopeExpr(this->loc, fir::Type::getVoid());
					ret->scope = newscope;

					// info(this, "ret scope %s", util::serialiseScope(scope));
					return ret;
				}
			}
			else
			{
				return expr;
			}
			#endif

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

		// TODO: consolidate the code below (seting traverseUpwards to false, teleporting scope, etc)


		if(auto td = dcast(sst::TreeDefn, def))
		{
			auto newscope = td->tree->getScope();
			auto oldscope = fs->getCurrentScope();

			fs->teleportToScope(newscope);
			defer(fs->teleportToScope(oldscope));

			if(auto id = dcast(ast::Ident, this->right))
				id->traverseUpwards = false;

			else if(auto fc = dcast(ast::FunctionCall, this->right))
				fc->traverseUpwards = false;

			// check what the right side is
			auto expr = this->right->typecheck(fs);
			iceAssert(expr);

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
				auto scope = cls->id.scope;
				scope.push_back(cls->id.name);

				fs->teleportToScope(scope);
				defer(fs->teleportToScope(oldscope));

				if(auto id = dcast(ast::Ident, this->right))
					id->traverseUpwards = false;

				else if(auto fc = dcast(ast::FunctionCall, this->right))
					fc->traverseUpwards = false;

				auto rhs = this->right->typecheck(fs);
				iceAssert(rhs);

				return rhs;
			}
			else if(auto enm = dcast(sst::EnumDefn, def))
			{
				auto oldscope = fs->getCurrentScope();
				auto scope = enm->id.scope;
				scope.push_back(enm->id.name);

				fs->teleportToScope(scope);
				defer(fs->teleportToScope(oldscope));

				if(auto id = dcast(ast::Ident, this->right))
					id->traverseUpwards = false;

				else if(auto fc = dcast(ast::FunctionCall, this->right))
					fc->traverseUpwards = false;

				auto rhs = this->right->typecheck(fs);
				iceAssert(rhs);

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

		auto scope = scp->scope;
		fs->teleportToScope(scope);

		// info(lhs, "scope is '%s'", util::serialiseScope(scope));

		if(auto id = dcast(ast::Ident, this->right))
			id->traverseUpwards = false;

		else if(auto fc = dcast(ast::FunctionCall, this->right))
			fc->traverseUpwards = false;

		auto expr = this->right->typecheck(fs);
		iceAssert(expr);

		fs->teleportToScope(oldscope);

		if(auto vr = dcast(sst::VarRef, expr))
		{
			// if it's a global, stop with the scopeexpr and return now.
			if(dynamic_cast<sst::VarDefn*>(vr->def) || dynamic_cast<sst::FunctionDefn*>(vr->def))
				return vr;

			scope.push_back(vr->name);
			auto ret = new sst::ScopeExpr(this->loc, fir::Type::getVoid());
			ret->scope = scope;

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










