// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "resolver.h"
#include "polymorph.h"
#include "typecheck.h"

#include "ir/type.h"

namespace sst
{
	namespace resolver
	{
		namespace misc
		{
			static std::vector<fir::LocatedType> _canonicaliseCallArguments(const Location& target,
				const std::unordered_map<std::string, size_t>& nameToIndex, const std::vector<FnCallArgument>& args, ErrorMsg** err)
			{
				std::vector<fir::LocatedType> ret(args.size());

				// strip out the name information, and do purely positional things.
				{
					int counter = 0;
					for(const auto& i : args)
					{
						if(!i.name.empty())
						{
							if(nameToIndex.find(i.name) == nameToIndex.end())
							{
								iceAssert(err);
								*err = SimpleError::make(MsgType::Note, i.loc, "function does not have a parameter named '%s'", i.name)->append(
									SimpleError::make(MsgType::Note, target, "function was defined here:")
								);

								return { };
							}
							else if(ret[nameToIndex.find(i.name)->second].type != 0)
							{
								iceAssert(err);
								*err = SimpleError::make(MsgType::Note, i.loc, "argument '%s' was already provided", i.name)->append(
									SimpleError::make(MsgType::Note, ret[nameToIndex.find(i.name)->second].loc, "here:")
								);

								return { };
							}
						}

						ret[i.name.empty() ? counter : nameToIndex.find(i.name)->second] = fir::LocatedType(i.value->type, i.loc);
						counter++;
					}
				}

				return ret;
			}



			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<FnParam>& params,
				const std::vector<FnCallArgument>& args, ErrorMsg** err)
			{
				return _canonicaliseCallArguments(target, getNameIndexMap(params), args, err);
			}

			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
				const std::vector<FnCallArgument>& args, ErrorMsg** err)
			{
				return _canonicaliseCallArguments(target, getNameIndexMap(params), args, err);
			}



			std::pair<TypeParamMap_t, ErrorMsg*> canonicalisePolyArguments(TypecheckState* fs, ast::Parameterisable* thing, const PolyArgMapping_t& pams)
			{
				if(thing->generics.empty())
					return { { }, SimpleError::make(fs->loc(), "cannot canonicalise poly arguments for non-generic (or nested) entity '%s'", thing->name) };

				TypeParamMap_t ret;

				//? we only check if we provided more than necessary, because (1) we might have optional args in the future, and (2) we can omit args
				//? to infer stuff.
				if(thing->generics.size() < pams.maps.size())
				{
					return { { }, SimpleError::make(fs->loc(), "mismatched number of type arguments to polymorph '%s'; expected %d, found %d instead",
						thing->name, thing->generics.size(), pams.maps.size()) };
				}

				for(const auto& pam : pams.maps)
				{
					if(!pam.name.empty())
					{
						// check if it exists.
						auto it = std::find_if(thing->generics.begin(), thing->generics.end(),
							[&pam](const std::pair<std::string, TypeConstraints_t>& a) -> bool {
								return a.first == pam.name;
							});

						if(it == thing->generics.end())
							return { { }, SimpleError::make(fs->loc(), "no type parameter named '%s' in polymorph '%s'", pam.name, thing->name) };

						// ok, it works.
						ret[pam.name] = fs->convertParserTypeToFIR(pam.type, /* allowFailure: */ false);
					}
					else
					{
						// ok, the index ought to exist.
						iceAssert(pam.index < thing->generics.size());

						// should be simple.
						ret[thing->generics[pam.index].first] = fs->convertParserTypeToFIR(pam.type, /* allowFailure: */ false);
					}
				}

				return { ret, nullptr };
			}


