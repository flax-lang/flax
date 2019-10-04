// driver.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"

#include "ir/type.h"

#include "errors.h"
#include "typecheck.h"

#include "resolver.h"
#include "polymorph.h"

#include "memorypool.h"

#include <set>

namespace sst {
namespace poly
{
	std::vector<PolyRefResult> findPolymorphReferences(TypecheckState* fs, const std::string& name, const std::vector<ast::Parameterisable*>& gdefs,
		const PolyArgMapping_t& pams, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall, std::vector<FnCallArgument>* args)
	{
		iceAssert(gdefs.size() > 0);

		//? now if we have multiple things then we need to try them all, which can get real slow real quick.
		//? unfortunately I see no better way to do this.
		// TODO: find a better way to do this??

		std::vector<PolyRefResult> pots;

		for(const auto& gdef : gdefs)
		{
			auto [ gmaps, err ] = resolver::misc::canonicalisePolyArguments(fs, gdef, pams);
			if(err != nullptr)
			{
				pots.push_back(PolyRefResult(TCResult(err), Solution_t(), gdef));
			}
			else
			{
				auto [ r, s ] = attemptToInstantiatePolymorph(fs, gdef, name, gmaps, return_infer, type_infer, isFnCall,
					args, /* fillplaceholders: */ true);

				pots.push_back(PolyRefResult(r, s, gdef));
			}
		}

		return pots;
	}





	namespace internal
	{
		static std::pair<Solution_t, ErrorMsg*> inferPolymorphicType(TypecheckState* fs, ast::TypeDefn* td, const std::string& name,
			const ProblemSpace_t& problems, const std::vector<FnCallArgument>& input, const TypeParamMap_t& partial, fir::Type* return_infer,
			fir::Type* type_infer, bool isFnCall, fir::Type* problem_infer, util::hash_map<std::string, size_t>* origParamOrder)
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
				util::hash_map<std::string, size_t> paramOrder;

				auto inputcopy = input;
				auto selfty = fir::PolyPlaceholderType::get(util::obfuscateName("self_infer"), getNextSessionId());
				inputcopy.insert(inputcopy.begin(), FnCallArgument(fs->loc(), "", util::pool<sst::RawValueExpr>(fs->loc(),
					selfty->getMutablePointerTo()), 0));

				fs->pushSelfContext(selfty);
				defer(fs->popSelfContext());



				std::vector<std::pair<Solution_t, ErrorMsg*>> rets;
				for(auto init : cls->initialisers)
				{
					rets.push_back(inferTypesForPolymorph(fs, init, name, cls->generics, inputcopy, partial, return_infer, type_infer, isFnCall,
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

				std::vector<std::string> fields;
				util::hash_map<std::string, size_t> fieldNames;
				{
					size_t i = 0;
					for(auto f : str->fields)
					{
						auto nm = std::get<0>(f);

						fields.push_back(nm);
						fieldNames[nm] = i++;
					}
				}

				auto [ seen, err ] = resolver::verifyStructConstructorArguments(fs->loc(), str->name, fields, input);
				if(err) return { soln, err };

				int session = getNextSessionId();
				std::vector<ArgType> given(seen.size());
				std::vector<ArgType> target(seen.size());

				for(const auto& s : seen)
				{
					auto idx = fieldNames[s.first];

					target[idx] = ArgType(std::get<0>(str->fields[idx]), convertPtsType(fs, str->generics, std::get<2>(str->fields[idx]), session),
						std::get<1>(str->fields[idx]));

					given[idx] = ArgType(input[s.second].name, input[s.second].value->type, input[s.second].loc);
				}

				*origParamOrder = fieldNames;
				return solveTypeList(fs->loc(), target, given, soln, isFnCall);
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
				std::vector<ArgType> target;
				if(vty->isTupleType())
				{
					for(auto t : vty->toTupleType()->getElements())
						target.push_back(ArgType("", t, uvloc));
				}
				else if(!vty->isVoidType())
				{
					target.push_back(ArgType("", vty, uvloc));
				}

				auto given = util::map(input, [](const FnCallArgument& fca) -> ArgType {
					return ArgType(fca.name, fca.value->type, fca.loc);
				});

				return solveTypeList(fs->loc(), target, given, soln, isFnCall);
			}
			else
			{
				error("no");
			}
		}




		static std::pair<Solution_t, ErrorMsg*> inferPolymorphicFunction(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
			const ProblemSpace_t& problems, const std::vector<FnCallArgument>& input,
			const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall, fir::Type* problem_infer,
			util::hash_map<std::string, size_t>* origParamOrder)
		{
			auto soln = Solution_t(partial);

			bool isinit = dcast(ast::InitFunctionDefn, thing) != nullptr;
			iceAssert(dcast(ast::FuncDefn, thing) || isinit);

			std::vector<ArgType> given;
			std::vector<ArgType> target;

			pts::Type* retty = 0;
			std::vector<ast::FuncDefn::Param> params;

			if(isinit)
			{
				auto i = dcast(ast::InitFunctionDefn, thing);
				params = i->params;
			}
			else
			{
				auto i = dcast(ast::FuncDefn, thing);
				retty = i->returnType;
				params = i->params;
			}

			int session = getNextSessionId();

			if(!problem_infer)
			{
				target = internal::unwrapFunctionParameters(fs, problems, params, session);

				if(!isinit && (!isFnCall || return_infer))
				{
					// add the return type to the fray. it doesn't have a name tho
					target.push_back(ArgType("<return_type>", convertPtsType(fs, thing->generics, retty, session), thing->loc));
				}
			}
			else
			{
				auto ift = problem_infer->toFunctionType();

				util::foreachIdx(ift->getArgumentTypes(), [&target, &params](fir::Type* ty, size_t i) {
					target.push_back(ArgType(params[i].name, ty, params[i].loc));
				});

				if(!isFnCall || return_infer)
					target.push_back(ArgType("<return_type>", ift->getReturnType(), Location()));
			}


			if(isFnCall)
			{
				given = util::map(input, [](const FnCallArgument& a) -> poly::ArgType {
					return ArgType(a.name, a.value->type, a.loc, /* opt: */ false, /* ignoreName: */ a.ignoreName);
				});

				if(return_infer)
					given.push_back(ArgType("<return_type>", return_infer, Location()));

				*origParamOrder = resolver::misc::getNameIndexMap(params);
			}
			else
			{
				if(type_infer == 0)
					return { soln, nullptr };

				else if(!type_infer->isFunctionType())
					return { soln, SimpleError::make(fs->loc(), "invalid type '%s' inferred for '%s'", type_infer, thing->name) };

				// ok, we should have it.
				iceAssert(type_infer->isFunctionType());
				given = util::mapidx(type_infer->toFunctionType()->getArgumentTypes(), [&params](fir::Type* t, size_t i) -> ArgType {
					return ArgType(params[i].name, t, params[i].loc);
				}) + ArgType("<return_type>", type_infer->toFunctionType()->getReturnType(), thing->loc);
			}

			return solveTypeList(fs->loc(), target, given, soln, isFnCall);
		}






		std::pair<Solution_t, ErrorMsg*> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
			const ProblemSpace_t& problems, const std::vector<FnCallArgument>& input,
			const TypeParamMap_t& partial, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall,
			fir::Type* problem_infer, util::hash_map<std::string, size_t>* origParamOrder)
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
































