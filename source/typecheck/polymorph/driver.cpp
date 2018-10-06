// driver.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"

#include "ir/type.h"

#include "errors.h"
#include "typecheck.h"

#include "polymorph.h"

#include "resolver.h"

#include <set>

namespace sst {
namespace poly
{
	std::vector<std::pair<TCResult, Solution_t>> findPolymorphReferences(TypecheckState* fs, const std::string& name,
		const std::vector<ast::Parameterisable*>& gdefs, const TypeParamMap_t& _gmaps, fir::Type* return_infer, fir::Type* type_infer,
		bool isFnCall, std::vector<FnCallArgument>* args)
	{
		iceAssert(gdefs.size() > 0);

		//? now if we have multiple things then we need to try them all, which can get real slow real quick.
		//? unfortunately I see no better way to do this.
		// TODO: find a better way to do this??

		std::vector<std::pair<TCResult, Solution_t>> pots;

		for(const auto& gdef : gdefs)
		{
			pots.push_back(attemptToInstantiatePolymorph(fs, gdef, _gmaps, return_infer, type_infer, isFnCall, args,
				/* fillplaceholders: */ true));
		}

		return pots;
	}





	namespace internal
	{
		static std::pair<Solution_t, SimpleError> inferPolymorphicType(TypecheckState* fs, ast::TypeDefn* td, const ProblemSpace_t& problems,
			const std::vector<FnCallArgument>& input, const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall,
			std::unordered_map<std::string, size_t>* origParamOrder)
		{
			auto soln = Solution_t(partial);

			if(!isFnCall)
			{
				if(auto missing = getMissingSolutions(problems, soln.solutions, true); missing.size() > 0)
				{
					auto ret = solvePolymorphWithPlaceholders(fs, td, partial);
					return { ret.second, SimpleError() };
				}
				else
				{
					return { soln, SimpleError() };
				}
			}
			else if(auto str = dcast(ast::StructDefn, td))
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

				auto [ seen, err ] = resolver::verifyStructConstructorArguments(fs->loc(), str->name, fieldset,
					util::map(input, [](const FnCallArgument& fca) -> FunctionDecl::Param {
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

					target[idx] = fir::LocatedType(convertPtsType(fs, str->generics, str->fields[idx]->type, session), str->fields[idx]->loc);
					given[idx] = fir::LocatedType(input[s.second].value->type, input[s.second].loc);
				}

				*origParamOrder = fieldNames;
				return solveTypeList(target, given, soln, isFnCall);
			}
			else if(auto cls = dcast(ast::ClassDefn, td))
			{
				std::unordered_map<std::string, size_t> paramOrder;

				std::vector<std::pair<Solution_t, SimpleError>> rets;
				for(auto init : cls->initialisers)
					rets.push_back(inferTypesForPolymorph(fs, init, cls->generics, input, partial, return_infer, type_infer, isFnCall, &paramOrder));

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

				*origParamOrder = paramOrder;
				return best;
			}
			else
			{
				error("no");
			}
		}




		static std::pair<Solution_t, SimpleError> inferPolymorphicFunction(TypecheckState* fs, ast::Parameterisable* thing,
			const std::unordered_map<std::string, TypeConstraints_t>& problems, const std::vector<FnCallArgument>& input,
			const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall,
			std::unordered_map<std::string, size_t>* origParamOrder)
		{
			auto soln = Solution_t(partial);

			bool isinit = dcast(ast::InitFunctionDefn, thing) != nullptr;
			iceAssert(dcast(ast::FuncDefn, thing) || isinit);

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

			// if(!type_infer)
			{
				target = internal::unwrapFunctionParameters(fs, problems, args, session);

				if(!isinit && (!isFnCall || return_infer))
				{
					// add the return type to the fray
					target.push_back(fir::LocatedType(convertPtsType(fs, thing->generics, retty, session), thing->loc));
				}
			}
			// else
			// {
			// 	auto ift = type_infer->toFunctionType();

			// 	for(auto a : ift->getArgumentTypes())
			// 		target.push_back(fir::LocatedType(a));

			// 	if(!isFnCall || return_infer)
			// 		target.push_back(fir::LocatedType(ift->getReturnType()));
			// }


			if(isFnCall)
			{
				SimpleError err;
				auto gvn = resolver::misc::canonicaliseCallArguments(thing->loc, args,
					util::map(input, [](const FnCallArgument& fca) -> FunctionDecl::Param {
						return FunctionDecl::Param(fca);
					}), &err);

				if(err.hasErrors()) return { soln, err };

				given = gvn;
				if(return_infer)
					given.push_back(fir::LocatedType(return_infer));

				*origParamOrder = resolver::misc::getNameIndexMap(args);
			}
			else
			{
				if(type_infer == 0)
					return { soln, SimpleError() };

				else if(!type_infer->isFunctionType())
					return { soln, SimpleError::make(fs->loc(), "invalid type '%s' inferred for '%s'", type_infer, thing->name) };

				// ok, we should have it.
				iceAssert(type_infer->isFunctionType());
				given = util::mapidx(type_infer->toFunctionType()->getArgumentTypes(), [&args](fir::Type* t, size_t i) -> fir::LocatedType {
					return fir::LocatedType(t, args[i].loc);
				}) + fir::LocatedType(type_infer->toFunctionType()->getReturnType(), thing->loc);
			}

			return solveTypeList(target, given, soln, isFnCall);
		}






		std::pair<Solution_t, SimpleError> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing,
			const std::unordered_map<std::string, TypeConstraints_t>& problems, const std::vector<FnCallArgument>& input,
			const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall,
			std::unordered_map<std::string, size_t>* origParamOrder)
		{
			if(auto td = dcast(ast::TypeDefn, thing))
			{
				return inferPolymorphicType(fs, td, problems, input, partial, return_infer, type_infer, isFnCall, origParamOrder);
			}
			else if(dcast(ast::FuncDefn, thing) || dcast(ast::InitFunctionDefn, thing))
			{
				return inferPolymorphicFunction(fs, thing, problems, input, partial, return_infer, type_infer, isFnCall, origParamOrder);
			}
			else
			{
				return std::make_pair(Solution_t(partial),
					SimpleError(thing->loc, strprintf("Unable to infer type for unsupported entity '%s'", thing->getKind()))
				);
			}
		}
	}
}
}
































