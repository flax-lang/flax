// solver.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "sst.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include "polymorph.h"


namespace sst
{
	namespace poly
	{
		static ErrorMsg* solveSingleTypeList(Solution_t* soln, const Location& callLoc, const std::vector<fir::LocatedType>& target,
			const std::vector<fir::LocatedType>& given, bool isFnCall, size_t firstOptionalArgument);

		static ErrorMsg* solveSingleType(Solution_t* soln, const fir::LocatedType& target, const fir::LocatedType& given)
		{
			auto tgt = target.type;
			auto gvn = given.type;

			// if we're just looking at normal types, then just add the cost.
			if(!tgt->containsPlaceholders() && !gvn->containsPlaceholders())
			{
				int dist = fir::getCastDistance(gvn, tgt);
				if(dist >= 0)
				{
					soln->distance += dist;
					return nullptr;
				}
				else
				{
					soln->distance = -1;
					return SimpleError::make(given.loc, "no valid cast from given type '%s' to target type '%s'", gvn, tgt);
				}
			}
			else
			{
				// limit decomposition of the given types by the number of transforms on the target type.
				auto [ tt, ttrfs ] = internal::decomposeIntoTransforms(tgt, -1);
				auto [ gt, gtrfs ] = internal::decomposeIntoTransforms(gvn, ttrfs.size());

				// if(ttrfs != gtrfs)
				// hmm???

				// substitute if possible.
				if(auto _gt = soln->substitute(gt); _gt != gt)
					gt = _gt;

				// check what kind of monster we're dealing with.
				if(tt->isPolyPlaceholderType())
				{
					// see if there's a match.
					auto ptt = tt->toPolyPlaceholderType();
					if(auto ltt = soln->getSolution(ptt->getName()); ltt != 0)
					{
						// check for conflict.
						if(!ltt->isPolyPlaceholderType() && !gt->isPolyPlaceholderType())
						{
							if(ltt->isConstantNumberType() || gt->isConstantNumberType())
							{
								gt = internal::mergeNumberTypes(ltt, gt);
								if(gt != ltt)
									soln->addSolution(ptt->getName(), fir::LocatedType(gt, given.loc));
							}
							else if(ltt != gt)
							{
								if(int d = fir::getCastDistance(gt, ltt); d >= 0)
								{
									soln->distance += d;
								}
								else
								{
									return SimpleError::make(given.loc, "conflicting solutions for type parameter '%s': previous: '%s', current: '%s'",
										ptt->getName(), ltt->str(), gvn);
								}
							}
						}
						else if(ltt->isPolyPlaceholderType() && !gt->isPolyPlaceholderType())
						{
							soln->addSubstitution(ltt, gt);
						}
						else if(!ltt->isPolyPlaceholderType() && gt->isPolyPlaceholderType())
						{
							soln->addSubstitution(gt, ltt);
						}
						else if(ltt->isPolyPlaceholderType() && gt->isPolyPlaceholderType())
						{
							warn("what???? '%s' and '%s' are both poly??", ltt->str(), gt);
						}
					}
					else
					{
						// debuglogln("solved %s = %s", ptt->getName(), gt);
						soln->addSolution(ptt->getName(), fir::LocatedType(gt, given.loc));
					}
				}
				else if(tt->isFunctionType() || tt->isTupleType())
				{
					// make sure they're the same 'kind' of type first.
					if(tt->isFunctionType() != gt->isFunctionType() || tt->isTupleType() != gt->isTupleType())
						return SimpleError::make(given.loc, "no valid conversion from given type '%s' to target type '%s'", gt, tt);

					std::vector<fir::LocatedType> problem;
					std::vector<fir::LocatedType> input;
					if(gt->isFunctionType())
					{
						input = util::map(gt->toFunctionType()->getArgumentTypes(), [given](fir::Type* t) -> fir::LocatedType {
							return fir::LocatedType(t, given.loc);
						}) + fir::LocatedType(gt->toFunctionType()->getReturnType(), given.loc);

						problem = util::map(tt->toFunctionType()->getArgumentTypes(), [target](fir::Type* t) -> fir::LocatedType {
							return fir::LocatedType(t, target.loc);
						}) + fir::LocatedType(tt->toFunctionType()->getReturnType(), target.loc);
					}
					else
					{
						iceAssert(gt->isTupleType());
						input = util::map(gt->toTupleType()->getElements(), [given](fir::Type* t) -> fir::LocatedType {
							return fir::LocatedType(t, given.loc);
						});

						problem = util::map(tt->toTupleType()->getElements(), [target](fir::Type* t) -> fir::LocatedType {
							return fir::LocatedType(t, target.loc);
						});
					}

					// for recursive solving, we're never a function call.
					// related: so, firstOptionalArgument is always -1 in these cases.
					return solveSingleTypeList(soln, given.loc, problem, input, /* isFnCall: */ false, /* firstOptionalArgument: */ -1);
				}
				else
				{
					error("'%s' not supported", tt);
				}
			}

			return nullptr;
		}



