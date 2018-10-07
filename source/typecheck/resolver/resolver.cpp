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




	std::pair<TCResult, std::vector<FnCallArgument>> resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc,
		const std::vector<std::pair<Defn*, std::vector<FnCallArgument>>>& _cands, const TypeParamMap_t& gmaps, bool allowImplicitSelf,
		fir::Type* return_infer)
	{
		if(_cands.empty())
			return { TCResult(BareError("no candidates")), { } };

		int bestDist = INT_MAX;
		std::map<Defn*, SpanError> fails;
		std::vector<std::tuple<Defn*, std::vector<FnCallArgument>, int>> finals;

		auto cands = _cands;

		for(const auto& [ _cand, _args ] : cands)
		{
			int dist = -1;
			Defn* curcandidate = _cand;
			std::vector<FnCallArgument> replacementArgs = _args;

			if(auto fn = dcast(FunctionDecl, curcandidate))
			{
				// make a copy, i guess.

				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!
				// TODO: investigate for methods!!!!!!

				if(auto def = dcast(FunctionDefn, fn); def && def->parentTypeForMethod != 0 && allowImplicitSelf)
				{
					replacementArgs.insert(replacementArgs.begin(), FnCallArgument(def->loc, "self", new sst::TypeExpr(def->loc, def->parentTypeForMethod->getPointerTo()), nullptr));
				}



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

						// do an inference -- with the arguments that we have.
						auto [ res, soln ] = poly::attemptToInstantiatePolymorph(fs, fd->original, gmaps, /* return_infer: */ return_infer,
							/* type_infer: */ nullptr, /* isFnCall: */ true, &replacementArgs, /* fillPlacholders: */ false,
							/* problem_infer: */ fn->type);

						if(!res.isDefn())
						{
							fails[fn] = SpanError().append(res.error());
							dist = -1;
						}
						else
						{
							curcandidate = res.defn();
							std::tie(dist, fails[fn]) = std::make_tuple(soln.distance, SpanError());
						}
					}
				}
				else
				{
					std::tie(dist, fails[fn]) = computeOverloadDistance(fn->loc, fn->params, util::map(replacementArgs, [](auto p) -> auto {
						return Param(p);
					}), fn->isVarArg);
				}
			}
			else if(auto vr = dcast(VarDefn, curcandidate))
			{
				iceAssert(vr->type->isFunctionType());
				auto ft = vr->type->toFunctionType();

				// check if have any names
				for(auto p : replacementArgs)
				{
					if(p.name != "")
					{
						return { TCResult(SimpleError(p.loc, "Function values cannot be called with named arguments")
							.append(SimpleError(vr->loc, strprintf("'%s' was defined here:", vr->id.name)))
						), { } };
					}
				}

				auto prms = ft->getArgumentTypes();
				std::tie(dist, fails[vr]) = computeOverloadDistance(curcandidate->loc, util::map(prms, [](fir::Type* t) -> auto {
					return Param(t);
				}), util::map(replacementArgs, [](auto p) -> auto {
					return Param(p);
				}), false);
			}

			if(dist == -1)
				continue;

			else if(dist < bestDist)
				finals.clear(), finals.push_back({ curcandidate, replacementArgs, dist }), bestDist = dist;

			else if(dist == bestDist)
				finals.push_back({ curcandidate, replacementArgs, dist });
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

			return { TCResult(errs), { } };
		}
		else if(finals.size() > 1)
		{
			// check if all of the targets we found are virtual, and that they belong to the same class.

			bool virt = true;
			fir::ClassType* self = 0;

			Defn* ret = std::get<0>(finals[0]);

			for(auto def : finals)
			{
				if(auto fd = dcast(sst::FunctionDefn, std::get<0>(def)); fd && fd->isVirtual)
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
				return { TCResult(ret), std::get<1>(finals[0]) };
			}
			else
			{
				auto err = SimpleError(callLoc, strprintf("Ambiguous call to function '%s', have %zu candidates:",
					cands[0].first->id.name, finals.size()));

				for(auto f : finals)
				{
					err.append(SimpleError(std::get<0>(f)->loc, strprintf("Possible target (overload distance %d):", std::get<2>(f)),
						MsgType::Note));
				}

				return { TCResult(err), { } };
			}
		}
		else
		{
			return { TCResult(std::get<0>(finals[0])), std::get<1>(finals[0]) };
		}
	}

}
}





























