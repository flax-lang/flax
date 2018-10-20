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
		std::pair<int, ErrorMsg*> computeNamedOverloadDistance(const Location& fnLoc, const std::vector<FnParam>& target,
			const std::vector<FnCallArgument>& _args, bool cvararg);

		std::pair<int, ErrorMsg*> computeOverloadDistance(const Location& fnLoc, const std::vector<fir::LocatedType>& target,
			const std::vector<fir::LocatedType>& _args, bool cvararg);


		std::pair<TCResult, std::vector<FnCallArgument>> resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc,
			const std::vector<std::pair<Defn*, std::vector<FnCallArgument>>>& cands, const TypeParamMap_t& gmaps, bool allowImplicitSelf,
			fir::Type* return_infer);

		std::pair<std::unordered_map<std::string, size_t>, ErrorMsg*> verifyStructConstructorArguments(const Location& callLoc,
			const std::string& name, const std::set<std::string>& fieldNames, const std::vector<FnCallArgument>& arguments);

		namespace misc
		{
			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<FnParam>& params,
				const std::vector<FnCallArgument>& args, ErrorMsg** err);

			std::vector<fir::LocatedType> canonicaliseCallArguments(const Location& target, const std::vector<ast::FuncDefn::Arg>& params,
				const std::vector<FnCallArgument>& args, ErrorMsg** err);

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








