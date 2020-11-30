// dotop.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "gluecode.h"
#include "resolver.h"
#include "polymorph.h"

#include "ir/type.h"

#include "memorypool.h"

namespace names = strs::names;


// make the nice message.
static ErrorMsg* wrongDotOpError(ErrorMsg* e, sst::StructDefn* str, const Location& l, const std::string& name, bool usedStatic)
{
	if(usedStatic)
	{
		// check static ones for a better error message.
		sst::Defn* found = 0;
		for(auto sm : str->methods)
			if(sm->id.name == name) { found = sm; break; }

		if(!found)
		{
			for(auto sf : str->fields)
				if(sf->id.name == name) { found = sf; break; }
		}

		if(found) e->append(SimpleError::make(MsgType::Note, found->loc, "use '.' to refer to the instance member '%s'", name));

		return e;
	}
	else
	{
		if(auto cls = dcast(sst::ClassDefn, str))
		{
			// check static ones for a better error message.
			sst::Defn* found = 0;
			for(auto sm : cls->staticMethods)
				if(sm->id.name == name) { found = sm; break; }

			if(!found)
			{
				for(auto sf : cls->staticFields)
					if(sf->id.name == name) { found = sf; break; }
			}


			if(found) e->append(SimpleError::make(MsgType::Note, found->loc, "use '::' to refer to the static member '%s'", name));
		}

		return e;
	}
};







struct search_result_t
{
	search_result_t() { }
	search_result_t(fir::Type* t, size_t i, bool tr) : type(t), fieldIdx(i), isTransparent(tr) { }

	fir::Type* type = 0;
	size_t fieldIdx = 0;
	bool isTransparent = false;
};

static std::vector<search_result_t> searchTransparentFields(sst::TypecheckState* fs, std::vector<search_result_t> stack,
	const std::vector<sst::StructFieldDefn*>& fields, const Location& loc, const std::string& name)
{
	// search for them by name first, instead of doing a super-depth-first-search.
	for(auto df : fields)
	{
		if(df->id.name == name)
		{
			stack.push_back(search_result_t(df->type, 0, false));
			return stack;
		}
	}


	size_t idx = 0;
	for(auto df : fields)
	{
		if(df->isTransparentField)
		{
			auto ty = df->type;
			iceAssert(ty->isRawUnionType() || ty->isStructType());

			auto defn = fs->typeDefnMap[ty];
			iceAssert(defn);

			std::vector<sst::StructFieldDefn*> flds;
			if(auto str = dcast(sst::StructDefn, defn); str)
				flds = str->fields;

			else if(auto unn = dcast(sst::RawUnionDefn, defn); unn)
				flds = zfu::map(unn->fields, zfu::pair_second()) + unn->transparentFields;

			else
				error(loc, "what kind of type is this? '%s'", ty);

			stack.push_back(search_result_t(ty, idx, true));
			auto ret = searchTransparentFields(fs, stack, flds, loc, name);

			if(!ret.empty())    return ret;
			else                stack.pop_back();
		}

		idx += 1;
	}

	// if we've reached the end of the line, return nothing.
	return { };
}


static sst::FieldDotOp* resolveFieldNameDotOp(sst::TypecheckState* fs, sst::Expr* lhs, const std::vector<sst::StructFieldDefn*>& fields,
	const Location& loc, const std::string& name)
{
	for(auto df : fields)
	{
		if(df->id.name == name)
		{
			auto ret = util::pool<sst::FieldDotOp>(loc, df->type);
			ret->lhs = lhs;
			ret->rhsIdent = name;

			return ret;
		}
	}

	// sad. search for the field, recursively, in transparent members.
	auto ops = searchTransparentFields(fs, { }, fields, loc, name);
	if(ops.empty())
		return nullptr;

	// ok, now we just need to make a link of fielddotops...
	sst::Expr* cur = lhs;
	for(const auto& x : ops)
	{
		auto op = util::pool<sst::FieldDotOp>(loc, x.type);

		op->lhs = cur;
		op->isTransparentField = x.isTransparent;
		op->indexOfTransparentField = x.fieldIdx;

		// don't set a name if we're transparent.
		op->rhsIdent = (x.isTransparent ? "" : name);

		cur = op;
	}

	auto ret = dcast(sst::FieldDotOp, cur);
	iceAssert(ret);

	return ret;
}


















