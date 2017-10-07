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
			error(dotop->right, "Right-hand side of dot-operator on tuple type ('%s') must be a number literal", type->str());

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
	else if(type->isPointerType() && type->getPointerElementType()->isStructType())
	{
		type = type->getPointerElementType();
	}
	else if(!type->isStructType())
	{
		error(lhs, "Unsupported left-side expression (with type '%s') for dot-operator", lhs->type->str());
	}


	// ok.
	auto defn = fs->typeDefnMap[type];
	iceAssert(defn);

	if(auto str = dcast(sst::StructDefn, defn))
	{
		// right.
		if(auto fc = dcast(ast::FunctionCall, dotop->right))
		{
			error("no");
		}
		else if(auto fld = dcast(ast::Ident, dotop->right))
		{
			auto name = fld->name;
			for(auto f : str->fields)
			{
				if(f->id.name == name)
				{
					auto ret = new sst::InstanceDotOp(dotop->loc, f->type);
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
							infer->str(), name, str->id.name);
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
						name, infer->str());
				}

				auto ret = new sst::InstanceDotOp(dotop->loc, retty);
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
		error(lhs, "Unsupported left-side expression (with type '%s') for dot-operator", lhs->type->str());
	}
}







sst::Expr* ast::DotOperator::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());


	auto lhs = this->left->typecheck(fs);
	if(auto ident = dcast(sst::VarRef, lhs))
	{
		auto defs = fs->getDefinitionsWithName(ident->name);
		if(defs.empty())
		{
			error(lhs, "No namespace or type with name '%s' in scope '%s'", ident->name, fs->serialiseCurrentScope());
		}
		else if(defs.size() > 1)
		{
			exitless_error(lhs, "Ambiguous reference to entity with name '%s'", ident->name);
			for(auto d : defs)
				info(d, "Possible reference:");

			doTheExit();
		}

		auto def = defs[0];

		// check the type
		if(auto ns = dcast(sst::NamespaceDefn, def))
		{
			auto scope = ns->id.scope;
			scope.push_back(ns->id.name);

			auto oldscope = fs->getCurrentScope();

			fs->teleportToScope(scope);

			// check what the right side is
			auto expr = this->right->typecheck(fs);
			iceAssert(expr);

			fs->teleportToScope(oldscope);

			// check the thing
			if(auto vr = dcast(sst::VarRef, expr))
			{
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
		else if(auto typ = dcast(sst::TypeDefn, def))
		{
			error("static things not supported");
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

		auto expr = this->right->typecheck(fs);
		iceAssert(expr);

		fs->teleportToScope(oldscope);

		if(auto vr = dcast(sst::VarRef, expr))
		{
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










