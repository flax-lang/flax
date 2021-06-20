// type.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include "resolver.h"
#include "polymorph.h"

#include <deque>

namespace sst
{
	static ErrorMsg* _complainNoParentScope(TypecheckState* fs, const std::string& top)
	{
		return SimpleError::make(fs->loc(), "invalid use of '^' at the topmost scope '%s'", top);
	}


	static StateTree* recursivelyFindTreeUpwards(TypecheckState* fs, const std::string& name)
	{
		auto from = fs->stree;

		if(name == "^")
		{
			if(!from->parent)
				_complainNoParentScope(fs, from->name)->postAndQuit();

			// move to our parent scope.
			while(from->parent && from->isAnonymous)
				from = from->parent;

			return from;
		}

		while(from)
		{
			if(from->name == name)
				return from;

			else if(auto it = from->subtrees.find(name); it != from->subtrees.end())
				return it->second;

			from = from->parent;
		}

		return 0;
	}


	fir::Type* TypecheckState::convertParserTypeToFIR(pts::Type* pt, bool allowFail)
	{
		//* note: 'allowFail' allows failure when we *don't find anything*
		//* but if we find something _wrong_, then we will always fail.

		iceAssert(pt);
		this->pushLoc(pt->loc);
		defer(this->popLoc());

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
				if(name == "self")
				{
					if(!this->hasSelfContext())
						error(this->loc(), "invalid use of 'self' type while not in method body");

					else
						return this->getCurrentSelfContext();
				}


				auto returnTheThing = [this, pt](StateTree* tree, const std::string& name, bool scoped, bool allowFail) -> fir::Type* {

					if(auto gmt = this->findGenericMapping(name, true))
						return gmt;

					std::vector<Defn*> defs;

					if(scoped)  defs = tree->getDefinitionsWithName(name);
					else        defs = this->getDefinitionsWithName(name);

					if(defs.empty())
					{
						// try generic defs.
						StateTree* str = (scoped ? tree : this->stree);
						std::vector<ast::Parameterisable*> gdefs;
						while(str && (gdefs = str->getUnresolvedGenericDefnsWithName(name)).size() == 0)
							str = (scoped ? 0 : str->parent);   // if we're scoped, we can't go upwards.

						if(gdefs.empty())
						{
							if(allowFail)   return 0;
							else            error(this->loc(), "no type named '%s' in scope '%s'", name, tree->name);
						}
						else if(gdefs.size() > 1)
						{
							auto err = SimpleError::make(this->loc(), "ambiguous reference to entity '%s' in scope", name);
							for(auto d : gdefs)
								err->append(SimpleError::make(MsgType::Note, d->loc, "possible reference:"));

							err->postAndQuit();
						}


						// TODO: not re-entrant either.
						auto restore = this->stree;

						this->stree = (scoped ? tree : str);
						iceAssert(this->stree);

						{
							auto gdef = gdefs[0];
							iceAssert(gdef);

							auto atd = dcast(ast::TypeDefn, gdef);
							if(!atd) error(this->loc(), "entity '%s' is not a type", name);

							if(pt->toNamedType()->genericMapping.empty())
								error(this->loc(), "parametric type '%s' cannot be referenced without type arguments", pt->toNamedType()->name);

							// right, now we instantiate it.
							auto [ mapping, err ] = sst::resolver::misc::canonicalisePolyArguments(this, gdef, pt->toNamedType()->genericMapping);

							if(err != nullptr)
							{
								err->prepend(BareError::make("mismatched type arguments to instantiation of polymorphic type '%s':", gdef->name)
									)->postAndQuit();
							}

							// types generally cannot be overloaded, so it doesn't make sense for it to be SFINAE-ed.
							// unwrapping it will post the error if any.
							auto td = poly::fullyInstantiatePolymorph(this, atd, mapping).defn();

							this->stree = restore;
							return td->type;
						}
					}
					else if(defs.size() > 1)
					{
						auto err = SimpleError::make(this->loc(), "ambiguous reference to entity '%s' in scope '%s'", name, tree->name);
						for(auto d : defs)
							err->append(SimpleError::make(MsgType::Note, d->loc, "possible reference:"));

						err->postAndQuit();
					}


					auto d = defs[0];

					auto tyd = dcast(TypeDefn, d);
					if(!tyd)
					{
						// helpful error message: see if there's a actually a type further up!
						ErrorMsg* extraHelp = 0;
						{
							auto t = tree->parent;
							while(!extraHelp && t)
							{
								if(auto ds = t->getDefinitionsWithName(name); ds.size() > 0)
								{
									for(const auto& d : ds)
									{
										if(dcast(sst::TypeDefn, d))
										{
											std::vector<std::string> ss;
											auto t1 = tree;
											while(t1 != t)
											{
												t1 = t1->parent;
												if(!t1->isAnonymous)
													ss.push_back("^");
											}

											ss.push_back(name);

											extraHelp = SimpleError::make(MsgType::Note, d->loc,
												"'%s' was defined as a type in the parent scope, here:", name)
												->append(BareError::make(MsgType::Note, "to refer to it, use '%s'",
													zfu::join(ss, "::")));
										}
									}
								}

								t = t->parent;
							}
						}

						//* example of something 'wrong'
						auto err = SimpleError::make(this->loc(), "definition of '%s' cannot be used as a type", d->id.name)
							->append(SimpleError::make(MsgType::Note, d->loc, "'%s' was defined here:", d->id.name));

						if(extraHelp) err->append(extraHelp);

						err->postAndQuit();
					}

					return tyd->type;
				};


				if(name.find("::") == std::string::npos)
				{
					return returnTheThing(this->stree, name, false, allowFail);
				}
				else
				{
					// fuck me
					std::string actual;
					bool skip_to_root = false;
					std::deque<std::string> scopes;
					{
						std::string tmp;
						for(size_t i = 0; i < name.size(); i++)
						{
							if(name[i] == ':' && (i + 1 < name.size()) && name[i+1] == ':')
							{
								if(tmp.empty())
								{
									if(i == 0)
									{
										i++;
										skip_to_root = true;
										continue;
									}
									else
									{
										error(this->loc(), "expected identifier between consecutive scopes ('::') in nested type specifier");
									}
								}

								scopes.push_back(tmp);
								tmp.clear();

								i++;
							}
							else if(name[i] == '^')
							{
								if(!tmp.empty())
									error(this->loc(), "parent-scope-specifier '^' must appear in its own path segment ('%s' is invalid)", tmp + name[i]);

								else
									tmp = "^";
							}
							else
							{
								tmp += name[i];
							}
						}

						if(tmp.empty())
							error(this->loc(), "expected identifier after final '::' in nested type specifier");

						// don't push back.
						actual = tmp;
					}

					StateTree* begin = 0;

					if(skip_to_root)
					{
						begin = this->stree;
						while(begin->parent)
							begin = begin->parent;
					}
					else
					{
						iceAssert(scopes.size() > 0);
						begin = recursivelyFindTreeUpwards(this, scopes.front());
					}

					if(!begin)
					{
						if(allowFail)   return 0;
						else            error(this->loc(), "nonexistent scope '%s'", scopes.front());
					}


					if(scopes.size() > 0)
					{
						std::string prev = scopes.front();

						if(!skip_to_root)
							scopes.pop_front();

						while(scopes.size() > 0)
						{
							if(scopes.front() == "^")
							{
								if(!begin->parent)
									_complainNoParentScope(this, begin->name)->postAndQuit();

								while(begin->parent && begin->isAnonymous)
									begin = begin->parent;
							}
							else
							{
								auto it = begin->subtrees.find(scopes.front());
								if(it == begin->subtrees.end())
								{
									if(allowFail)   return 0;
									else            error(this->loc(), "no entity '%s' in scope '%s'", scopes.front(), prev);
								}

								begin = it->second;
							}

							prev = scopes.front();
							scopes.pop_front();
						}
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
			error(this->loc(), "invalid pts::Type found");
		}
	}
}


