static sst::Expr* doExpressionDotOp(sst::TypecheckState* fs, ast::DotOperator* dotop, fir::Type* infer)
{
	auto lhs = dotop->left->typecheck(fs).expr();

	// first we got to find the struct defn based on the type
	auto type = lhs->type;
	if(!type)
	{
		if(dcast(sst::ScopeExpr, lhs))
		{
			error(dotop, "invalid use of '.' for static scope access; use '::' instead");
		}
		else
		{
			error(lhs, "failed to resolve type of lhs in expression dot-op!");
		}
	}
	else if(auto vr = dcast(sst::VarRef, lhs); vr && (dcast(sst::UnionDefn, vr->def) || dcast(sst::EnumDefn, vr->def)))
	{
		error(dotop, "use '::' to access enumeration cases and union variants");
	}

	if(type->isTupleType())
	{
		ast::LitNumber* ln = dcast(ast::LitNumber, dotop->right);
		if(!ln)
			error(dotop->right, "right-hand side of dot-operator on tuple type ('%s') must be a number literal", type);

		if(ln->num.find(".") != std::string::npos || ln->num.find("-") != std::string::npos)
			error(dotop->right, "tuple indices must be non-negative integer numerical literals");

		size_t n = std::stoul(ln->num);
		auto tup = type->toTupleType();

		if(n >= tup->getElementCount())
			error(dotop->right, "tuple only has %d elements, cannot access element %d", tup->getElementCount(), n);

		auto ret = util::pool<sst::TupleDotOp>(dotop->loc, tup->getElementN(n));
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
			if(zfu::match(vr->name, names::saa::FIELD_LENGTH, names::saa::FIELD_CAPACITY,
				names::saa::FIELD_REFCOUNT, names::string::FIELD_COUNT))
			{
				res = fir::Type::getNativeWord();
			}
			else if(vr->name == names::saa::FIELD_POINTER)
			{
				res = fir::Type::getInt8Ptr();
			}


			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
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
			if(fc->name == names::saa::FN_CLONE)
			{
				res = type;
				if(fc->args.size() != 0)
					error(fc, "builtin string method 'clone' expects exactly 0 arguments, found %d instead", fc->args.size());
			}
			else if(fc->name == names::saa::FN_APPEND)
			{
				res = fir::Type::getVoid();
				if(fc->args.size() != 1)
					error(fc, "builtin string method 'append' expects exactly 1 argument, found %d instead", fc->args.size());

				else if(!fc->args[0].first.empty())
					error(fc, "argument to builtin method 'append' cannot be named");

				args.push_back(fc->args[0].second->typecheck(fs).expr());
				if(!args[0]->type->isCharType() && !args[0]->type->isStringType() && !args[0]->type->isCharSliceType())
				{
					error(fc, "invalid argument type '%s' to builtin string method 'append'; expected one of '%s', '%s', or '%s'",
						args[0]->type, fir::Type::getInt8(), fir::Type::getCharSlice(false), fir::Type::getString());
				}
			}


			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
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
			if(vr->name == names::saa::FIELD_LENGTH || (type->isDynamicArrayType()
				&& zfu::match(vr->name, names::saa::FIELD_CAPACITY, names::saa::FIELD_REFCOUNT)))
			{
				res = fir::Type::getNativeWord();
			}
			else if(vr->name == names::saa::FIELD_POINTER)
			{
				res = type->getArrayElementType()->getPointerTo();
				if(type->isDynamicArrayType())
					res = res->getMutablePointerVersion();

				else if(type->isArraySliceType() && type->toArraySliceType()->isMutable())
					res = res->getMutablePointerVersion();
			}



			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
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

			if(fc->name == names::saa::FN_CLONE)
			{
				res = type;
				if(fc->args.size() != 0)
					error(fc, "builtin array method 'clone' expects exactly 0 arguments, found %d instead", fc->args.size());
			}
			else if(fc->name == names::saa::FN_APPEND)
			{
				if(type->isArrayType())
					error(fc, "'append' method cannot be called on arrays");

				res = fir::DynamicArrayType::get(type->getArrayElementType());

				if(fc->args.size() != 1)
					error(fc, "builtin array method 'append' expects exactly 1 argument, found %d instead", fc->args.size());

				else if(!fc->args[0].first.empty())
					error(fc, "argument to builtin method 'append' cannot be named");

				args.push_back(fc->args[0].second->typecheck(fs).expr());
				if(args[0]->type != type->getArrayElementType()     //? vv logic below looks a little sketch.
					&& ((args[0]->type->isArraySliceType() && args[0]->type->getArrayElementType() != type->getArrayElementType()))
					&& args[0]->type != type)
				{
					error(fc, "invalid argument type '%s' to builtin array method 'append'; expected one of '%s', '%s', or '%s'",
						args[0]->type, type, type->getArrayElementType(),
						fir::ArraySliceType::get(type->getArrayElementType(), false));
				}
			}
			else if(fc->name == names::array::FN_POP)
			{
				if(!type->isDynamicArrayType())
					error(fc, "'pop' method can only be called on dynamic arrays");

				res = type->getArrayElementType();

				if(fc->args.size() != 0)
					error(fc, "builtin array method 'pop' expects no arguments, found %d instead", fc->args.size());
			}

			// fallthrough

			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
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
			if(vr->name == names::enumeration::FIELD_NAME)
				res = fir::Type::getCharSlice(false);

			else if(vr->name == names::enumeration::FIELD_INDEX)
				res = fir::Type::getNativeWord();

			else if(vr->name == names::enumeration::FIELD_VALUE)
				res = type->toEnumType()->getCaseType();


			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
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
			if(vr->name == names::range::FIELD_BEGIN || vr->name == names::range::FIELD_END || vr->name == names::range::FIELD_STEP)
				res = fir::Type::getNativeWord();

			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
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
			if(vr->name == names::any::FIELD_TYPEID)
				res = fir::Type::getNativeUWord();

			else if(vr->name == names::any::FIELD_REFCOUNT)
				res = fir::Type::getNativeWord();

			if(res)
			{
				auto tmp = util::pool<sst::BuiltinDotOp>(dotop->right->loc, res);
				tmp->lhs = lhs;
				tmp->name = vr->name;

				return tmp;
			}
		}

		// else: fallthrough
	}



	// ok.
	auto defn = fs->typeDefnMap[type];

	// note: if `defn` is null, then all the dcasts will fail and we'll
	// fallthrough to the bottom.
	if(auto str = dcast(sst::StructDefn, defn))
	{
		// right.
		if(auto fc = dcast(ast::FunctionCall, dotop->right))
		{
			fs->pushSelfContext(str->type);
			defer(fs->popSelfContext());


			// check methods first
			std::vector<FnCallArgument> arguments = zfu::map(fc->args, [fs](auto arg) -> FnCallArgument {
				return FnCallArgument(arg.second->loc, arg.first, arg.second->typecheck(fs).expr(), arg.second);
			});

			//! SELF HANDLING (INSERTION) (DOT-METHOD-CALL)
			//* note: how we handle this is that we insert the self argument to interface with our resolver,
			//* then remove it below since our codegen will handle the actual insertion.
			arguments.insert(arguments.begin(), FnCallArgument::make(fc->loc, "this", str->type->getMutablePointerTo(),
				/* ignoreName: */ true));

			fs->teleportInto(str->innerScope);

			ErrorMsg* err = 0;
			sst::Defn* resolved = 0;

			auto curstr = str;
			while(!resolved)
			{
				auto arg_copy = arguments;
				auto res = sst::resolver::resolveFunctionCall(fs, fc->loc, fc->name, &arg_copy, fc->mappings,
					/* traverseUp: */ false, infer);

				if(res.isDefn())
				{
					resolved = res.defn();
					break;
				}

				if(auto cls = dcast(sst::ClassDefn, curstr); cls && cls->baseClass)
				{
					// teleport out, then back in.
					fs->teleportOut();
					fs->teleportInto(cls->baseClass->innerScope);

					curstr = cls->baseClass;
					continue;
				}

				// sighs
				err = res.error();
				break;
			}


			if(!resolved)
			{
				iceAssert(err);
				wrongDotOpError(SimpleError::make(fc->loc, "no valid call to method named '%s' in type '%s'%s", fc->name, str->id.name,
					curstr != str ? " (or any of its parent types)" : ""),
					str, fc->loc, fc->name, false)->append(err)->postAndQuit();
			}


			sst::Expr* finalCall = 0;
			if(auto fndef = dcast(sst::FunctionDefn, resolved); fndef)
			{
				auto c = util::pool<sst::FunctionCall>(fc->loc, resolved->type->toFunctionType()->getReturnType());
				c->arguments = arguments;
				c->name = fc->name;
				c->target = resolved;
				c->isImplicitMethodCall = false;

				//! SELF HANDLING (REMOVAL) (DOT-METHOD-CALL)
				c->arguments.erase(c->arguments.begin(), c->arguments.begin() + 1);
				finalCall = c;
			}
			else
			{
				auto c = util::pool<sst::ExprCall>(fc->loc, resolved->type->toFunctionType()->getReturnType());
				c->arguments = zfu::map(arguments, [](const FnCallArgument& e) -> sst::Expr* { return e.value; });

				auto tmp = util::pool<sst::FieldDotOp>(fc->loc, resolved->type);
				tmp->lhs = lhs;
				tmp->rhsIdent = fc->name;

				c->callee = tmp;

				//! SELF HANDLING (REMOVAL) (DOT-METHOD-CALL)
				c->arguments.erase(c->arguments.begin(), c->arguments.begin() + 1);
				finalCall = c;
			}


			auto ret = util::pool<sst::MethodDotOp>(fc->loc, resolved->type->toFunctionType()->getReturnType());
			ret->lhs = lhs;
			ret->call = finalCall;

			fs->teleportOut();
			return ret;
		}
		else if(auto fld = dcast(ast::Ident, dotop->right))
		{
			auto name = fld->name;
			{
				auto copy = str;

				while(copy)
				{
					auto hmm = resolveFieldNameDotOp(fs, lhs, copy->fields, dotop->loc, name);
					if(hmm) return hmm;

					// ok, we didn't find it.
					if(auto cls = dcast(sst::ClassDefn, copy); cls)
						copy = cls->baseClass;

					else
						copy = nullptr;
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
				wrongDotOpError(SimpleError::make(fld->loc, "no field named '%s' in type '%s'", fld->name, str->id.name),
					str, fld->loc, fld->name, false)->postAndQuit();
			}
			else
			{
				// ok, we potentially have a method -- if we used '.', error out.
				if(!dotop->isStatic)
					error(dotop, "use '::' to refer to a method of a type");

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
						auto err = SimpleError::make(dotop->right->loc, "ambiguous reference to method '%s' in struct '%s'", name, str->id.name);
						for(auto m : meths)
							err->append(SimpleError::make(MsgType::Note, m->loc, "potential target here:"));

						err->postAndQuit();
					}

					// else...
					if(!infer->isFunctionType())
					{
						error(dotop->right, "non-function type '%s' inferred for reference to method '%s' of struct '%s'",
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
					error(dotop->right, "no matching method named '%s' with signature '%s' to match inferred type",
						name, infer);
				}

				auto ret = util::pool<sst::FieldDotOp>(dotop->loc, retty);
				ret->lhs = lhs;
				ret->rhsIdent = name;
				ret->isMethodRef = true;

				return ret;
			}
		}
		else
		{
			error(dotop->right, "unsupported right-side expression for dot-operator on type '%s'", str->id.name);
		}
	}
	else if(auto rnn = dcast(sst::RawUnionDefn, defn))
	{
		if(auto fld = dcast(ast::Ident, dotop->right))
		{
			auto flds = zfu::map(rnn->fields, zfu::pair_second()) + rnn->transparentFields;
			auto hmm = resolveFieldNameDotOp(fs, lhs, flds, dotop->loc, fld->name);
			if(hmm)
			{
				return hmm;
			}
			else
			{
				// ok we didn't return. this is a raw union so extensions R NOT ALLOWED!! (except methods maybe)
				error(fld, "union '%s' has no member named '%s'", rnn->id.name, fld->name);
			}
		}
		else
		{
			error(dotop->right, "unsupported right-side expression for dot-operator on type '%s'", defn->id.name);
		}
	}
	else
	{
		// TODO: this error message could be better!!!
		//* it's because we are pending extension support!
		error(lhs, "unsupported left-side expression (with type '%s') for dot-operator", lhs->type);
	}

	// TODO: plug in extensions here!!
}



