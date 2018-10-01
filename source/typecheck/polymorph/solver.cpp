// solver.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "sst.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include "polymorph.h"
#include "polymorph_internal.h"


namespace sst
{
	namespace poly
	{
		SimpleError solveSingleType(TypecheckState* fs, Solution_t* soln, const LocatedType& target, const LocatedType& given)
		{
			auto tgt = target.type;
			auto gvn = given.type;

			// if we're just looking at normal types, then just add the cost.
			if(!tgt->containsPlaceholders() && !gvn->containsPlaceholders())
			{
				int dist = fs->getCastDistance(gvn, tgt);
				if(dist >= 0)
				{
					soln->distance += dist;
					return SimpleError();
				}
				else
				{
					soln->distance = -1;
					return SimpleError::make(given.loc, "No valid cast from given type '%s' to target type '%s'", gvn, tgt);
				}
			}
			else
			{
				// limit decomposition of the given types by the number of transforms on the target type.
				auto [ tt, ttrfs ] = decomposeIntoTransforms(tgt, -1);
				auto [ gt, gtrfs ] = decomposeIntoTransforms(gvn, ttrfs.size());

				// substitute if possible.
				if(auto _gt = soln->substitute(gt); _gt != gt)
					gt = _gt;

				bool ttpoly = tt->isPolyPlaceholderType();
				bool gtpoly = gt->isPolyPlaceholderType();

				// check what kind of monster we're dealing with.
				if(tt->isPolyPlaceholderType())
				{
					// see if there's a match.
					auto ptt = tt->toPolyPlaceholderType();
					if(auto ltt = soln->getSolution(ptt->getName()); ltt.type != 0)
					{
						// check for conflict.
						if(!ltt.type->isPolyPlaceholderType() && !gtpoly)
						{
							if(ltt.type->isConstantNumberType() || gt->isConstantNumberType())
							{
								gt = misc::mergeNumberTypes(ltt.type, gt);
								if(gt != ltt.type)
									soln->addSolution(ptt->getName(), LocatedType(gt, given.loc));
							}
							else if(ltt.type != gt)
							{
								return SimpleError::make(given.loc, "Conflicting solutions for type parameter '%s': previous: '%s', current: '%s'",
									ptt->getName(), ltt.type, gvn);
							}
						}
						else if(ltt.type->isPolyPlaceholderType() && !gtpoly)
						{
							soln->addSubstitution(ltt.type, gt);
						}
						else if(!ltt.type->isPolyPlaceholderType() && gtpoly)
						{
							soln->addSubstitution(gt, ltt.type);
						}
						else if(ltt.type->isPolyPlaceholderType() && gtpoly)
						{
							error("what???? '%s' and '%s' are both poly??", ltt.type, gt);
						}
					}
					else
					{
						soln->addSolution(ptt->getName(), LocatedType(gt, given.loc));
					}
				}
				else if(tt->isFunctionType() || tt->isTupleType())
				{
					// make sure they're the same 'kind' of type first.
					if(tt->isFunctionType() != gt->isFunctionType() || tt->isTupleType() != gt->isTupleType())
						return SimpleError::make(given.loc, "no valid conversion from given type '%s' to target type '%s'", gt, tt);

					std::vector<LocatedType> problem;
					std::vector<LocatedType> input;
					if(gt->isFunctionType())
					{
						input = util::map(gt->toFunctionType()->getArgumentTypes(), [given](fir::Type* t) -> LocatedType {
							return LocatedType(t, given.loc);
						}) + LocatedType(gt->toFunctionType()->getReturnType(), given.loc);

						problem = util::map(tt->toFunctionType()->getArgumentTypes(), [target](fir::Type* t) -> LocatedType {
							return LocatedType(t, target.loc);
						}) + LocatedType(tt->toFunctionType()->getReturnType(), target.loc);
					}
					else
					{
						iceAssert(gt->isTupleType());
						input = util::map(gt->toTupleType()->getElements(), [given](fir::Type* t) -> LocatedType {
							return LocatedType(t, given.loc);
						});

						problem = util::map(tt->toTupleType()->getElements(), [target](fir::Type* t) -> LocatedType {
							return LocatedType(t, target.loc);
						});
					}

					return solveSingleTypeList(fs, soln, problem, input);
				}
				else
				{
					error("'%s' not supported", tt);
				}
			}

			return SimpleError();
		}

		SimpleError solveSingleTypeList(TypecheckState* fs, Solution_t* soln, const std::vector<LocatedType>& target, const std::vector<LocatedType>& given)
		{
			// for now just do this.
			if(target.size() != given.size())
				return SimpleError::make(fs->loc(), "mismatched argument count");

			for(size_t i = 0; i < std::min(target.size(), given.size()); i++)
			{
				auto err = solveSingleType(fs, soln, target[i], given[i]);
				if(err.hasErrors())
					return err;

				// possibly increase solution completion by re-substituting with new information
				soln->resubstituteIntoSolutions();
			}

			return SimpleError();
		}


		std::pair<Solution_t, SimpleError> solveTypeList(TypecheckState* fs, const std::vector<LocatedType>& target, const std::vector<LocatedType>& given)
		{
			Solution_t prevSoln;
			while(true)
			{
				auto soln = prevSoln;
				auto errs = solveSingleTypeList(fs, &soln, target, given);
				if(errs.hasErrors()) return { soln, errs };

				if(soln == prevSoln)    break;
				else                    prevSoln = soln;
			}

			return { prevSoln, SimpleError() };
		}
	}
}
























