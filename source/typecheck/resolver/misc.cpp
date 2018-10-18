// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "resolver.h"
#include "typecheck.h"

#include "ir/type.h"

namespace sst
{
	namespace resolver
	{
		namespace misc
		{
			static std::vector<fir::LocatedType> _canonicaliseCallArguments(const Location& target,
				const std::unordered_map<std::string, size_t>& nameToIndex, const std::vector<FnCallArgument>& args, SimpleError* err)
			{
				std::vector<fir::LocatedType> ret(args.size());

				// strip out the name information, and do purely positional things.
				{
					int counter = 0;
					for(const auto& i : args)
					{
						if(!i.name.empty())
						{
							if(nameToIndex.find(i.name) == nameToIndex.end())
							{
								iceAssert(err);
								*err = SimpleError::make(MsgType::Note, i.loc, "function does not have a parameter named '%s'",
									i.name).append(SimpleError::make(MsgType::Note, target, "function was defined here:"));

								return { };
							}
							else if(ret[nameToIndex.find(i.name)->second].type != 0)
							{
								iceAssert(err);
								*err = SimpleError::make(MsgType::Note, i.loc, "argument '%s' was already provided", i.name).append(
									SimpleError::make(MsgType::Note, ret[nameToIndex.find(i.name)->second].loc, "here:"));

								return { };
							}
						}

						ret[i.name.empty() ? counter : nameToIndex.find(i.name)->second] = fir::LocatedType(i.value->type, i.loc);
						counter++;
					}
				}

				return ret;
			}



			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<FnParam>& params,
				const std::vector<FnCallArgument>& args, SimpleError* err)
			{
				return _canonicaliseCallArguments(target, getNameIndexMap(params), args, err);
			}

			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
				const std::vector<FnCallArgument>& args, SimpleError* err)
			{
				return _canonicaliseCallArguments(target, getNameIndexMap(params), args, err);
			}



			std::vector<FnCallArgument> typecheckCallArguments(TypecheckState* fs, const std::vector<std::pair<std::string, ast::Expr*>>& args)
			{
				return util::map(args, [fs](const auto& a) -> FnCallArgument {
					return FnCallArgument(a.second->loc, a.first, a.second->typecheck(fs).expr(), a.second);
				});
			}
		}


















		std::pair<std::unordered_map<std::string, size_t>, SimpleError> verifyStructConstructorArguments(const Location& callLoc,
			const std::string& name, const std::set<std::string>& fieldNames, const std::vector<FnCallArgument>& arguments)
		{
			// ok, structs get named arguments, and no un-named arguments.
			// we just loop through each argument, ensure that (1) every arg has a name; (2) every name exists in the struct

			//* note that structs don't have inline member initialisers, so there's no trouble with this approach (in the codegeneration)
			//* of inserting missing arguments as just '0' or whatever their default value is

			//* in the case of classes, they will have inline initialisers, so the constructor calling must handle such things.
			//* but then class constructors are handled like regular functions, so it should be fine.

			bool useNames = false;
			bool firstName = true;

			size_t ctr = 0;
			std::unordered_map<std::string, size_t> seenNames;
			for(auto arg : arguments)
			{
				if(arg.name.empty() && useNames)
				{
					return { { }, SimpleError::make(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor") };
				}
				else if(firstName && !arg.name.empty())
				{
					useNames = true;
					firstName = false;
				}
				else if(!arg.name.empty() && !useNames && !firstName)
				{
					return { { }, SimpleError::make(arg.loc, "Named arguments cannot be mixed with positional arguments in a struct constructor") };
				}
				else if(useNames && fieldNames.find(arg.name) == fieldNames.end())
				{
					return { { }, SimpleError::make(arg.loc, "Field '%s' does not exist in struct '%s'", arg.name, name) };
				}
				else if(useNames && seenNames.find(arg.name) != seenNames.end())
				{
					return { { }, SimpleError::make(arg.loc, "Duplicate argument for field '%s' in constructor call to struct '%s'",
						arg.name, name) };
				}

				seenNames[arg.name] = ctr;
				ctr += 1;
			}

			//* note: if we're doing positional args, allow only all or none.
			if(!useNames && arguments.size() != fieldNames.size() && arguments.size() > 0)
			{
				return { { }, SimpleError::make(callLoc,
					"Mismatched number of arguments in constructor call to type '%s'; expected %d arguments, found %d arguments instead",
					name, fieldNames.size(), arguments.size()).append(
						BareError("All arguments are mandatory when using positional arguments", MsgType::Note)
					)
				};
			}

			return { seenNames, SimpleError() };
		}

	}






	int TypecheckState::getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b)
	{
		OverloadError errs;

		return resolver::computeOverloadDistance(Location(), util::map(a, [](fir::Type* t) -> fir::LocatedType {
			return fir::LocatedType(t, Location());
		}), util::map(b, [](fir::Type* t) -> fir::LocatedType {
			return fir::LocatedType(t, Location());
		}), false).first;
	}

	bool TypecheckState::isDuplicateOverload(const std::vector<FnParam>& a, const std::vector<FnParam>& b)
	{
		return this->getOverloadDistance(util::map(a, [](auto p) -> auto { return p.type; }),
			util::map(b, [](auto p) -> auto { return p.type; })) == 0;
	}

}
































