// driver.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"

#include "ir/type.h"

#include "errors.h"
#include "typecheck.h"

#include "polymorph.h"
#include "polymorph_internal.h"

#include "resolver.h"

#include <set>

namespace sst {
namespace poly
{
	std::vector<std::pair<TCResult, Solution_t>> findPolymorphReferences(TypecheckState* fs, const std::string& name,
		const std::vector<ast::Parameterisable*>& gdefs, const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>* args)
	{
		iceAssert(gdefs.size() > 0);

		//? now if we have multiple things then we need to try them all, which can get real slow real quick.
		//? unfortunately I see no better way to do this.
		// TODO: find a better way to do this??

		std::vector<std::pair<TCResult, Solution_t>> pots;

		for(const auto& gdef : gdefs)
		{
			pots.push_back(attemptToInstantiatePolymorph(fs, gdef, _gmaps, /* infer: */ infer, /* isFnCall: */ isFnCall,
				/* args:  */ args, /* fillplaceholders: */ true));
		}

		return pots;
	}







	//* might not return a complete solution, and does not check that the solution it returns is complete at all.
	std::pair<Solution_t, SimpleError> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing,
		const std::unordered_map<std::string, TypeConstraints_t>& problems, const std::vector<FnCallArgument>& _input,
		const TypeParamMap_t& partial, fir::Type* infer, bool isFnCall)
	{
		Solution_t soln;
		for(const auto& p : partial)
			soln.addSolution(p.first, fir::LocatedType(p.second));


		if(auto td = dcast(ast::TypeDefn, thing))
		{
			if(!isFnCall)
			{
				if(auto missing = getMissingSolutions(problems, soln.solutions, true); missing.size() > 0)
				{
					auto ret = solvePolymorphWithPlaceholders(fs, thing, partial);
					return { ret.second, SimpleError() };
				}
				else
				{
					return { soln, SimpleError() };
				}
			}
			else if(auto str = dcast(ast::StructDefn, thing))
			{
				std::set<std::string> fieldset;
				std::unordered_map<std::string, size_t> fieldNames;
				{
					size_t i = 0;
					for(auto f : str->fields)
					{
						fieldset.insert(f->name);
						fieldNames[f->name] = i++;
					}
				}

				auto [ seen, err ] = fs->verifyStructConstructorArguments(str->name, fieldset,
					util::map(_input, [](const FnCallArgument& fca) -> FunctionDecl::Param {
						return FunctionDecl::Param(fca);
					}
				));

				if(err.hasErrors())
					return { soln, err };

				int session = getNextSessionId();
				std::vector<fir::LocatedType> given(seen.size());
				std::vector<fir::LocatedType> target(seen.size());

				for(const auto& s : seen)
				{
					auto idx = fieldNames[s.first];

					target[idx] = fir::LocatedType(misc::convertPtsType(fs, str->generics, str->fields[idx]->type, session), str->fields[idx]->loc);
					given[idx] = fir::LocatedType(_input[s.second].value->type, _input[s.second].loc);
				}

				return solveTypeList(target, given, soln, isFnCall);
			}
			else if(auto cls = dcast(ast::ClassDefn, thing))
			{
				std::vector<std::pair<Solution_t, SimpleError>> rets;
				for(auto init : cls->initialisers)
					rets.push_back(inferTypesForPolymorph(fs, init, cls->generics, _input, partial, infer, isFnCall));

				// check the distance of all the solutions... i guess?
				std::pair<Solution_t, SimpleError> best;
				best.first = soln;
				best.first.distance = INT_MAX;
				best.second = SimpleError::make(fs->loc(), "ambiguous reference to constructor of class '%s'", cls->name);

				for(const auto& r : rets)
				{
					if(r.first.distance < best.first.distance && !r.second.hasErrors())
						best = r;
				}

				return best;
			}
			else
			{
				error("no");
			}
		}
		else if(bool isinit = dcast(ast::InitFunctionDefn, thing); isinit || dcast(ast::FuncDefn, thing))
		{
			std::vector<fir::LocatedType> given;
			std::vector<fir::LocatedType> target;

			pts::Type* retty = 0;
			std::vector<ast::FuncDefn::Arg> args;

			if(isinit)
			{
				auto i = dcast(ast::InitFunctionDefn, thing);
				args = i->args;
			}
			else
			{
				auto i = dcast(ast::FuncDefn, thing);
				retty = i->returnType;
				args = i->args;
			}

			int session = getNextSessionId();
			target = misc::unwrapFunctionCall(fs, problems, args, session);

			if(!isinit && (!isFnCall || infer))
			{
				// add the return type to the fray
				target.push_back(fir::LocatedType(misc::convertPtsType(fs, thing->generics, retty, session), thing->loc));
			}

			if(isFnCall)
			{
				SimpleError err;
				auto gvn = resolver::canonicaliseCallArguments(thing->loc, args, _input, &err);
				if(err.hasErrors()) return { soln, err };

				given = gvn;
				if(infer)
					given.push_back(fir::LocatedType(infer));
			}
			else
			{
				if(infer == 0)
					return { soln, SimpleError::make(fs->loc(), "unable to infer type for reference to '%s'", thing->name) };

				else if(!infer->isFunctionType())
					return { soln, SimpleError::make(fs->loc(), "invalid type '%s' inferred for '%s'", infer, thing->name) };

				// ok, we should have it.
				iceAssert(infer->isFunctionType());
				given = util::mapidx(infer->toFunctionType()->getArgumentTypes(), [&args](fir::Type* t, size_t i) -> fir::LocatedType {
					return fir::LocatedType(t, args[i].loc);
				}) + fir::LocatedType(infer->toFunctionType()->getReturnType(), thing->loc);
			}

			return solveTypeList(target, given, soln, isFnCall);
		}
		else
		{
			return std::make_pair(soln,
				SimpleError(thing->loc, strprintf("Unable to infer type for unsupported entity '%s'", thing->getKind()))
			);
		}
	}
}
}
































