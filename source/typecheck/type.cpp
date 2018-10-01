// type.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"
#include "polymorph.h"

namespace sst
{
	fir::Type* TypecheckState::inferCorrectTypeForLiteral(fir::ConstantNumberType* type)
	{
		auto ty = type->toConstantNumberType();
		{
			if(ty->isFloating())
			{
				if(ty->getMinBits() <= fir::Type::getFloat64()->getBitWidth())
					return fir::Type::getFloat64();

				else if(ty->getMinBits() <= fir::Type::getFloat80()->getBitWidth())
					return fir::Type::getFloat80();

				else if(ty->getMinBits() <= fir::Type::getFloat128()->getBitWidth())
					return fir::Type::getFloat128();

				else
					error("float overflow");
			}
			else
			{
				if(ty->getMinBits() <= fir::Type::getInt64()->getBitWidth() - 1)
					return fir::Type::getInt64();

				else if(ty->getMinBits() <= fir::Type::getInt128()->getBitWidth() - 1)
					return fir::Type::getInt128();

				else if(ty->isSigned() && ty->getMinBits() <= fir::Type::getUint128()->getBitWidth())
					return fir::Type::getUint128();

				else
					error("int overflow");
			}

			# if 0
				// ok, check if it fits anywhere
				if(num <= mpfr::mpreal(INT64_MAX) && num >= mpfr::mpreal(INT64_MIN))
					return fir::Type::getInt64();

				else if(num >= 0 && num <= mpfr::mpreal(UINT64_MAX))
					return fir::Type::getUint64();

				else
					error(this->loc(), "Numberic literal does not fit into largest supported (64-bit) type");
			}
			else
			{
				if(num >= mpfr::mpreal(DBL_MIN) && num <= mpfr::mpreal(DBL_MAX))
					return fir::Type::getFloat64();

				else if(num >= mpfr::mpreal(LDBL_MIN) && num <= mpfr::mpreal(LDBL_MAX))
					return fir::Type::getFloat80();

				else
					error(this->loc(), "Numberic literal does not fit into largest supported type ('%s')", fir::Type::getFloat80());
			}
			#endif
		}
	}


	fir::Type* TypecheckState::convertParserTypeToFIR(pts::Type* pt, bool allowFail)
	{
		//* note: 'allowFail' allows failure when we *don't find anything*
		//* but if we find something _wrong_, then we will always fail.

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

					if(auto gmt = this->findGenericMapping(name, true))
						return gmt;

					std::vector<Defn*> defs;

					if(scoped)  defs = tree->getDefinitionsWithName(name);
					else        defs = this->getDefinitionsWithName(name);

					if(defs.empty())
					{
						// try generic defs.
						StateTree* str = this->stree;
						std::vector<ast::Parameterisable*> gdefs;
						while(str && (gdefs = str->getUnresolvedGenericDefnsWithName(name)).size() == 0)
							str = (scoped ? 0 : str->parent);   // if we're scoped, we can't go upwards.

						if(gdefs.empty())
						{
							if(allowFail)   return 0;
							else            error(this->loc(), "No type named '%s' in scope '%s'", name, tree->name);
						}
						else if(gdefs.size() > 1)
						{
							auto err = SimpleError::make(this->loc(), "Ambiguous reference to entity '%s' in scope", name);
							for(auto d : gdefs)
								err.append(SimpleError::make(MsgType::Note, d, "Possible reference:"));

							err.postAndQuit();
						}


						// TODO: not re-entrant either.
						auto restore = this->stree;

						this->stree = (scoped ? this->stree : str);
						iceAssert(this->stree);

						{
							auto gdef = gdefs[0];
							iceAssert(gdef);

							auto atd = dcast(ast::TypeDefn, gdef);
							if(!atd) error(this->loc(), "Entity '%s' is not a type", name);

							if(pt->toNamedType()->genericMapping.empty())
								error(this->loc(), "Parametric type '%s' cannot be referenced without type arguments", pt->toNamedType()->name);

							// right, now we instantiate it.
							TypeParamMap_t mapping = this->convertParserTypeArgsToFIR(pt->toNamedType()->genericMapping, allowFail);

							// types generally cannot be overloaded, so it doesn't make sense for it to be SFINAE-ed.
							// unwrapping it will post the error if any.
							auto td = poly::fullyInstantiatePolymorph(this, atd, mapping).defn();

							this->stree = restore;
							return td->type;
						}
					}
					else if(defs.size() > 1)
					{
						auto err = SimpleError::make(this->loc(), "Ambiguous reference to entity '%s' in scope '%s'", name, tree->name);
						for(auto d : defs)
							err.append(SimpleError::make(MsgType::Note, d, "Possible reference:"));

						err.postAndQuit();
					}


					for(auto d : defs)
					{
						auto tyd = dcast(TypeDefn, d);
						if(!tyd)
						{
							if(allowFail) return 0;

							//* example of something 'wrong'
							SimpleError::make(this->loc(), "Definition of '%s' cannot be used as a type", d->id.name)
								.append(SimpleError::make(MsgType::Note, d, "'%s' was defined here:", d->id.name))
								.postAndQuit();
						}

						return tyd->type;
					}

					iceAssert(0);
					return 0;
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
			if(pt->toPointerType()->isMutable)
				return this->convertParserTypeToFIR(pt->toPointerType()->base)->getMutablePointerTo();

			else
				return this->convertParserTypeToFIR(pt->toPointerType()->base)->getPointerTo();
		}
		else if(pt->isTupleType())
		{
			std::vector<fir::Type*> ts;
			for(auto t : pt->toTupleType()->types)
				ts.push_back(this->convertParserTypeToFIR(t));

			return fir::TupleType::get(ts);
		}
		else if(pt->isArraySliceType())
		{
			return fir::ArraySliceType::get(this->convertParserTypeToFIR(pt->toArraySliceType()->base), pt->toArraySliceType()->mut);
		}
		else if(pt->isDynamicArrayType())
		{
			return fir::DynamicArrayType::get(this->convertParserTypeToFIR(pt->toDynamicArrayType()->base));
		}
		else if(pt->isVariadicArrayType())
		{
			return fir::ArraySliceType::getVariadic(this->convertParserTypeToFIR(pt->toVariadicArrayType()->base));
		}
		else if(pt->isFixedArrayType())
		{
			return fir::ArrayType::get(this->convertParserTypeToFIR(pt->toFixedArrayType()->base), pt->toFixedArrayType()->size);
		}
		else if(pt->isFunctionType())
		{
			std::vector<fir::Type*> ps;
			for(auto p : pt->toFunctionType()->argTypes)
				ps.push_back(this->convertParserTypeToFIR(p));

			auto ret = this->convertParserTypeToFIR(pt->toFunctionType()->returnType);
			return fir::FunctionType::get(ps, ret);
		}
		else
		{
			error(this->loc(), "Invalid pts::Type found");
		}
	}
}


















