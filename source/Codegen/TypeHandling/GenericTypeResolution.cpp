// GenericTypeResolution.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <set>

#include "pts.h"
#include "ast.h"
#include "codegen.h"

using namespace Ast;

namespace Codegen
{
	static void _getAllGenericTypesContainedWithinRecursively(pts::Type* t, const std::map<std::string, TypeConstraints_t>& gt,
		std::set<std::string>* list)
	{
		if(t->isNamedType())
		{
			if(gt.find(t->toNamedType()->name) != gt.end())
				list->insert(t->toNamedType()->name);
		}
		else if(t->isFunctionType())
		{
			for(auto p : t->toFunctionType()->argTypes)
				_getAllGenericTypesContainedWithinRecursively(p, gt, list);

			_getAllGenericTypesContainedWithinRecursively(t->toFunctionType()->returnType, gt, list);
		}
		else if(t->isTupleType())
		{
			for(auto m : t->toTupleType()->types)
				_getAllGenericTypesContainedWithinRecursively(m, gt, list);
		}
		else if(t->isPointerType())
		{
			while(t->isPointerType())
				t = t->toPointerType()->base;

			_getAllGenericTypesContainedWithinRecursively(t, gt, list);
		}
		else if(t->isDynamicArrayType())
		{
			_getAllGenericTypesContainedWithinRecursively(t->toDynamicArrayType()->base, gt, list);
		}
		else if(t->isFixedArrayType())
		{
			_getAllGenericTypesContainedWithinRecursively(t->toFixedArrayType()->base, gt, list);
		}
		else if(t->isVariadicArrayType())
		{
			_getAllGenericTypesContainedWithinRecursively(t->toVariadicArrayType()->base, gt, list);
		}
		else if(t->isArraySliceType())
		{
			_getAllGenericTypesContainedWithinRecursively(t->toArraySliceType()->base, gt, list);
		}
		else
		{
			iceAssert("??" && 0);
		}
	}



	static std::set<std::string> getAllGenericTypesContainedWithin(pts::Type* t, const std::map<std::string, TypeConstraints_t>& gt)
	{
		std::set<std::string> ret;
		_getAllGenericTypesContainedWithinRecursively(t, gt, &ret);

		return ret;
	}


	/*

		note: using basic intuition and without solid proof, i believe that
		unify(unify(unify(a, b), c), d) == unify(a, b, c, d)

		hence there is no need to overcomplicate the solution to handle multiple simultaneous solutions
		we simply attempt to unify with the existing solution if there is one, iteratively.

		this should yield the final type to be a proper solution in the end.
	*/
	static fir::Type* unifyTypeSolutions(fir::Type* a, fir::Type* b)
	{
		// todo: when we get typeclasses (protocols), actually make this find the best common type
		// for now, we just compare a == b.

		// 1. if they're equal, just return them
		if(a == b) return a;


		return 0;
	}








	// twelve arguments wtf
	static bool checkFunctionOrTupleArgumentToGenericFunction(CodegenInstance* cgi, FuncDecl* candidate, std::set<std::string> toSolve,
		size_t ix, pts::Type* prm, fir::Type* arg, std::map<std::string, fir::Type*>* resolved, std::map<std::string, fir::Type*>* fnSoln,
		std::string* errorString, Expr** failedExpr, bool isVariadic, bool returnIncomplete);



