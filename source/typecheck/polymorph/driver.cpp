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
			pots.push_back(attemptToInstantiatePolymorph(fs, gdef, name, _gmaps, return_infer, type_infer, isFnCall, args,
				/* fillplaceholders: */ true));
		}

		return pots;
	}





	namespace internal
	{
		static std::pair<Solution_t, ErrorMsg*> inferPolymorphicType(TypecheckState* fs, ast::TypeDefn* td, const std::string& name,
			const ProblemSpace_t& problems, const std::vector<FnCallArgument>& input, const TypeParamMap_t& partial, fir::Type* return_infer,
			fir::Type* type_infer, bool isFnCall, fir::Type* problem_infer, std::unordered_map<std::string, size_t>* origParamOrder)
		{
			auto soln = Solution_t(partial);

			if(!isFnCall && !type_infer)
			{
				if(auto missing = getMissingSolutions(problems, soln.solutions, true); missing.size() > 0)
				{
					auto ret = solvePolymorphWithPlaceholders(fs, td, name, partial);
					return { ret.second, nullptr };
				}
				else
				{
					return { soln, nullptr };
				}
			}
			else if(auto cls = dcast(ast::ClassDefn, td))
			{
				std::unordered_map<std::string, size_t> paramOrder;

				std::vector<std::pair<Solution_t, ErrorMsg*>> rets;
				for(auto init : cls->initialisers)
				{
					rets.push_back(inferTypesForPolymorph(fs, init, name, cls->generics, input, partial, return_infer, type_infer, isFnCall,
						problem_infer, &paramOrder));
				}

				// check the distance of all the solutions... i guess?
				std::pair<Solution_t, ErrorMsg*> best;
				best.first = soln;
				best.first.distance = INT_MAX;
				best.second = SimpleError::make(fs->loc(), "ambiguous reference to constructor of class '%s'", cls->name);

				for(const auto& r : rets)
				{
					if(r.first.distance < best.first.distance && r.second == nullptr)
						best = r;
				}

				*origParamOrder = paramOrder;
				return best;
			}
			else if(auto str = dcast(ast::StructDefn, td))
			{
				//* for constructors, we look like a function call, so type_infer is actually return_infer.
				if(!type_infer && return_infer)
				{
					//* so what we do here is, if we have type_infer, we lookup the sst::Defn, and use it to find the matching genericVersion
					//* in the ast::Defn, and extract the solution map, then recursively call inferPolymorphicType with our new solution.
					std::swap(type_infer, return_infer);
					if(auto gen_str = fs->typeDefnMap[type_infer])
					{
						for(const auto& v : str->genericVersions)
						{
							if(v.first == gen_str)
							{
								return inferPolymorphicType(fs, str, name, problems, input, v.second.back(), /* return_infer: */ nullptr,
									/* type_infer: */ nullptr, isFnCall, problem_infer, origParamOrder);
							}
						}
					}
				}

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


				auto [ seen, err ] = resolver::verifyStructConstructorArguments(fs->loc(), str->name, fieldset, input);
				if(err) return { soln, err };


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
			else if(auto unn = dcast(ast::UnionDefn, td))
			{
				//* see the comment above -- same thing about infer, except we look for type_infer because for unions we know to pass it there
				//* (since we don't really look like a function call most of the time)
				if(type_infer)
				{
					if(auto gen_str = fs->typeDefnMap[type_infer])
					{
						for(const auto& v : unn->genericVersions)
						{
							if(v.first == gen_str)
							{
								return inferPolymorphicType(fs, unn, name, problems, input, v.second.back(), /* return_infer: */ nullptr,
									/* type_infer: */ nullptr, isFnCall, problem_infer, origParamOrder);
							}
						}
					}
				}

				if(unn->cases.find(name) == unn->cases.end())
					error("no variant named '%s'", name);

				auto uvloc = std::get<1>(unn->cases[name]);

				int session = getNextSessionId();
				fir::Type* vty = convertPtsType(fs, unn->generics, std::get<2>(unn->cases[name]), session);

				// ok, then. check the type + arguments.
				std::vector<fir::LocatedType> target;
				if(vty->isTupleType())
				{
					for(auto t : vty->toTupleType()->getElements())
						target.push_back(fir::LocatedType(t, uvloc));
				}
				else if(!vty->isVoidType())
				{
					target.push_back(fir::LocatedType(vty, uvloc));
				}

				auto given = util::map(input, [](const FnCallArgument& fca) -> fir::LocatedType {
					return fir::LocatedType(fca.value->type, fca.loc);
				});

				return solveTypeList(target, given, soln, isFnCall);
			}
			else
			{
				error("no");
			}
		}




		static std::pair<Solution_t, ErrorMsg*> inferPolymorphicFunction(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
			const std::unordered_map<std::string, TypeConstraints_t>& problems, const std::vector<FnCallArgument>& input,
			const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall, fir::Type* problem_infer,
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
			if(!problem_infer)
			{
				target = internal::unwrapFunctionParameters(fs, problems, args, session);

				if(!isinit && (!isFnCall || return_infer))
				{
					// add the return type to the fray
					target.push_back(fir::LocatedType(convertPtsType(fs, thing->generics, retty, session), thing->loc));
				}
			}
			else
			{
				auto ift = problem_infer->toFunctionType();

				for(auto a : ift->getArgumentTypes())
					target.push_back(fir::LocatedType(a));

				if(!isFnCall || return_infer)
					target.push_back(fir::LocatedType(ift->getReturnType()));
			}


			if(isFnCall)
			{
				ErrorMsg* err = 0;

				auto gvn = resolver::misc::canonicaliseCallArguments(thing->loc, args, input, &err);
				if(err) return { soln, err };

				given = gvn;
				if(return_infer)
					given.push_back(fir::LocatedType(return_infer));

				*origParamOrder = resolver::misc::getNameIndexMap(args);
			}
			else
			{
				if(type_infer == 0)
					return { soln, nullptr };

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






		std::pair<Solution_t, ErrorMsg*> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
			const std::unordered_map<std::string, TypeConstraints_t>& problems, const std::vector<FnCallArgument>& input,
			const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall,
			fir::Type* problem_infer, std::unordered_map<std::string, size_t>* origParamOrder)
		{
			if(auto td = dcast(ast::TypeDefn, thing))
			{
				return inferPolymorphicType(fs, td, name, problems, input, partial, return_infer, type_infer, isFnCall, problem_infer, origParamOrder);
			}
			else if(dcast(ast::FuncDefn, thing) || dcast(ast::InitFunctionDefn, thing))
			{
				return inferPolymorphicFunction(fs, thing, name, problems, input, partial, return_infer, type_infer, isFnCall, problem_infer, origParamOrder);
			}
			else
			{
				return std::make_pair(Solution_t(partial),
					SimpleError::make(thing->loc, "unable to infer type for unsupported entity '%s'", thing->getKind())
				);
			}
		}
	}
}
}
































