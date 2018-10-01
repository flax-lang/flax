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
			soln.addSolution(p.first, LocatedType(p.second));


		if(auto td = dcast(ast::TypeDefn, thing))
		{
			if(!isFnCall)
			{
				if(auto missing = getMissingSolutions(problems, soln.solutions, false); missing.size() > 0)
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
				std::vector<LocatedType> given(seen.size());
				std::vector<LocatedType> target(seen.size());

				for(const auto& s : seen)
				{
					auto idx = fieldNames[s.first];

					target[idx] = LocatedType(misc::convertPtsType(fs, str->generics, str->fields[idx]->type, session), str->fields[idx]->loc);
					given[idx] = LocatedType(_input[s.second].value->type, _input[s.second].loc);
				}

				return solveTypeList(fs, target, given, soln);
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
			std::vector<LocatedType> given;
			std::vector<LocatedType> target;

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
				target.push_back(LocatedType(misc::convertPtsType(fs, thing->generics, retty, session), thing->loc));
			}

			if(isFnCall)
			{
				auto [ gvn, err ] = misc::unwrapArgumentList(fs, thing, args, _input);
				if(err.hasErrors()) return { soln, err };

				given = gvn;
				if(infer)
					given.push_back(LocatedType(infer));
			}
			else
			{
				if(infer == 0)
					return { soln, SimpleError::make(fs->loc(), "unable to infer type for reference to '%s'", thing->name) };

				else if(!infer->isFunctionType())
					return { soln, SimpleError::make(fs->loc(), "invalid type '%s' inferred for '%s'", infer, thing->name) };

				// ok, we should have it.
				iceAssert(infer->isFunctionType());
				given = util::mapidx(infer->toFunctionType()->getArgumentTypes(), [&args](fir::Type* t, size_t i) -> LocatedType {
					return LocatedType(t, args[i].loc);
				}) + LocatedType(infer->toFunctionType()->getReturnType(), thing->loc);
			}

			return solveTypeList(fs, target, given, soln);
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





















#if 0
{
		if(!isFnCall)
		{
			// fill it with placeholders instead, i guess??

			return { this->getPlaceholderSolutions(_target, partial), { }, SimpleError() };
			// return { partial, { }, SimpleError::make(td, "Invalid use of type '%s' in non-constructor-call context", td->name) };
		}

		if(auto cld = dcast(ast::ClassDefn, td))
		{
			std::vector<std::tuple<ast::InitFunctionDefn*, std::vector<Param_t>, std::vector<UnsolvedType>, SimpleError>> solns;
			for(auto init : cld->initialisers)
			{
				auto [ input, problem, errs ] = _internal_unwrapFunctionCall(this, init, init->args, 0, _input, true, infer, partial);

				if(!errs.hasErrors())
					solns.push_back({ init, input, problem, errs });
			}

			// TODO: FIXME: better error messages
			if(solns.empty())
				return { partial, { }, SimpleError::make(td, "No matching constructor to type '%s'", td->name) };

			else if(solns.size() > 1)
				return { partial, { }, SimpleError::make(td, "Ambiguous call to constructor of type '%s'", td->name) };

			// ok, we're good??
			// just return the solution, i guess.
			auto [ init, input, problem, errs ] = solns[0];
			return _internal_inferTypesForGenericEntity(this, problemSpace, problem, input, partial, infer);
		}
		else if(auto str = dcast(ast::StructDefn, td))
		{
			// this is a special case. we need to tailor the 'problem' to the 'input', instead of the other way around.
			// first we need to check that whatever arguments we're passing do exist in the struct.

			// TODO: the stuff below is pretty much a dupe of the stuff we do in typecheck/call.cpp for struct constructors.
			//! combine?

			std::unordered_map<std::string, size_t> fieldNames;
			{
				size_t i = 0;
				for(auto f : str->fields)
					fieldNames[f->name] = i++;
			}

			bool useNames = false;
			bool firstName = true;

			size_t ctr = 0;
			std::unordered_map<std::string, size_t> seenNames;
			for(auto arg : _input)
			{
				if((arg.name.empty() && useNames) || (!arg.name.empty() && !useNames && !firstName))
				{
					return { partial, { },
						SimpleError::make(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor")
					};
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
					firstName = false;
				}
				else if(useNames && fieldNames.find(arg.name) == fieldNames.end())
				{
					return { partial, { },
						SimpleError::make(arg.loc, "Field '%s' does not exist in struct '%s'", arg.name, str->name)
					};
				}
				else if(useNames && seenNames.find(arg.name) != seenNames.end())
				{
					return { partial, { },
						SimpleError::make(arg.loc, "Duplicate argument for field '%s' in constructor call to struct '%s'",
						arg.name, str->name)
					};
				}

				seenNames[arg.name] = ctr++;
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames && _input.size() != fieldNames.size() && _input.size() > 0)
			{
				return { partial, { },
					SimpleError::make(this->loc(), "Mismatched number of arguments in constructor call to type '%s'; expected %d arguments, found %d arguments instead", str->name, fieldNames.size(), _input.size())
						.append(BareError("All arguments are mandatory when using positional arguments", MsgType::Note))
				};
			}

			// preallocate for both problem and input.
			input.resize(seenNames.size());
			problem.resize(seenNames.size());

			for(const auto& seen : seenNames)
			{
				auto idx = fieldNames[seen.first];

				problem[idx] = (UnsolvedType(str->fields[idx]->loc, str->fields[idx]->type));
				input[idx] = Param_t(_input[seen.second]);
			}

			return _internal_inferTypesForGenericEntity(this, problemSpace, problem, input, partial, infer);
		}
		else
		{
			error(td, "no.");
		}
	}


#endif













