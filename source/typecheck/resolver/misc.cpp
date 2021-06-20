// misc.cpp
// Copyright (c) 2017, zhiayang
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
			std::pair<TypeParamMap_t, ErrorMsg*> canonicalisePolyArguments(TypecheckState* fs, ast::Parameterisable* thing, const PolyArgMapping_t& pams)
			{
				if(thing->generics.empty())
				{
					return { { }, SimpleError::make(fs->loc(), "cannot canonicalise poly arguments for non-generic (or nested) entity '%s'",
						thing->name) };
				}

				TypeParamMap_t ret;

				//? we only check if we provided more than necessary, because (1) we might have optional args in the future, and (2) we can omit args
				//? to infer stuff.
				if(thing->generics.size() < pams.maps.size())
				{
					return { { }, SimpleError::make(fs->loc(),
						"mismatched number of type arguments to polymorph '%s'; expected %d, found %d instead",
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
				return zfu::map(args, [fs](const auto& a) -> FnCallArgument {
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

				auto [ dist, errs ] = resolver::computeOverloadDistance(unn->loc, target, zfu::map(*arguments, [](const FnCallArgument& fca) -> auto {
					return fir::LocatedType(fca.value->type, fca.loc);
				}), /* isCVarArg: */ false, fs->loc());

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


		std::pair<util::hash_map<std::string, size_t>, ErrorMsg*> verifyStructConstructorArguments(const Location& callLoc,
			const std::string& name, const std::vector<std::string>& fieldNames, const std::vector<FnCallArgument>& arguments)
		{
			//* note that structs don't have inline member initialisers, so there's no trouble with this approach (in the codegeneration)
			//* of inserting missing arguments as just '0' or whatever their default value is

			//* in the case of classes, they will have inline initialisers, so the constructor calling must handle such things.
			//* but then class constructors are handled like regular functions, so it should be fine.

			bool useNames = false;
			bool firstName = true;

			size_t ctr = 0;
			util::hash_map<std::string, size_t> seenNames;
			for(const auto& arg : arguments)
			{
				if((arg.name.empty() && useNames) || (!firstName && !useNames && !arg.name.empty()))
				{
					return { { }, SimpleError::make(arg.loc, "named arguments cannot be mixed with positional arguments in a struct constructor") };
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
				}


				if(!arg.name.empty() && std::find(fieldNames.begin(), fieldNames.end(), arg.name) == fieldNames.end())
				{
					return { { }, SimpleError::make(arg.loc, "field '%s' does not exist in struct '%s'", arg.name, name) };
				}
				else if(!arg.name.empty() && seenNames.find(arg.name) != seenNames.end())
				{
					return { { }, SimpleError::make(arg.loc, "duplicate argument for field '%s' in constructor call to struct '%s'",
						arg.name, name) };
				}

				firstName = false;
				seenNames[arg.name] = ctr;
				ctr += 1;
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames)
			{
				if(arguments.size() != fieldNames.size() && arguments.size() > 0)
				{
					return { { }, SimpleError::make(callLoc,
						"mismatched number of arguments in constructor call to type '%s'; expected %d, found %d instead",
						name, fieldNames.size(), arguments.size())->append(
							BareError::make(MsgType::Note, "all arguments are mandatory when using positional arguments")
						)
					};
				}

				// ok; populate 'seenNames' with all the fields, because we 'saw' them, I guess.
				for(size_t i = 0; i < fieldNames.size(); i++)
					seenNames[fieldNames[i]] = i;
			}

			return { seenNames, nullptr };
		}

	}






	int getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b)
	{
		return resolver::computeOverloadDistance(Location(), zfu::map(a, [](fir::Type* t) -> fir::LocatedType {
			return fir::LocatedType(t, Location());
		}), zfu::map(b, [](fir::Type* t) -> fir::LocatedType {
			return fir::LocatedType(t, Location());
		}), /* isCVarArg: */ false, Location()).first;
	}

	bool isDuplicateOverload(const std::vector<FnParam>& a, const std::vector<FnParam>& b)
	{
		return getOverloadDistance(zfu::map(a, [](const auto& p) -> auto { return p.type; }),
			zfu::map(b, [](const auto& p) -> auto { return p.type; })) == 0;
	}
}




































