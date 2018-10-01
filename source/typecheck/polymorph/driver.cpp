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
		const std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial, fir::Type* infer, bool isFnCall)
	{
		Solution_t soln;
		for(const auto& p : partial)
			soln.addSolution(p.first, LocatedType(p.second));


		if(auto td = dcast(ast::TypeDefn, thing))
		{
			error("type not supported");
		}
		else if(auto fd = dcast(ast::FuncDefn, thing))
		{
			std::vector<LocatedType> given;
			std::vector<LocatedType> target;

			target = misc::unwrapFunctionCall(fs, fd, getNextSessionId(), /* includeReturn: */ !isFnCall || infer);

			if(isFnCall)
			{
				auto [ gvn, err ] = misc::unwrapArgumentList(fs, fd, _input);
				if(err.hasErrors()) return { soln, err };

				given = gvn;
				if(infer)
					given.push_back(LocatedType(infer));
			}
			else
			{
				if(infer == 0)
					return { soln, SimpleError::make(fs->loc(), "unable to infer type for reference to '%s'", fd->name) };

				else if(!infer->isFunctionType())
					return { soln, SimpleError::make(fs->loc(), "invalid type '%s' inferred for '%s'", infer, fd->name) };

				// ok, we should have it.
				iceAssert(infer->isFunctionType());
				given = util::mapidx(infer->toFunctionType()->getArgumentTypes(), [fd](fir::Type* t, size_t i) -> LocatedType {
					return LocatedType(t, fd->args[i].loc);
				}) + LocatedType(infer->toFunctionType()->getReturnType(), fd->loc);
			}


			auto [ soln, errs ] = solveTypeList(fs, target, given);
			for(auto& pair : soln.solutions)
			{
				if(pair.second->isConstantNumberType())
					pair.second = fir::getBestFitTypeForConstant(pair.second->toConstantNumberType());
			}

			return { soln, errs };
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





