			std::vector<FnCallArgument> typecheckCallArguments(TypecheckState* fs, const std::vector<std::pair<std::string, ast::Expr*>>& args)
			{
				return util::map(args, [fs](const auto& a) -> FnCallArgument {
					return FnCallArgument(a.second->loc, a.first, a.second->typecheck(fs).expr(), a.second);
				});
			}
		}















		TCResult resolveAndInstantiatePolymorphicUnion(TypecheckState* fs, sst::UnionVariantDefn* uvd, std::vector<FnCallArgument>* arguments,
			fir::Type* union_infer, bool isFnCall)
		{
			auto name = uvd->variantName;
			auto unn = uvd->parentUnion;
			iceAssert(unn);

			if(unn->type->containsPlaceholders())
			{
				auto orig_unn = unn->original;
				iceAssert(orig_unn);

				auto [ res, soln ] = poly::attemptToInstantiatePolymorph(fs, orig_unn, name, /* gmaps: */ { }, /* return_infer */ nullptr,
					/* type_infer: */ union_infer, isFnCall, arguments, /* fillPlaceholders: */ false, /* problem_infer: */ nullptr);

				if(res.isError() || (res.isDefn() && res.defn()->type->containsPlaceholders()))
				{
					ErrorMsg* e = SimpleError::make(fs->loc(), "unable to infer types for union '%s' using variant '%s'", unn->id.name, name);

					if(res.isError())
						e->append(res.error());

					return TCResult(e);
				}

				// make it so
				unn = dcast(sst::UnionDefn, res.defn());
				iceAssert(unn);

				// re-do it.
				uvd = unn->variants[name];
				iceAssert(uvd);
			}

			auto vty = uvd->type->toUnionVariantType()->getInteriorType();

			if(isFnCall)
			{
				std::vector<fir::LocatedType> target;
				if(vty->isTupleType())
				{
					for(auto t : vty->toTupleType()->getElements())
						target.push_back(fir::LocatedType(t, uvd->loc));
				}
				else if(!vty->isVoidType())
				{
					target.push_back(fir::LocatedType(vty, uvd->loc));
				}

				auto [ dist, errs ] = resolver::computeOverloadDistance(unn->loc, target, util::map(*arguments, [](const FnCallArgument& fca) -> auto {
					return fir::LocatedType(fca.value->type, fca.loc);
				}), false);

				if(errs != nullptr || dist == -1)
				{
					auto x = SimpleError::make(fs->loc(), "mismatched types in construction of variant '%s' of union '%s'", name, unn->id.name);
					if(errs)    errs->prepend(x);
					else        errs = x;

					return TCResult(errs);
				}
			}

			return TCResult(uvd);
		}


		std::pair<std::unordered_map<std::string, size_t>, ErrorMsg*> verifyStructConstructorArguments(const Location& callLoc,
			const std::string& name, const std::set<std::string>& fieldNames, const std::vector<FnCallArgument>& arguments)
		{
			// ok, structs get named arguments, and no un-named arguments.
			// we just loop through each argument, ensure that (1) every arg has a name; (2) every name exists in the struct

			//* note that structs don't have inline member initialisers, so there's no trouble with this approach (in the codegeneration)
			//* of inserting missing arguments as just '0' or whatever their default value is

			//* in the case of classes, they will have inline initialisers, so the constructor calling must handle such things.
			//* but then class constructors are handled like regular functions, so it should be fine.

			bool useNames = false;
			bool firstName = true;

			size_t ctr = 0;
			std::unordered_map<std::string, size_t> seenNames;
			for(auto arg : arguments)
			{
				if(arg.name.empty() && useNames)
				{
					return { { }, SimpleError::make(arg.loc, "named arguments cannot be mixed with positional arguments in a struct constructor") };
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
					firstName = false;
				}
				else if(!arg.name.empty() && !useNames && !firstName)
				{
					return { { }, SimpleError::make(arg.loc, "named arguments cannot be mixed with positional arguments in a struct constructor") };
				}
				else if(useNames && fieldNames.find(arg.name) == fieldNames.end())
				{
					return { { }, SimpleError::make(arg.loc, "field '%s' does not exist in struct '%s'", arg.name, name) };
				}
				else if(useNames && seenNames.find(arg.name) != seenNames.end())
				{
					return { { }, SimpleError::make(arg.loc, "duplicate argument for field '%s' in constructor call to struct '%s'",
						arg.name, name) };
				}

				seenNames[arg.name] = ctr;
				ctr += 1;
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames && arguments.size() != fieldNames.size() && arguments.size() > 0)
			{
				return { { }, SimpleError::make(callLoc,
					"mismatched number of arguments in constructor call to type '%s'; expected %d arguments, found %d arguments instead",
					name, fieldNames.size(), arguments.size())->append(
						BareError::make(MsgType::Note, "all arguments are mandatory when using positional arguments")
					)
				};
			}

			return { seenNames, nullptr };
		}

	}






	int TypecheckState::getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b)
	{
		return resolver::computeOverloadDistance(Location(), util::map(a, [](fir::Type* t) -> fir::LocatedType {
			return fir::LocatedType(t, Location());
		}), util::map(b, [](fir::Type* t) -> fir::LocatedType {
			return fir::LocatedType(t, Location());
		}), false).first;
	}

	bool TypecheckState::isDuplicateOverload(const std::vector<FnParam>& a, const std::vector<FnParam>& b)
	{
		return this->getOverloadDistance(util::map(a, [](auto p) -> auto { return p.type; }),
			util::map(b, [](auto p) -> auto { return p.type; })) == 0;
	}

}
