static sst::Expr* checkRhs2(sst::TypecheckState* fs, ast::DotOperator* dot, const sst::Scope& olds, const sst::Scope& news,
	fir::Type* rhs_infer, sst::StructDefn* possibleStructDefn = 0)
{
	if(auto id = dcast(ast::Ident, dot->right))
		id->traverseUpwards = false;

	else if(auto fc = dcast(ast::FunctionCall, dot->right))
		fc->traverseUpwards = false;

	// note: for function/expr calls, we typecheck the arguments *before* we teleport to the scope, so that we don't conflate
	// the scope of the argument (which is the current scope) with the scope of the call target (which is in whatever namespace)

	sst::Expr* ret = 0;
	if(auto fc = dcast(ast::FunctionCall, dot->right))
	{
		auto args = zfu::map(fc->args, [fs](auto e) -> FnCallArgument { return FnCallArgument(e.second->loc, e.first,
			e.second->typecheck(fs).expr(), e.second);
		});

		fs->teleportInto(news);
		ret = fc->typecheckWithArguments(fs, args, rhs_infer);
	}
	else if(auto ec = dcast(ast::ExprCall, dot->right))
	{
		auto args = zfu::map(fc->args, [fs](auto e) -> FnCallArgument { return FnCallArgument(e.second->loc, e.first,
			e.second->typecheck(fs).expr(), e.second);
		});

		fs->teleportInto(news);
		ret = ec->typecheckWithArguments(fs, args, rhs_infer);
	}
	else if(dcast(ast::Ident, dot->right) || dcast(ast::DotOperator, dot->right))
	{
		fs->teleportInto(news);
		auto res = dot->right->typecheck(fs, rhs_infer);
		if(res.isError() && possibleStructDefn && dcast(ast::Ident, dot->right))
		{
			wrongDotOpError(res.error(), possibleStructDefn, dot->right->loc, dcast(ast::Ident, dot->right)->name, true)->postAndQuit();
		}
		else
		{
			// will post if res is an error, even if we didn't give the fancy error.
			ret = res.expr();
		}
	}
	else
	{
		error(dot->right, "unexpected %s on right-side of dot-operator following static scope '%s' on the left", dot->right->readableName,
			news.string());
	}

	iceAssert(ret);

	fs->teleportOut();
	return ret;
}




