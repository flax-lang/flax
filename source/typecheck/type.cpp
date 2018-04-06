// type.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

namespace sst
{
	fir::Type* TypecheckState::inferCorrectTypeForLiteral(Expr* expr)
	{
		iceAssert(expr->type->isConstantNumberType());
		auto num = expr->type->toConstantNumberType()->getValue();

		// if(auto lit = dcast(sst::LiteralNumber, expr))
		{
			// auto num = lit->number;

			// check if we're an integer
			if(mpfr::isint(num))
			{
				// ok, check if it fits anywhere
				if(num <= mpfr::mpreal(INT64_MAX) && num >= mpfr::mpreal(INT64_MIN))
					return fir::Type::getInt64();

				else if(num >= 0 && num <= mpfr::mpreal(UINT64_MAX))
					return fir::Type::getUint64();

				else
					error(expr, "Numberic literal does not fit into largest supported (64-bit) type");
			}
			else
			{
				if(num >= mpfr::mpreal(DBL_MIN) && num <= mpfr::mpreal(DBL_MAX))
					return fir::Type::getFloat64();

				else if(num >= mpfr::mpreal(LDBL_MIN) && num <= mpfr::mpreal(LDBL_MAX))
					return fir::Type::getFloat80();

				else
					error(expr, "Numberic literal does not fit into largest supported type ('%s')", fir::Type::getFloat80());
			}
		}
	}


	fir::Type* TypecheckState::convertParserTypeToFIR(pts::Type* pt, bool allowFail)
	{
		//* note: 'allowFail' allows failure when we *don't find anything*
		//* but if we find something _wrong_, then we will always fail.

		#define convert(...)	(this->convertParserTypeToFIR)(__VA_ARGS__)

		iceAssert(pt);

		if(pt->isNamedType())
		{
			auto builtin = fir::Type::fromBuiltin(pt->toNamedType()->str());
			if(builtin)
			{
				return builtin;
			}
			else
			{
				auto name = pt->toNamedType()->name;

				auto returnTheThing = [this, pt](StateTree* tree, const std::string& name, bool scoped, bool allowFail) -> fir::Type* {

					if(auto gmt = this->findGenericTypeMapping(name, true))
						return gmt;

					std::vector<Defn*> defs;

					if(scoped)  defs = tree->getDefinitionsWithName(name);
					else        defs = this->getDefinitionsWithName(name);

					if(defs.empty())
					{
						// try generic defs.
						StateTree* str = this->stree;
						std::vector<ast::Stmt*> gdefs;
						while((gdefs = str->getUnresolvedGenericDefnsWithName(name)).size() == 0 && str)
							str = (scoped ? 0 : str->parent);   // if we're scoped, we can't go upwards.

						if(gdefs.empty())
						{
							if(allowFail)   return 0;
							else            error(this->loc(), "No type named '%s' in scope '%s'", name, tree->name);
						}
						else if(gdefs.size() > 1)
						{
							exitless_error(this->loc(), "Ambiguous reference to entity '%s' in scope", name);
							for(auto d : gdefs)
								info(d, "Possible reference:");

							doTheExit();
						}


						// TODO: not re-entrant either.
						auto restore = this->stree;
						this->stree = tree;
						{
							auto gdef = gdefs[0];
							iceAssert(gdef);

							auto atd = dcast(ast::TypeDefn, gdef);
							if(!atd) error(this->loc(), "Entity '%s' is not a type", name);

							// right, now we instantiate it.
							std::unordered_map<std::string, fir::Type*> mapping;
							for(auto mp : pt->toNamedType()->genericMapping)
								mapping[mp.first] = this->convertParserTypeToFIR(mp.second, allowFail);

							auto td = this->instantiateGenericType(atd, mapping);
							iceAssert(td);

							this->stree = restore;
							return td->type;
						}
					}
					else if(defs.size() > 1)
					{
						exitless_error(this->loc(), "Ambiguous reference to entity '%s' in scope '%s'", name, tree->name);
						for(auto d : defs)
							info(d, "Possible reference:");

						doTheExit();
					}


					for(auto d : defs)
					{
						auto tyd = dcast(TypeDefn, d);
						if(!tyd)
						{
							//* example of something 'wrong'
							exitless_error(this->loc(), "Definition of '%s' cannot be used as a type", d->id.name);
							info(d, "'%s' was defined here:", d->id.name);

							doTheExit();
						}

						return tyd->type;
					}

					iceAssert(0);
				};


				if(name.find(".") == std::string::npos)
				{
					return returnTheThing(this->stree, name, false, allowFail);
				}
				else
				{
					// fuck me
					std::string actual;
					std::deque<std::string> scopes;
					{
						std::string tmp;
						for(auto c : name)
						{
							if(c == '.')
							{
								if(tmp.empty())
									error(this->loc(), "Expected identifier between consecutive periods ('.') in nested type specifier");

								scopes.push_back(tmp);
								tmp.clear();
							}
							else
							{
								tmp += c;
							}
						}

						if(tmp.empty())
							error(this->loc(), "Expected identifier after final '.' in nested type specifier");

						// don't push back.
						actual = tmp;
					}

					auto begin = this->recursivelyFindTreeUpwards(scopes.front());
					if(!begin)
					{
						if(allowFail)   return 0;
						else            error(this->loc(), "No such scope '%s'", scopes.front());
					}

					std::string prev = scopes.front();
					scopes.pop_front();

					while(scopes.size() > 0)
					{
						auto it = begin->subtrees.find(scopes.front());
						if(it == begin->subtrees.end())
						{
							if(allowFail)   return 0;
							else            error(this->loc(), "No such entity '%s' in scope '%s'", scopes.front(), prev);
						}

						prev = scopes.front();
						scopes.pop_front();

						begin = it->second;
					}

					return returnTheThing(begin, actual, true, allowFail);
				}
			}
		}
		else if(pt->isPointerType())
		{
			return convert(pt->toPointerType()->base)->getPointerTo();
		}
		else if(pt->isTupleType())
		{
			std::vector<fir::Type*> ts;
			for(auto t : pt->toTupleType()->types)
				ts.push_back(convert(t));

			return fir::TupleType::get(ts);
		}
		else if(pt->isArraySliceType())
		{
			return fir::ArraySliceType::get(convert(pt->toArraySliceType()->base));
		}
		else if(pt->isDynamicArrayType())
		{
			return fir::DynamicArrayType::get(convert(pt->toDynamicArrayType()->base));
		}
		else if(pt->isFixedArrayType())
		{
			return fir::ArrayType::get(convert(pt->toFixedArrayType()->base), pt->toFixedArrayType()->size);
		}
		else if(pt->isFunctionType())
		{
			std::vector<fir::Type*> ps;
			for(auto p : pt->toFunctionType()->argTypes)
				ps.push_back(convert(p));

			auto ret = convert(pt->toFunctionType()->returnType);
			return fir::FunctionType::get(ps, ret);
		}
		else
		{
			error(this->loc(), "Invalid pts::Type found");
		}
	}
}
