	static bool solveSingleArgument(CodegenInstance* cgi, FuncDecl* candidate, size_t ix, std::set<std::string> toSolve,
		fir::Type* _baseArg, size_t argIndex, pts::Type* _expected, fir::Type* _given, std::map<std::string, fir::Type*>* topLevelSolution,
		std::map<std::string, fir::Type*>* fnSoln, std::string* errorString, Expr** failedExpr)
	{
		using TrfList = std::vector<pts::TypeTransformer>;

		if(toSolve.empty())
			return true;


		// check the given with the expected
		fir::Type* givent = 0; TrfList giventrfs;
		std::tie(givent, giventrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(_given);

		pts::Type* expt = 0; TrfList exptrfs;
		std::tie(expt, exptrfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(_expected);


		// the second part is to *ensure* they're compatible -- just because the transformations are compatible,
		// doesn't mean the types are; i64 and (i64, i64) have 0 transformations, but aren't compatible
		if(!pts::areTransformationsCompatible(exptrfs, giventrfs))
		{
			if(errorString && failedExpr)
			{
				*errorString = strprintf("Incompatible types in solution for argument %zu of function parameter"
					" (which is argument %zu of parent function): expected '%s', have '%s' (No valid transformations)",
					argIndex + 1, ix + 1, expt->str().c_str(), _baseArg->str().c_str());

				if(candidate) *failedExpr = candidate->params[ix];
			}
			return false;
		}

		// fix it
		givent = pts::reduceMaximallyWithSubset(_given, exptrfs, giventrfs);


		auto checkNeedsSolving = [cgi, toSolve](std::string s) -> bool {
			return toSolve.find(s) != toSolve.end();
		};

		auto checkFinishedSolving = [toSolve](const std::map<std::string, fir::Type*>& cursln) -> bool {
			for(auto s : toSolve)
			{
				if(cursln.find(s) == cursln.end())
					return false;
			}

			return true;
		};


		auto& cursln = *topLevelSolution;
		auto& genericfnsoln = *fnSoln;

		/*
			time for some more ranting/pseudo/rubberduck

			primary objective is to solve for the top-level parameters, not the inner parameters.
			inner parameters are only useful when recursing into deeper function-as-parameter madness.

			during codegen, once the top level parameters are figured out, all the subsequent deeper types can all be
			figured out with 0 cost. Hence the priority is to solve the top level.


			first, `givent` is the 'given' type, ie. the type of the parameter being passed in. may or may not be polymorphic.
			`expt` is the 'expected' type, ie. the type of the argument in the function.

			expected and given here refer to the caller/callee argument relationship; 'given' is what is passed, and 'expected' is,
			naturally, what is expected.

			so, expt is the upper (upper != top, since we could be several layers deep), and given is the current level.
			note that both, either, or neither of these can be parametric types.

			pseudo-ducking:

			1. is `expt` -> T is a parametric type? if so:
				a. if there is an existing solution:
					- get the solution -> actualType(X)
					- is `givent` a parametric type -> S?
						• no -- check if actualType(X) == actualType(S). if not, error out.
						• yes -- is there a solution for S in the inner solution set for the corresponding `givent` -> Q?
							- yes -- check if actualType(X) == Q. If not, error out.
							- no -- insert the solution, Q = actualType(X)

				b. if there is no existing solution:
					- is `givent` a parametric type -> S?
						• no -- insert the solution, T = actualType(S)
						• yes -- is there a solution for S in the inner solution set for the corresponding `givent` -> Q?
							- yes -- insert the solution, T = Q
							- no -- do nothing, since we probably don't have enough information to proceed.

			2. if `expt` is not a parametric type, is it a normal type -> actualType(T)? if so:
				a. is `givent` a parametric type -> S?
					- yes -- check if there's a solution -> solution(S) for `givent`
						• yes -- check that actualType(T) == solution(S). if not, error out.
						• no -- insert the solution, S = actualType(T)

					- no -- check that actualType(T) == actualType(S). if not, we have a regular type mismatch; error out.

			3. `expt` should either be a tuple type or a function type at this state.
				a. recurse into this function.
		*/


		// 1 & 2 above
		if(expt->isNamedType())
		{
			if(checkNeedsSolving(expt->toNamedType()->name))
			{
				// 1a: get solution
				if(cursln.find(expt->toNamedType()->name) != cursln.end())
				{
					fir::Type* sln = cursln[expt->toNamedType()->name];
					if(givent->isParametricType())
					{
						// check if there's a solution for it
						if(genericfnsoln.find(givent->toParametricType()->getName()) != genericfnsoln.end())
						{
							fir::Type* givensln = genericfnsoln[givent->toParametricType()->getName()];

							// check these two are the same
							if(unifyTypeSolutions(sln, givensln) == 0)
							{
								*errorString = strprintf("Conflicting solutions for argument %zu (in argument %zu of parent function); solution for parent parametric type '%s' was '%s', but solution for child parametric type '%s' was '%s'",
									argIndex + 1, ix + 1, expt->toNamedType()->name.c_str(), sln->str().c_str(), givent->str().c_str(),
									givensln->str().c_str());

								if(candidate) *failedExpr = candidate->params[ix];
								return false;
							}

							// ok.
						}
						else
						{
							// no solution, insert.
							genericfnsoln[givent->toParametricType()->getName()] = sln;
						}
					}
					else
					{
						if(unifyTypeSolutions(sln, givent) == 0)
						{
							*errorString = strprintf("Conflicting solutions for argument %zu (in argument %zu of parent function); solution for parent parametric type '%s' was '%s', given expression had type '%s'", argIndex + 1, ix + 1,
								expt->toNamedType()->name.c_str(), sln->str().c_str(), givent->str().c_str());

							if(candidate) *failedExpr = candidate->params[ix];
							return false;
						}

						// ok.
					}
				}
				else
				{
					// no solution (1b)
					if(givent->isParametricType())
					{
						// check if we have a solution
						if(genericfnsoln.find(givent->toParametricType()->getName()) != genericfnsoln.end())
						{
							fir::Type* givensln = genericfnsoln[givent->toParametricType()->getName()];

							// insert new solution
							cursln[expt->toNamedType()->name] = givensln;
						}
						else
						{
							// do nothing, lacking information.
						}
					}
					else
					{
						// insert new solution
						cursln[expt->toNamedType()->name] = givent;
					}
				}
			}
			else
			{
				// expt is a normal type (no need to solve, and is named)
				// so resolve it

				fir::Type* exptype = cgi->getTypeFromParserType(candidate, expt);
				iceAssert(exptype);

				// 2.
				if(givent->isParametricType())
				{
					if(genericfnsoln.find(givent->toParametricType()->getName()) != genericfnsoln.end())
					{
						fir::Type* givensln = genericfnsoln[givent->toParametricType()->getName()];

						if(unifyTypeSolutions(givensln, exptype) == 0)
						{
							*errorString = strprintf("Conflicting solutions for argument %zu (in argument %zu of parent function); parent type was concrete '%s', given expression (for parameter '%s') had type '%s'", argIndex + 1, ix + 1,
								exptype->str().c_str(), givent->toParametricType()->getName().c_str(), givensln->str().c_str());

							if(candidate) *failedExpr = candidate->params[ix];
							return false;
						}
					}
					else
					{
						// add the solution here
						genericfnsoln[givent->toParametricType()->getName()] = exptype;
					}
				}
				else
				{
					// check if they match up
					if(unifyTypeSolutions(exptype, givent) == 0)
					{
						*errorString = strprintf("Conflicting types for argument %zu (in argument %zu of parent function); have '%s', found '%s'", argIndex + 1, ix + 1, exptype->str().c_str(), givent->str().c_str());

						if(candidate) *failedExpr = candidate->params[ix];
						return false;
					}
				}
			}
		}
		else
		{
			iceAssert(expt->isFunctionType() || expt->isTupleType());
			iceAssert(givent->isFunctionType() || givent->isTupleType());

			iceAssert(expt->isFunctionType() == givent->isFunctionType());

			/*
				basically, call this function recursively
				this caused some pain to mentally figure out
				basically, we check "expt" against "givent" -- expected vs given.
				the expected type in this case is the main one.

				eg:
				(type parameters in each function are made unique)
				(E, U/V, T/K/R)

				func foo<U, V>(x: U, f: [(U) -> V]) -> V { ... }
				func bar<E>(x: E) -> E { ... }
				func qux<T, K, R>(arr: T[], fn: [(T) -> K], fa: [(T, [(T) -> K]) -> R]) -> R[] { ... }

				qux([ 1, 2, 3 ], bar, foo)


				in the above, calling 'qux' immediately we know 'T = i64'.
				then, we resolve 'fn' as bar. since we know 'T = i64', and 'T = E', and 'K = E', then 'K = i64'.

				this leaves 'R' as the only unknown type.

				then, we start resolving 'fa'.
				all is well at first -- we know 'T = i64', and 'U = T'. hence 'U = i64'.

				now, the next parameter is a function type, so we enter this area.
				at this stage,

				cursln :: [ T = i64, K = i64 ]
				genericfnsoln :: [ U = i64 ]

				so, we need to find out the type of 'V', and since 'V = R', this gives us 'R'.

				the second map (named 'fnsoln' in the parameter list of this function) is the so-called 'inner solution set'.
				this is the partial solution set of the inner, recursed function -- in this case 'foo'.

				expt is type fa -- using 'T', 'K', and 'R'.
				givent is foo -- using 'U' and 'V'.

				referencing the actual code above,
				we check if 'cursln' contains a solution for 'exptN',
				and if so whether 'genericfnsoln' contains a solution for 'giventN'.

				'exptN' and 'giventN' are the types of the individual arguments in the function type,
				in the case above, expt0 and givent0 are 'T' and 'U' in the base case,
				'T' and 'U' in the inner case as well (ie. in resolving the type of 'f' in 'foo')


				so it goes:

				... stuff above -- finding 'T' and 'K' through the array argument and 'bar'


				resolving [(U) -> V] (given) against [(T) -> K] (exp)

				does 'cursln' have a solution for 'T'?
				yes, yes it does.
				does 'genericfnsoln' have a solution for 'U'?
				yes, yes it does. do they conflict? no, good.


				does 'cursln' have a solution for 'K'?
				yes, yes it does.
				does 'genericfnsoln' have a solution for 'V'?
				no, it does not. K = V, so V = solution of K (i64).

				now we know both 'U' and 'V', and can return.


				returned:
				resolving 'foo' against 'fa'

				does 'cursln' have a solution for 'R'?
				no, it does not.
				does 'genericfnsoln' have a solution for 'V'?
				yes, yes it does. R = V, so R = i64.

				T, K, and R are solved == exit.
			*/

			// if we're done solving, don't bother
			if(!checkFinishedSolving(cursln))
			{
				// return incomplete solutions, so we don't exit too early when the first argument can't be solved
				return checkFunctionOrTupleArgumentToGenericFunction(cgi, candidate, toSolve, 0, expt, givent, &cursln, &genericfnsoln,
					errorString, failedExpr, givent->isFunctionType() ? givent->toFunctionType()->isVariadicFunc() : false, true);
			}
		}

		return true;
	}







	// iterative (better) solver
	// accepts a parameter, returns whether a solution was found (bool)

	static bool checkFunctionOrTupleArgumentToGenericFunction(CodegenInstance* cgi, FuncDecl* candidate, std::set<std::string> toSolve,
		size_t ix, pts::Type* prm, fir::Type* arg, std::map<std::string, fir::Type*>* resolved,
		std::map<std::string, fir::Type*>* fnSoln, std::string* errorString, Expr** failedExpr, bool isVariadic, bool returnIncomplete)
	{
		typedef std::vector<pts::TypeTransformer> TrfList;

		if(toSolve.empty())
			return true;

		// decompose each type fully
		pts::Type* dpt = 0; TrfList ptrfs;
		std::tie(dpt, ptrfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(prm);

		fir::Type* dft = 0; TrfList ftrfs;
		std::tie(dft, ftrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(arg);

		iceAssert(dpt->isFunctionType() || dpt->isTupleType());
		if(!pts::areTransformationsCompatible(ptrfs, ftrfs))
		{
			if(errorString && failedExpr)
			{
				*errorString = strprintf("Incompatible types in argument %zu: expected '%s', have '%s' (No valid transformations)",
					ix + 1, prm->str().c_str(), arg->str().c_str());

				if(candidate) *failedExpr = candidate->params[ix];
			}

			return false;
		}
		else if((!dft->isFunctionType() && !dft->isTupleType()) || (dft->isFunctionType() && !dpt->isFunctionType())
			|| (dft->isTupleType() && !dpt->isTupleType()))
		{
			if(errorString && failedExpr)
			{
				*errorString = strprintf("Incompatible types in solution for argument %zu:"
					" expected %s type '%s', have '%s'", ix + 1, dpt->isFunctionType() ? "function" : "tuple",
					prm->str().c_str(), arg->str().c_str());

				if(candidate) *failedExpr = candidate->params[ix];
			}

			return false;
		}



		// ok, the types are compatible
		// look at arguments

		// note: should you be able to pass a function (T, K[...]) -> void to an argument taking (T) -> void?
		// it shouldn't make a difference, but would probably be unexpected.

		std::vector<fir::Type*> ftlist;
		std::vector<pts::Type*> ptlist;
		if(dft->isFunctionType())
		{
			iceAssert(dft->isFunctionType() && dpt->isFunctionType());

			ftlist = dft->toFunctionType()->getArgumentTypes();
			ptlist = dpt->toFunctionType()->argTypes;
		}
		else
		{
			iceAssert(dft->isTupleType() && dpt->isTupleType());
			for(auto t : dft->toTupleType()->getElements())
				ftlist.push_back(t);

			ptlist = dpt->toTupleType()->types;
		}

		if(ftlist.size() != ptlist.size() && !isVariadic)
		{
			if(errorString && failedExpr)
			{
				*errorString = strprintf("Incompatible (function or tuple) type lists in argument %zu:"
					" Size mismatch, have %zu, expected %zu", ix + 1, ftlist.size(), ptlist.size());

				if(candidate) *failedExpr = candidate->params[ix];
			}
			return false;
		}


		auto checkNeedsSolving = [cgi, toSolve](std::string s) -> bool {
			return toSolve.find(s) != toSolve.end();
		};

		auto checkFinishedSolving = [toSolve](const std::map<std::string, fir::Type*>& cursln) -> bool {
			for(auto s : toSolve)
			{
				if(cursln.find(s) == cursln.end())
					return false;
			}

			return true;
		};


		// infinite loop
		for(size_t cnt = 0;; cnt++)
		{
			// save a local copy of the current soluion -- this is the "global" or "top level" solution set
			auto cursln = *resolved;

			// this is the solution set for the inner function, which is distinct from the top-level solution set
			// (due to name collisions and stuff; 'T' in the inner function might not correspond to the same type 'T' in the top level
			// solution)
			std::map<std::string, fir::Type*>& genericfnsoln = *fnSoln;

			size_t maxArg = MIN(ftlist.size(), ptlist.size());

			if(isVariadic)
			{
				iceAssert(ptlist.size() > 0);
				iceAssert(ftlist.size() >= ptlist.size());

				// don't touch the last one, which is the variable array part.
				maxArg -= 1;
			}



			// ok, now... loop through each item
			for(size_t i = 0; i < maxArg; i++)
			{
				auto argToSolve = getAllGenericTypesContainedWithin(ptlist[i], candidate->genericTypes);
				bool res = solveSingleArgument(cgi, candidate, ix, argToSolve, arg, i, ptlist[i], ftlist[i],
					&cursln, &genericfnsoln, errorString, failedExpr);

				if(!res) return false;
			}


			if(isVariadic)
			{
				// checks should have already been done by checkGenericFunction about minimum argument count
				iceAssert(ftlist.size() >= ptlist.size());

				pts::Type* baseVarType = ptlist.back();
				iceAssert(baseVarType->isVariadicArrayType());
				baseVarType = baseVarType->toVariadicArrayType()->base;

				auto argToSolve = getAllGenericTypesContainedWithin(baseVarType, candidate->genericTypes);



				// check direct forwarding
				if(ftlist.size() == ptlist.size() && ftlist.back()->isDynamicArrayType())
				{
					fir::DynamicArrayType* dat = ftlist.back()->toDynamicArrayType();

					// check the element type
					bool res = solveSingleArgument(cgi, candidate, ix, argToSolve, arg, maxArg, baseVarType, dat->getElementType(),
						&cursln, &genericfnsoln, errorString, failedExpr);

					if(!res) return false;
				}
				else
				{
					for(size_t k = maxArg; k < ftlist.size(); k++)
					{
						bool res = solveSingleArgument(cgi, candidate, ix, argToSolve, arg, k, baseVarType, ftlist[k],
							&cursln, &genericfnsoln, errorString, failedExpr);

						if(!res) return false;
					}
				}
			}







			// try the return type if this is a function
			if(dft->isFunctionType())
			{
				iceAssert(dpt->isFunctionType());

				// check the given with the expected
				fir::Type* frt = 0; TrfList fttrfs;
				std::tie(frt, fttrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(dft->toFunctionType()->getReturnType());

				pts::Type* prt = 0; TrfList pttrfs;
				std::tie(prt, pttrfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(dpt->toFunctionType()->returnType);

				if(!pts::areTransformationsCompatible(pttrfs, fttrfs))
				{
					if(errorString && failedExpr)
					{
						*errorString = strprintf("Incompatible types in solution for return type of function parameter"
							" (which is argument %zu of parent function): expected '%s', have '%s' (No valid transformations)",
							ix + 1, prt->str().c_str(), frt->str().c_str());

						if(candidate) *failedExpr = candidate->params[ix];
					}
					return false;
				}

				// fix it
				frt = pts::reduceMaximallyWithSubset(dft->toFunctionType()->getReturnType(), pttrfs, fttrfs);


				// check if we don't already have a solution for 'prt'.
				if(prt->isNamedType())
				{
					// no solution for 'prt'
					bool needsolve = checkNeedsSolving(prt->toNamedType()->name);

					if(needsolve)
					{
						if(cursln.find(prt->toNamedType()->name) == cursln.end())
						{
							if(frt->isParametricType())
							{
								if(genericfnsoln.find(frt->toParametricType()->getName()) != genericfnsoln.end())
								{
									fir::Type* found = genericfnsoln[frt->toParametricType()->getName()];
									cursln[prt->toNamedType()->name] = found;

									// debuglog("add soln (4) %s = %s -> %s\n", prt->toNamedType()->name.c_str(),
									// 	frt->toParametricType()->getName().c_str(), found->str().c_str());
								}
								else
								{
									// can't do anything.
								}
							}
							else if(frt->isTupleType() || frt->isFunctionType())
							{
								// again should not be the case, since the transformations matched and 'prt' is a named type
								error("somehow transformations matched? (3) (%s / %s)", frt->str().c_str(), prt->str().c_str());
							}
							else
							{
								cursln[prt->toNamedType()->name] = frt;

								// debuglog("add soln (5) %s -> %s\n", prt->toNamedType()->name.c_str(),
								// 	frt->str().c_str());
							}
						}
						else if(frt->isParametricType())
						{
							// we have a solution
							// update the generic function thing

							fir::Type* rest = cursln[prt->toNamedType()->name];
							iceAssert(rest);

							if(genericfnsoln.find(frt->toParametricType()->getName()) != genericfnsoln.end())
							{
								fir::Type* found = genericfnsoln[frt->toParametricType()->getName()];

								if(unifyTypeSolutions(rest, found) == 0)
								{
									if(errorString && failedExpr)
									{
										*errorString = strprintf("Conflicting types in solution for type parameter '%s', in return type of function parameter (which is argument %zu of parent function): have existing solution '%s', found '%s'", prt->toNamedType()->name.c_str(), ix + 1, rest->str().c_str(), found->str().c_str());

										if(candidate) *failedExpr = candidate->params[ix];
									}

									return false;
								}

								iceAssert(unifyTypeSolutions(cursln[prt->toNamedType()->name], found));

								// debuglog("add soln (6) %s = %s -> %s\n", prt->toNamedType()->name.c_str(),
								// 	frt->toParametricType()->getName().c_str(), found->str().c_str());
							}
							else
							{
								genericfnsoln[frt->toParametricType()->getName()] = rest;
								// debuglog("add soln (7) %s -> %s\n", frt->toParametricType()->getName().c_str(), rest->str().c_str());
							}
						}
					}
				}
				else
				{
					iceAssert(prt->isFunctionType() || prt->isTupleType());
					iceAssert(frt->isFunctionType() || frt->isTupleType());

					// same as the one above

					// if we're done solving, don't bother
					if(!checkFinishedSolving(cursln))
					{
						checkFunctionOrTupleArgumentToGenericFunction(cgi, candidate, toSolve, ix, prt, frt, &cursln, &genericfnsoln,
							errorString, failedExpr, frt->isFunctionType() ? frt->toFunctionType()->isVariadicFunc() : false,
							returnIncomplete);
					}
				}
			}




			/*
				the sets of code above check the non-function/non-tuple types of the function arguments.
				it basically skips anything complex -- this is to allow for maximum resolution, aka getting
				as much of the easily obtainable information as possible first.
				now that we have that, we can attempt solving all the function bits.

				for instance, this allows a function to be like this:
				func foo<T, K, U, R>(a: T, b: [(U) -> K], c: K[], d: U[]) -> R
				ie. the types of U and K *will* be known (eventually), but not at the point where we encounter the function
				argument.

				of course, we can just use the function to always try to resolve, and ignore its return value, and let the
				iterative solver solve it... iteratively.

				ok fuck it, that's what i'm going to do.

				TODO: OPTIMISE THIS FUNCTION (according to notes above)
			*/





			// if our local copy matches the resolved copy, we made no progress this time.
			if(*resolved == cursln)
			{
				if(!checkFinishedSolving(cursln))
				{
					if(returnIncomplete)
						return true;

					if(errorString && failedExpr)
					{
						std::string solvedstr; // = "\nSolutions found so far: ";

						for(auto t : cursln)
							solvedstr += strprintf("    %s = '%s'\n", t.first.c_str(), t.second->str().c_str());

						if(solvedstr.empty())
						{
							solvedstr = " (no partial solutions found)";
						}
						else
						{
							solvedstr = COLOUR_BLACK_BOLD "\n\nPartial solutions:\n" COLOUR_RESET + solvedstr;

							// get missing
							std::string missingstr;
							for(auto s : toSolve)
							{
								if(cursln.find(s) == cursln.end())
									missingstr += strprintf("    '%s'\n", s.c_str());
							}

							solvedstr += "\n" COLOUR_BLACK_BOLD "Missing solutions:\n" COLOUR_RESET + missingstr;
						}

						*errorString = strprintf("Failed to find solution for function parameter %zu in parent function using"
							" provided type '%s' to solve '%s'; made no progress after %zu iteration%s, and terminated.%s",
							ix, arg->str().c_str(), prm->str().c_str(), cnt + 1, cnt == 0 ? "" : "s", solvedstr.c_str());

						if(candidate) *failedExpr = candidate->params[ix];
					}

					return false;
				}

				*resolved = cursln;
				return true;
			}

			*resolved = cursln;
			if(checkFinishedSolving(cursln))
				return true;
		}

		return true;
	}


































	// main solver function
	static bool checkGenericFunction(CodegenInstance* cgi, std::map<std::string, fir::Type*>* gtm,
		FuncDecl* candidate, std::vector<fir::Type*> args, std::string* errorString, Expr** failedExpr)
	{
		auto prof = prof::Profile("generics");

		iceAssert(gtm);
		iceAssert(candidate);

		if(candidate->params.size() != args.size())
		{
			// if it's not variadic, and it's either a normal function (no parent class) or is a static method,
			// then there's no reason for the parameters to mismatch.
			if(!candidate->isVariadic && (!candidate->parentClass.first || candidate->isStatic))
			{
				if(errorString && failedExpr)
				{
					*errorString = strprintf("Mismatched argument count; expected %zu, have %zu",
						candidate->params.size(), args.size());

					*failedExpr = candidate;
				}
				return false;
			}
			else if(candidate->parentClass.first && !candidate->isStatic)
			{
				// make sure it's only one off
				if(args.size() < candidate->params.size() || args.size() - candidate->params.size() > 1)
				{
					if(errorString && failedExpr)
					{
						*errorString = strprintf("Mismatched argument count; expected %zu, have %zu",
							candidate->params.size(), args.size());

						*failedExpr = candidate;
					}
					return false;
				}

				// didSelfParam = true;
				iceAssert(args.front()->isPointerType() && args.front()->getPointerElementType()->isClassType() && "what, no.");

				// originalFirstParam = args.front();
				args.erase(args.begin());

				iceAssert(candidate->params.size() == args.size());
			}
		}


		// if the candidate is variadic, the number of parameters must be *at least* the number of fixed parameters
		if(candidate->isVariadic)
		{
			if(args.size() < candidate->params.size() - 1)
			{
				if(errorString && failedExpr)
				{
					*errorString = strprintf("Mismatched argument count; expected at least %zu, have %zu",
						candidate->params.size() - 1, args.size());

					*failedExpr = candidate;
				}
				return false;
			}
		}






		// always use the iterative solver
		{
			std::vector<pts::Type*> pFunctionParamTypes;

			for(auto p : candidate->params)
				pFunctionParamTypes.push_back(p->ptype);


			std::string es; Expr* fe = 0; std::map<std::string, fir::Type*> empty;
			std::map<std::string, fir::Type*> solns;

			pts::TupleType* pFnType = new pts::TupleType(pFunctionParamTypes);
			fir::TupleType* fFnType = fir::TupleType::get(args);

			auto tosolve = getAllGenericTypesContainedWithin(pFnType, candidate->genericTypes);

			bool res = checkFunctionOrTupleArgumentToGenericFunction(cgi, candidate, tosolve, 0, pFnType,
				fFnType, &solns, &empty, &es, &fe, candidate->isVariadic, false);

			if(!res)
			{
				if(errorString) *errorString = es;
				if(failedExpr) *failedExpr = fe;

				return false;
			}

			*gtm = solns;
		}


		// last phase: ensure the type constraints are met
		for(auto cst : *gtm)
		{
			TypeConstraints_t constr = candidate->genericTypes[cst.first];

			for(auto protstr : constr.protocols)
			{
				ProtocolDef* prot = cgi->resolveProtocolName(candidate, protstr);
				iceAssert(prot);

				bool doesConform = prot->checkTypeConformity(cgi, cst.second);

				if(!doesConform)
				{
					if(errorString && failedExpr)
					{
						*errorString = strprintf("Solution for parametric type '%s' ('%s') does not conform to protocol '%s'",
							cst.first.c_str(), cst.second->str().c_str(), protstr.c_str());

						*failedExpr = candidate;
					}
					return false;
				}
			}
		}

		return true;
	}









	FuncDefPair CodegenInstance::instantiateGenericFunctionUsingMapping(Expr* user, std::map<std::string, fir::Type*> gtm,
		Func* func, std::string* err, Expr** ex)
	{
		iceAssert(func);
		iceAssert(func->decl);

		FuncDecl* fnDecl = func->decl;

		bool needToCodegen = true;
		if(this->reifiedGenericFunctions.find({ func, gtm }) != this->reifiedGenericFunctions.end())
			needToCodegen = false;

		// we need to push a new "generic type stack", and add the types that we resolved into it.
		// todo: might be inefficient.
		// todo: look into creating a version of pushGenericTypeStack that accepts a std::map<string, fir::Type*>
		// so we don't have to iterate etc etc.
		// I don't want to access cgi->instantiatedGenericTypeStack directly.

		fir::Function* ffunc = nullptr;
		if(needToCodegen)
		{
			Result_t res = fnDecl->generateDeclForGenericFunction(this, gtm);
			ffunc = (fir::Function*) res.value;

			this->reifiedGenericFunctions[{ func, gtm }] = ffunc;
		}
		else
		{
			ffunc = this->reifiedGenericFunctions[{ func, gtm }];
			iceAssert(ffunc);
		}

		iceAssert(ffunc);

		this->pushGenericTypeStack();
		for(auto pair : gtm)
			this->pushGenericType(pair.first, pair.second);

		if(needToCodegen)
		{
			// dirty: use 'lhsPtr' to pass the version we want.
			auto s = this->saveAndClearScope();
			func->codegen(this, ffunc);
			this->restoreScope(s);
		}

		this->removeFunctionFromScope(FuncDefPair(0, func->decl, func));
		this->popGenericTypeStack();

		return FuncDefPair(ffunc, func->decl, func);
	}

	FuncDefPair CodegenInstance::instantiateGenericFunctionUsingParameters(Expr* user, Func* func, std::vector<fir::Type*> params,
		std::string* err, Ast::Expr** ex)
	{
		iceAssert(func);
		iceAssert(func->decl);

		std::map<std::string, fir::Type*> gtm;
		{
			bool res = checkGenericFunction(this, &gtm, func->decl, params, err, ex);
			if(!res) return FuncDefPair::empty();
		}

		return this->instantiateGenericFunctionUsingMapping(user, gtm, func, err, ex);
	}



	FuncDefPair CodegenInstance::tryResolveGenericFunctionCallUsingCandidates(FuncCall* fc, std::vector<Func*> candidates,
		std::map<Func*, std::pair<std::string, Expr*>>* errs)
	{
		// try and resolve shit
		std::map<std::string, fir::Type*> gtm;

		if(candidates.size() == 0)
		{
			return FuncDefPair::empty();	// just fail
		}

		std::vector<fir::Type*> fargs;
		for(auto p : fc->params)
			fargs.push_back(p->getType(this));

		auto it = candidates.begin();
		while(it != candidates.end())
		{
			std::string s; Expr* e = 0;
			bool result = checkGenericFunction(this, &gtm, (*it)->decl, fargs, &s, &e);

			if(!result)
			{
				if(errs) (*errs)[*it] = { s, e };
				it = candidates.erase(it);
			}
			else
			{
				it++;
			}
		}

		if(candidates.size() == 0)
		{
			return FuncDefPair::empty();
		}
		else if(candidates.size() > 1)
		{
			std::string cands;
			for(auto c : candidates)
				cands += this->printAst(c->decl) + "\n";

			error(fc, "Ambiguous instantiation of parametric function %s, have %zd candidates:\n%s\n", fc->name.c_str(),
				candidates.size(), cands.c_str());
		}

		// we know gtm isn't empty, and we only set the errors if we need to verify
		// so we can safely ignore them here.
		std::string _; Expr* __ = 0;
		return this->instantiateGenericFunctionUsingMapping(fc, gtm, candidates[0], &_, &__);
	}

	FuncDefPair CodegenInstance::tryResolveGenericFunctionCall(FuncCall* fc, std::map<Func*, std::pair<std::string, Expr*>>* errs)
	{
		std::vector<Func*> candidates = this->findGenericFunctions(fc->name);
		return this->tryResolveGenericFunctionCallUsingCandidates(fc, candidates, errs);
	}


	FuncDefPair CodegenInstance::tryResolveGenericFunctionFromCandidatesUsingFunctionType(Expr* user, std::vector<Func*> candidates,
		fir::FunctionType* ft, std::map<Func*, std::pair<std::string, Expr*>>* errs)
	{
		std::vector<FuncDefPair> ret;
		for(auto fn : candidates)
		{
			std::string s; Expr* e = 0;
			auto fp = this->instantiateGenericFunctionUsingParameters(user, fn, ft->getArgumentTypes(), &s, &e);

			if(fp.firFunc && fp.funcDef)
				ret.push_back(fp);

			else if(errs)
				(*errs)[fn] = { s, e };
		}

		if(ret.empty())
		{
			return FuncDefPair::empty();
		}
		else if(candidates.size() > 1)
		{
			std::string cands;
			for(auto c : candidates)
				cands += this->printAst(c->decl) + "\n";

			error(user, "Ambiguous instantiation of parametric function %s, have %zd candidates:\n%s\n",
				ret.front().funcDecl->ident.name.c_str(), candidates.size(), cands.c_str());
		}

		return ret.front();
	}





	fir::Function* CodegenInstance::instantiateGenericFunctionUsingValueAndType(Expr* user, fir::Function* oldfn, fir::FunctionType* oldft,
		fir::FunctionType* ft, MemberAccess* ma)
	{
		iceAssert(ft && ft->isFunctionType());
		if(ft->toFunctionType()->isGenericFunction())
		{
			error(user, "Unable to infer the instantiation of parametric function (type '%s'); explicit type specifier must be given",
				ft->str().c_str());
		}
		else
		{
			// concretised function is *not* generic.
			// hooray.

			fir::Function* fn = 0;
			std::map<Func*, std::pair<std::string, Expr*>> errs;
			if(ma)
			{
				auto fp = this->resolveAndInstantiateGenericFunctionReference(user, oldft, ft->toFunctionType(), ma, &errs);
				fn = fp;
			}
			else
			{
				if(!oldfn)
					error(user, "Could not resolve generic function??");

				auto fp = this->tryResolveGenericFunctionFromCandidatesUsingFunctionType(user,
					this->findGenericFunctions(oldfn->getName().name), ft->toFunctionType(), &errs);

				fn = fp.firFunc;
			}

			if(fn == 0)
			{
				exitless_error(user, "Invalid instantiation of parametric function of type '%s' with type '%s'",
					oldft->str().c_str(), ft->str().c_str());

				if(errs.size() > 0)
				{
					for(auto p : errs)
		 				info(p.first, "Candidate not suitable: %s", p.second.first.c_str());
				}

				doTheExit();
			}
			else
			{
				// return new history
				return fn;
			}
		}
	}


}




namespace fir
{
	std::string mangleGenericTypes(std::map<std::string, fir::Type*> tm)
	{
		std::vector<std::string> strs;

		for(auto arg : tm)
			strs.push_back(arg.first + ":" + arg.second->encodedStr());

		std::string ret;
		for(auto s : strs)
			ret += "_" + s;

		return ret;
	}
}




