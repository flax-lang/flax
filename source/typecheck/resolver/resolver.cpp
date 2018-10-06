// resolver.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include "resolver.h"
#include "polymorph.h"

namespace sst {
namespace resolver
{
	using Param = FunctionDecl::Param;

	std::pair<int, SpanError> computeOverloadDistance(const Location& fnLoc, const std::vector<Param>& target, const std::vector<Param>& _args,
		bool cvararg)
	{
		SimpleError warnings;

		std::vector<Param> input;
		if(cvararg)
		{
			input = util::take(_args, target.size());

			// check we're not trying anything funny.
			for(auto it = _args.begin() + input.size(); it != _args.end(); it++)
			{
				if(!it->type->isPrimitiveType() && !it->type->isPointerType())
				{
					warnings.append(SimpleError::make(MsgType::Warning, it->loc,
						"passing non-primitive type '%s' to c-style variadic function is ill-advised", it->type));
				}
			}
		}
		else
		{
			input = _args;
		}

		SimpleError err;
		auto arguments = resolver::misc::canonicaliseCallArguments(fnLoc, target, input, &err);


		auto [ soln, err1 ] = poly::solveTypeList(util::map(target, [](const Param& p) -> fir::LocatedType {
			return fir::LocatedType(p.type, p.loc);
		}), arguments, poly::Solution_t(), /* isFnCall: */ true);


		if(err1.hasErrors())    return { -1, SpanError().append(err1) };
		else                    return { soln.distance, SpanError() };
	}




	TCResult resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc, const std::vector<std::pair<Defn*, std::vector<Param>>>& cands,
		const TypeParamMap_t& gmaps, bool allowImplicitSelf)
	{
		if(cands.empty()) return TCResult(BareError("no candidates"));

		using Param = FunctionDefn::Param;
		iceAssert(cands.size() > 0);


		int bestDist = INT_MAX;
		std::map<Defn*, SpanError> fails;
		std::vector<std::pair<Defn*, int>> finals;

		for(const auto& [ cand, arguments ] : cands)
		{
			int dist = -1;

			if(auto fn = dcast(FunctionDecl, cand))
			{
				// make a copy, i guess.
				auto args = arguments;

				if(auto def = dcast(FunctionDefn, fn); def && def->parentTypeForMethod != 0 && allowImplicitSelf)
					args.insert(args.begin(), Param(def->parentTypeForMethod->getPointerTo()));

				// check for placeholders -- means that we should attempt to infer the type of the parent if its a static method.
				//* there are some assumptions we can make -- primarily that this will always be a static method of a type.
				//? (a): namespaces cannot be generic.
				//? (b): instance methods must have an associated 'self', and you can't have a variable of generic type
				if(fn->type->containsPlaceholders())
				{
					if(auto fd = dcast(FunctionDefn, fn); !fd)
					{
						error("invalid non-definition of a function with placeholder types");
					}
					else
					{
						// ok, i guess.
						iceAssert(fd);
						iceAssert(fd->original);

						auto infer_args = util::map(args, [](const Param& e) -> FnCallArgument {
							return FnCallArgument(e.loc, e.name, new sst::TypeExpr(e.loc, e.type), 0);
						});

						// do an inference -- with the arguments that we have.
						//!!!! not done !!!!
						error("!!");
						auto [ res, soln ] = poly::attemptToInstantiatePolymorph(fs, fd->original, gmaps, /* return_infer: */ 0,
							/* type_infer: */ fn->type, /* isFnCall: */ true, &infer_args, false);

						if(!res.isDefn())
						{
							iceAssert(soln.distance == -1);
							fails[fn] = SpanError().append(res.error());
							dist = -1;
						}
						else
						{
							std::tie(dist, fails[fn]) = std::make_tuple(soln.distance, SpanError());
						}
					}
				}
				else
				{
					std::tie(dist, fails[fn]) = computeOverloadDistance(fn->loc, fn->params, args, fn->isVarArg);
				}
			}
			else if(auto vr = dcast(VarDefn, cand))
			{
				iceAssert(vr->type->isFunctionType());
				auto ft = vr->type->toFunctionType();

				// check if have any names
				for(auto p : arguments)
				{
					if(p.name != "")
					{
						return TCResult(SimpleError(p.loc, "Function values cannot be called with named arguments")
							.append(SimpleError(vr->loc, strprintf("'%s' was defined here:", vr->id.name)))
						);
					}
				}

				auto prms = ft->getArgumentTypes();
				std::tie(dist, fails[vr]) = computeOverloadDistance(cand->loc, util::map(prms, [](fir::Type* t) -> auto {
					return Param(t);
				}), arguments, false);
			}

			if(dist == -1)
				continue;

			else if(dist < bestDist)
				finals.clear(), finals.push_back({ cand, dist }), bestDist = dist;

			else if(dist == bestDist)
				finals.push_back({ cand, dist });
		}

		if(finals.empty())
		{
			// TODO: make this error message better, because right now we assume the arguments are all the same.

			iceAssert(cands.size() == fails.size());
			std::vector<fir::Type*> tmp = util::map(cands[0].second, [](Param p) -> auto { return p.type; });

			auto errs = OverloadError(SimpleError(callLoc, strprintf("No overload in call to '%s(%s)' amongst %zu %s",
				cands[0].first->id.name, fir::Type::typeListToString(tmp), fails.size(), util::plural("candidate", fails.size()))));

			for(auto f : fails)
				errs.addCand(f.first, f.second);

			return TCResult(errs);
		}
		else if(finals.size() > 1)
		{
			// check if all of the targets we found are virtual, and that they belong to the same class.

			bool virt = true;
			fir::ClassType* self = 0;

			Defn* ret = finals[0].first;
			for(auto def : finals)
			{
				if(auto fd = dcast(sst::FunctionDefn, def.first); fd && fd->isVirtual)
				{
					iceAssert(fd->parentTypeForMethod);
					iceAssert(fd->parentTypeForMethod->isClassType());

					if(!self)
					{
						self = fd->parentTypeForMethod->toClassType();
					}
					else
					{
						// check if they're co/contra variant
						auto ty = fd->parentTypeForMethod->toClassType();

						//* here we're just checking that 'ty' and 'self' are part of the same class hierarchy -- we don't really care about the method
						//* that we resolve being at the lowest or highest level of that hierarchy.

						if(!ty->isInParentHierarchy(self) && !self->isInParentHierarchy(ty))
						{
							virt = false;
							break;
						}
					}
				}
				else
				{
					virt = false;
					break;
				}
			}

			if(virt)
			{
				return TCResult(ret);
			}
			else
			{
				auto err = SimpleError(callLoc, strprintf("Ambiguous call to function '%s', have %zu candidates:",
					cands[0].first->id.name, finals.size()));

				for(auto f : finals)
					err.append(SimpleError(f.first->loc, strprintf("Possible target (overload distance %d):", f.second), MsgType::Note));

				return TCResult(err);
			}
		}
		else
		{
			return TCResult(finals[0].first);
		}
	}

}
}





























