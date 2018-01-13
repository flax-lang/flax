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


	fir::Type* TypecheckState::convertParserTypeToFIR(pts::Type* pt)
	{
		#define convert(...)	(this->convertParserTypeToFIR)(__VA_ARGS__)

		if(pt->isNamedType())
		{
			auto builtin = fir::Type::fromBuiltin(pt->toNamedType()->str());
			if(builtin)
			{
				return builtin;
			}
			else
			{
				auto name = pt->toNamedType()->str();

				if(name.find(".") == std::string::npos)
				{
					auto defs = this->getDefinitionsWithName(name);

					for(auto d : defs)
					{
						auto tyd = dcast(TypeDefn, d);
						if(!tyd)
						{
							exitless_error(this->loc(), "Definition of '%s' cannot be used as a type", d->id.name);
							info(d, "'%s' was defined here:", d->id.name);

							doTheExit();
						}

						return tyd->type;
					}

					error(this->loc(), "No such type '%s' defined", name);
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
					if(!begin) error(this->loc(), "No such scope '%s'", scopes.front());

					std::string prev = scopes.front();
					scopes.pop_front();

					while(scopes.size() > 0)
					{
						auto it = begin->subtrees.find(scopes.front());
						if(it == begin->subtrees.end())
							error(this->loc(), "No such entity '%s' in scope '%s'", scopes.front(), prev);

						prev = scopes.front();
						scopes.pop_front();

						begin = it->second;
					}

					// find the definitions.
					auto defs = begin->getDefinitionsWithName(actual);
					if(defs.empty())
					{
						error(this->loc(), "No type named '%s' in scope '%s'", actual, begin->name);
					}
					else if(defs.size() > 1)
					{
						exitless_error(this->loc(), "Ambiguous reference to entity '%s' in scope '%s'", actual, begin->name);
						for(auto d : defs)
							info(d, "Possible reference:");

						doTheExit();
					}

					auto def = defs[0];
					if(auto tyd = dcast(TypeDefn, def))
					{
						return tyd->type;
					}
					else
					{
						exitless_error(this->loc(), "Definition of '%s' cannot be used as a type", def->id.name);
						info(def, "'%s' was defined here:", def->id.name);

						doTheExit();
					}
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
















