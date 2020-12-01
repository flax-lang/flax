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
		static ErrorMsg* solveSingleTypeList(Solution_t* soln, const Location& callLoc, const std::vector<ArgType>& target,
			const std::vector<ArgType>& given, bool isFnCall);

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
				auto [ tt, ttrfs ] = internal::decomposeIntoTransforms(tgt, static_cast<size_t>(-1));
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

					std::vector<ArgType> problem;
					std::vector<ArgType> input;
					if(gt->isFunctionType())
					{
						input = zfu::map(gt->toFunctionType()->getArgumentTypes(), [given](fir::Type* t) -> ArgType {
							return ArgType("", t, given.loc);
						}) + ArgType("", gt->toFunctionType()->getReturnType(), given.loc);

						problem = zfu::map(tt->toFunctionType()->getArgumentTypes(), [target](fir::Type* t) -> ArgType {
							return ArgType("", t, target.loc);
						}) + ArgType("", tt->toFunctionType()->getReturnType(), target.loc);
					}
					else
					{
						iceAssert(gt->isTupleType());
						input = zfu::map(gt->toTupleType()->getElements(), [given](fir::Type* t) -> ArgType {
							return ArgType("", t, given.loc);
						});

						problem = zfu::map(tt->toTupleType()->getElements(), [target](fir::Type* t) -> ArgType {
							return ArgType("", t, target.loc);
						});
					}

					// for recursive solving, we're never a function call.
					// related: so, firstOptionalArgument is always -1 in these cases.
					return solveSingleTypeList(soln, given.loc, problem, input, /* isFnCall: */ false);
				}
				else
				{
					error("'%s' not supported", tt);
				}
			}

			return nullptr;
		}



		static ErrorMsg* solveSingleTypeList(Solution_t* soln, const Location& callLoc, const std::vector<ArgType>& target,
			const std::vector<ArgType>& given, bool isFnCall)
		{
			bool fvararg = (isFnCall && target.size() > 0 && target.back()->isVariadicArrayType());

			util::hash_map<std::string, size_t> targetnames;

			// either we have all names or no names for the target!
			if(target.size() > 0 && target[0].name != "")
			{
				zfu::foreachIdx(target, [&targetnames](const ArgType& t, size_t i) {
					targetnames[t.name] = i;
				});
			}

			std::set<size_t> unsolvedtargets;
			zfu::foreachIdx(target, [&unsolvedtargets, &target, fvararg](const ArgType& a, size_t i) {
				// if it's optional, we don't mark it as 'unsolved'.
				if((!fvararg || i + 1 != target.size()) && !a.optional)
					unsolvedtargets.insert(i);
			});

			// record which optionals we passed, for a better error message.
			std::set<std::string> providedOptionals;


			size_t last_arg = std::min(target.size() + (fvararg ? -1 : 0), given.size());

			// we used to do this check in the parser, but to support more edge cases (like passing varargs)
			// we moved it here so we can actually check stuff.
			bool didNames = false;

			size_t positionalCounter = 0;
			size_t varArgStart = last_arg;
			for(size_t i = 0; i < last_arg; i++)
			{
				const ArgType* targ = 0;

				// note that when we call std::set::erase(), if the key did not exist (because it was optional),
				// nothing will happen, which is fine.
				if(targetnames.size() > 0 && given[i].name != "")
				{
					if(auto it = targetnames.find(given[i].name); it != targetnames.end())
					{
						targ = &target[it->second];
						unsolvedtargets.erase(it->second);
					}
					else
					{
						return SimpleError::make(given[i].loc, "function has no parameter named '%s'", given[i].name);
					}

					/*
						optional arguments "don't count" as passing by name. this means that you can do this, for example:

						foo(x: 30, "blabla")

						where 'x' is an optional argument. this should be fine in terms of the rest of the compiler, because when
						we *declare* the function, all optional arguments must come last. this gives us a good compromise because
						optional arguments must still be passed by name, but we can actually have optional arguments together with
						variadic functions without needing to name all the arguments.
					*/

					if(!targ->optional)
					{
						if(!given[i].ignoreName)
							didNames = true;

						positionalCounter++;
					}
					else
					{
						providedOptionals.insert(given[i].name);
					}
				}
				else
				{
					/*
						we didn't pass a name. if the function is variadic, we might have wanted to pass the following argument(s)
						variadically. so, instead of assuming we made a mistake (like not passing the optional by name), assume we
						wanted to pass it to the vararg.

						so, `positionalCounter` counts the paramters on the declaration-side. thus, once we encounter a default value,
						it must mean that the rest of the parameters will be optional as well.

						* ie. we've passed all the positional arguments already, leaving the optional ones, which means every argument from
						* here onwards (including this one) must be named. since this is *not* named, we just skip straight to the varargs if
						* it was present.
					*/

					targ = &target[positionalCounter];

					if(fvararg && targ->optional)
					{
						varArgStart = i;
						break;
					}

					unsolvedtargets.erase(positionalCounter);
					positionalCounter++;
				}

				/*
					TODO: not sure if there's a way to get around this, but if we have a function like this:

					fn foo(a: int, b: int, c: int, x: int = 9, y: int = 8, z: int = 7) { ... }

					then calling it wrongly like this: foo(x: 4, 1, 2, 5, z: 6, 3)

					results in an error at the last argument ('3') saying taht optional argument 'x' must be passed by name.
					the problem is that we can't really tell what argument you wanted to pass; after seeing '1', '2', and '5',
					the positionalCounter now points to the 4th argument, 'x'.

					even though you already passed x prior, we don't really know that? and we assume you wanted to pass x (again)
				*/

				if(given[i].name.empty())
				{
					if(didNames)
						return SimpleError::make(given[i].loc, "positional arguments cannot appear after named arguments");

					else if(targ->optional)
					{
						std::string probablyIntendedArgumentName;
						for(const auto& a : target)
						{
							if(!a.optional)
								continue;

							if(auto it = providedOptionals.find(a.name); it == providedOptionals.end())
							{
								probablyIntendedArgumentName = a.name;
								break;
							}
						};

						if(probablyIntendedArgumentName.empty())
						{
							//* this shouldn't happen, because we only get here if we're not variadic, but if we weren't
							//* variadic, then we would've errored out if the argument count was wrong to begin with.
							return SimpleError::make(given[i].loc, "extraneous argument without corresponding parameter");
						}
						else
						{
							return SimpleError::make(given[i].loc, "optional argument '%s' must be passed by name",
								probablyIntendedArgumentName);
						}
					}
				}

				iceAssert(targ);
				auto err = solveSingleType(soln, targ->toFLT(), given[i].toFLT());
				if(err != nullptr) return err;

				// possibly increase solution completion by re-substituting with new information
				soln->resubstituteIntoSolutions();
			}

			if(unsolvedtargets.size() > 0 || (!fvararg && given.size() > target.size()))
			{
				if(targetnames.empty() || given.size() > target.size())
				{
					return SimpleError::make(callLoc, "expected %d %s, but %d %s provided",
						target.size(), zfu::plural("argument", target.size()), given.size(), given.size() == 1 ? "was" : "were");
				}
				else
				{
					std::vector<std::string> missings;
					for(const auto& us : unsolvedtargets)
						missings.push_back(target[us].name);

					auto s = util::listToEnglish(missings, /* quote: */ true);
					return SimpleError::make(callLoc, "missing %s for %s %s", zfu::plural("argument", missings.size()),
						zfu::plural("parameter", missings.size()), s);
				}
			}



			// solve the variadic part.
			if(fvararg)
			{
				// check for forwarding first.
				if(given.size() == target.size() && given.back()->isVariadicArrayType())
				{
					auto copy = *soln;

					// ok, if we fulfil all the conditions to forward, then we forward.
					auto err = solveSingleType(&copy, target.back().toFLT(), given.back().toFLT());
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

				for(size_t i = varArgStart; i < given.size(); i++)
				{
					auto err = solveSingleType(soln, ltvarty, given[i].toFLT());
					if(err) return err->append(SimpleError::make(MsgType::Note, target.back().loc, "in argument of variadic parameter"));
				}

				// ok, everything should be good??
				return nullptr;
			}

			return nullptr;
		}








		std::pair<Solution_t, ErrorMsg*> solveTypeList(const Location& callLoc, const std::vector<ArgType>& target,
			const std::vector<ArgType>& given, const Solution_t& partial, bool isFnCall)
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

				auto errs = solveSingleTypeList(&soln, callLoc, target, given, isFnCall);
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
























