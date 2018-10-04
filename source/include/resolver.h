// resolver.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

#include "ast.h"
#include "stcommon.h"

namespace fir
{
	struct LocatedType;
}

namespace sst
{
	namespace resolver
	{
		std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
			const std::vector<FnCallArgument>& args, SimpleError* err);
	}
}