static sst::Expr* doStaticDotOp(sst::TypecheckState* fs, ast::DotOperator* dot, sst::Expr* left, fir::Type* infer)
{
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

			def = fs->typeDefnMap[te->type];
		}

		if(!def)
		{
			error(left->loc, "could not resolve definition");
		}

		if(auto typdef = dcast(sst::TypeDefn, def))
		{
			if(dcast(sst::ClassDefn, def) || dcast(sst::StructDefn, def))
			{
				fs->pushSelfContext(def->type);

				auto oldscope = fs->scope();
				auto newscope = typdef->innerScope;

				fs->pushGenericContext();
				defer(fs->popGenericContext());
				{
					int pses = sst::poly::internal::getNextSessionId();

					iceAssert(typdef->original);
					for(auto g : typdef->original->generics)
						fs->addGenericMapping(g.first, fir::PolyPlaceholderType::get(g.first, pses));
				}

				auto ret = checkRhs2(fs, dot, oldscope, newscope, infer);
				fs->popSelfContext();

				return ret;
			}
			else if(auto unn = dcast(sst::UnionDefn, def))
			{
				bool wasfncall = false;
				std::string name;
				if(auto fc = dcast(ast::FunctionCall, dot->right))
				{
					wasfncall = true;
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
					SimpleError::make(dot->right->loc, "union '%s' has no variant '%s'", unn->id.name, name)
						->append(SimpleError::make(MsgType::Note, unn->loc, "union was defined here:"))
						->postAndQuit();
				}
				else if(!wasfncall && unn->type->containsPlaceholders() && infer == nullptr)
				{
					// note that we check infer == 0 before giving this error.
					// we should be able to pass in the infer value such that it works properly
					// eg. let x: Foo<int> = Foo.none
					SimpleError::make(dot->right->loc,
						"could not infer type parameters for polymorphic union '%s' using variant '%s' ",
						unn->id.name, name)->append(SimpleError::make(MsgType::Note, unn->variants[name]->loc, "variant was defined here:"))->postAndQuit();
				}
				else if(wasfncall && unn->type->toUnionType()->getVariants().at(name)->getInteriorType()->isVoidType())
				{
					SimpleError::make(dot->right->loc,
						"variant '%s' of union does not have values, and cannot be constructed via function-call",
						name, unn->id.name)->append(SimpleError::make(MsgType::Note, unn->variants[name]->loc, "variant was defined here:"))->postAndQuit();
				}

				// dot-op on the union to access its variants; we need constructor stuff for it.
				auto oldscope = fs->scope();
				auto newscope = unn->innerScope;

				return checkRhs2(fs, dot, oldscope, newscope, infer);
			}
			else if(auto enm = dcast(sst::EnumDefn, def))
			{
				auto oldscope = fs->scope();
				auto newscope = enm->innerScope;

				auto rhs = checkRhs2(fs, dot, oldscope, newscope, infer);

				if(auto vr = dcast(sst::VarRef, rhs))
				{
					iceAssert(vr->def && enm->cases[vr->name] == vr->def);
					return vr;
				}
				else
				{
					error(rhs, "unsupported right-hand expression on enum '%s'", enm->id.name);
				}
			}
			else
			{
				error(dot, "static access is not supported on type '%s'", def->type);
			}
		}
		else
		{
			error(dot, "invalid static access on variable (of type '%s'); use '.' to refer to instance members", def->type);
		}
	}
	else if(auto scp = dcast(sst::ScopeExpr, left))
	{
		auto oldscope = fs->scope();
		auto newscope = scp->scope;

		return checkRhs2(fs, dot, oldscope, newscope, infer);
	}

	error("????!!!!");
}



TCResult ast::DotOperator::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	this->left->checkAsType = this->checkAsType;
	this->right->checkAsType = this->checkAsType;

	if(this->isStatic)  return TCResult(doStaticDotOp(fs, this, this->left->typecheck(fs).expr(), infer));
	else                return TCResult(doExpressionDotOp(fs, this, infer));
}


























