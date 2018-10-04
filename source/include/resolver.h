// resolver.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "ast.h"
#include "sst.h"
#include "stcommon.h"

namespace fir
{
	struct LocatedType;
}

namespace sst
{
	namespace resolver
	{
		std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<FunctionDecl::Param>& params,
			const std::vector<FunctionDecl::Param>& args, SimpleError* err);

		std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
			const std::vector<FunctionDecl::Param>& args, SimpleError* err);
	}
}