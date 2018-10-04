// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "resolver.h"

#include "ir/type.h"

namespace sst {
namespace resolver
{
	std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
		const std::vector<FnCallArgument>& args, SimpleError* err)
	{
		std::vector<fir::LocatedType> ret(args.size());

		// strip out the name information, and do purely positional things.
		std::unordered_map<std::string, size_t> nameToIndex;
		{
			for(size_t i = 0; i < params.size(); i++)
			{
				const auto& arg = params[i];
				nameToIndex[arg.name] = i;
			}

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
					else if(ret[nameToIndex[i.name]].type != 0)
					{
						iceAssert(err);
						*err = SimpleError::make(MsgType::Note, i.loc, "argument '%s' was already provided", i.name).append(
							SimpleError::make(MsgType::Note, ret[nameToIndex[i.name]].loc, "here:"));

						return { };
					}
				}

				ret[i.name.empty() ? counter : nameToIndex[i.name]] = fir::LocatedType(i.value->type, i.loc);
				counter++;
			}
		}

		return ret;
	}
}
}
































