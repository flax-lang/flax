// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "resolver.h"

#include "ir/type.h"

namespace sst {
namespace resolver
{
	static std::vector<fir::LocatedType> _canonicaliseCallArguments(const Location& target, const std::unordered_map<std::string, size_t>& nameToIndex,
		const std::vector<FunctionDecl::Param>& args, SimpleError* err)
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

				ret[i.name.empty() ? counter : nameToIndex.find(i.name)->second] = fir::LocatedType(i.type, i.loc);
				counter++;
			}
		}

		return ret;
	}



	std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<FunctionDecl::Param>& params,
		const std::vector<FunctionDecl::Param>& args, SimpleError* err)
	{
		std::unordered_map<std::string, size_t> nameToIndex;
		for(size_t i = 0; i < params.size(); i++)
		{
			const auto& arg = params[i];
			nameToIndex[arg.name] = i;
		}

		return _canonicaliseCallArguments(target, nameToIndex, args, err);
	}

	std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
		const std::vector<FunctionDecl::Param>& args, SimpleError* err)
	{
		std::unordered_map<std::string, size_t> nameToIndex;
		for(size_t i = 0; i < params.size(); i++)
		{
			const auto& arg = params[i];
			nameToIndex[arg.name] = i;
		}

		return _canonicaliseCallArguments(target, nameToIndex, args, err);
	}
}
}
































