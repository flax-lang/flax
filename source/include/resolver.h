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

		TCResult resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc, const std::vector<std::pair<Defn*,
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


			template <typename T>
			std::unordered_map<std::string, size_t> getNameIndexMap(const std::vector<T>& params)
			{
				std::unordered_map<std::string, size_t> ret;
				for(size_t i = 0; i < params.size(); i++)
				{
					const auto& arg = params[i];
					ret[arg.name] = i;
				}

				return ret;
			}
		}
	}
}








