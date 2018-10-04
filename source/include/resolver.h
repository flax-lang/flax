// resolver.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "ast.h"
#include "sst.h"
#include "stcommon.h"

#include <set>

namespace fir
{
	struct LocatedType;
}

namespace sst
{
	struct TypecheckState;

	namespace resolver
	{
		std::pair<int, SpanError> computeOverloadDistance(const Location& fnLoc, const std::vector<FunctionDecl::Param>& target,
			const std::vector<FunctionDecl::Param>& _args, bool cvararg);

		TCResult resolveFunctionCallFromCandidates(const Location& callLoc, const std::vector<std::pair<Defn*,
			std::vector<FunctionDecl::Param>>>& cands, const TypeParamMap_t& gmaps, bool allowImplicitSelf);

		std::pair<std::unordered_map<std::string, size_t>, SimpleError> verifyStructConstructorArguments(const Location& callLoc,
			const std::string& name, const std::set<std::string>& fieldNames, const std::vector<FunctionDecl::Param>& arguments);

		namespace misc
		{
			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<FunctionDecl::Param>& params,
				const std::vector<FunctionDecl::Param>& args, SimpleError* err);

			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
				const std::vector<FunctionDecl::Param>& args, SimpleError* err);

			std::vector<FnCallArgument> typecheckCallArguments(TypecheckState* fs, const std::vector<std::pair<std::string, ast::Expr*>>& args);
		}
	}
}