		static ErrorMsg* solveSingleTypeList(Solution_t* soln, const Location& callLoc, const std::vector<fir::LocatedType>& target,
			const std::vector<fir::LocatedType>& given, bool isFnCall, size_t firstOptionalArgument)
		{
			bool fvararg = (isFnCall && target.size() > 0 && target.back()->isVariadicArrayType());

			// early out if you just plainly called it wrongly.
			if(target.size() != given.size() && !fvararg && firstOptionalArgument == -1)
			{
				return SimpleError::make(callLoc, "mismatched argument count; expected %d, but %d %s provided", target.size(), given.size(),
					given.size() == 1 ? "was" : "were");
			}

			// TODO: see how this works (if at all) in the presence of varaidic functions
			// solve the normal arguments first. here's how we handle optional arguments: `firstOptionalArgument` should contain the index
			// of the first optional argument (everything after that should also be optional!)
			// this entire function is name-agnostic, so we expect `given` to be in the same order as `target`

			// so, we just modify last_arg accordingly to stop before getting to the unfilled optional arguments.
			size_t last_arg = std::min(target.size() + (fvararg ? -1 : 0),
				std::max(given.size(), (firstOptionalArgument == -1 ? 0 : firstOptionalArgument - 1)));

			for(size_t i = 0; i < last_arg; i++)
			{
				auto err = solveSingleType(soln, target[i], given[i]);
				if(err != nullptr) return err;

				// possibly increase solution completion by re-substituting with new information
				soln->resubstituteIntoSolutions();
			}

			// solve the variadic part.
			if(fvararg)
			{
				// check for forwarding first.
				if(given.size() == target.size() && given.back()->isVariadicArrayType())
				{
					auto copy = *soln;

					// ok, if we fulfil all the conditions to forward, then we forward.
					auto err = solveSingleType(&copy, target.back(), given.back());
					if(err == nullptr)
					{
						iceAssert(copy.distance >= 0);
						*soln = copy;

						// ok, things should be solved, and we will forward.
						return nullptr;
					}
				}

				//* note: the reason we put this outside an 'else' is so that, in the event we're unable to solve
				//* for the forwarding case for whatever reason, we will treat it as an argument-passing case.

				// get the supposed type of the thing.
				auto varty = target.back()->toArraySliceType()->getArrayElementType();
				auto ltvarty = fir::LocatedType(varty, target.back().loc);

				for(size_t i = last_arg; i < given.size(); i++)
				{
					auto err = solveSingleType(soln, ltvarty, given[i]);
					if(err) return err->append(SimpleError::make(MsgType::Note, target.back().loc, "in argument of variadic parameter"));
				}

				// ok, everything should be good??
				return nullptr;
			}

			return nullptr;
		}








		std::pair<Solution_t, ErrorMsg*> solveTypeList(const Location& callLoc, const std::vector<fir::LocatedType>& target,
			const std::vector<fir::LocatedType>& given, const Solution_t& partial, bool isFnCall, size_t firstOptionalArgument)
		{
			Solution_t prevSoln = partial;

			std::vector<fir::PolyPlaceholderType*> tosolve;
			for(auto t : target)
				tosolve = tosolve + t->getContainedPlaceholders();

			auto checkFinished = [&tosolve](const Solution_t& soln) -> bool {
				for(auto t : tosolve)
				{
					if(!soln.hasSolution(t->getName()))
						return false;
				}

				return true;
			};

			while(true)
			{
				//* note!! we reset the distance here, because we will always loop through every argument.
				//* if we didn't reset the distance, it would just keep increasing to infinity (and overflow)
				auto soln = prevSoln; soln.distance = 0;

				auto errs = solveSingleTypeList(&soln, callLoc, target, given, isFnCall, firstOptionalArgument);
				if(errs) return { soln, errs };

				if(soln == prevSoln)            { break; }
				else if(checkFinished(soln))    { prevSoln = soln; break; }
				else                            { prevSoln = soln; }
			}


			for(auto& pair : prevSoln.solutions)
			{
				if(pair.second->isConstantNumberType())
					pair.second = fir::getBestFitTypeForConstant(pair.second->toConstantNumberType());
			}

			return { prevSoln, nullptr };
		}
	}
}
























