// resolver.h
// Copyright (c) 2017, zhiayang
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

	namespace poly
	{
		struct ArgType;
	}

	namespace resolver
	{
		std::pair<int, ErrorMsg*> computeNamedOverloadDistance(const Location& fnLoc, const std::vector<FnParam>& target,
			const std::vector<FnCallArgument>& _args, bool cvararg, const Location& callLoc);

		std::pair<int, ErrorMsg*> computeOverloadDistance(const Location& fnLoc, const std::vector<fir::LocatedType>& target,
			const std::vector<fir::LocatedType>& _args, bool cvararg, const Location& callLoc);


		TCResult resolveFunctionCall(TypecheckState* fs, const Location& callLoc, const std::string& name, std::vector<FnCallArgument>* arguments,
			const PolyArgMapping_t& gmaps, bool traverseUp, fir::Type* inferredRetType);

		TCResult resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc, const std::vector<Defn*>& cs,
			std::vector<FnCallArgument>* arguments, const PolyArgMapping_t& gmaps, bool allowImplicitSelf);

		TCResult resolveConstructorCall(TypecheckState* fs, const Location& callLoc, TypeDefn* defn, const std::vector<FnCallArgument>& arguments,
			const PolyArgMapping_t& gmaps);


		std::pair<util::hash_map<std::string, size_t>, ErrorMsg*> verifyStructConstructorArguments(const Location& callLoc,
			const std::string& name, const std::vector<std::string>& fieldNames, const std::vector<FnCallArgument>& arguments);

		TCResult resolveAndInstantiatePolymorphicUnion(TypecheckState* fs, sst::UnionVariantDefn* uvd, std::vector<FnCallArgument>* arguments,
			fir::Type* return_infer, bool isFnCall);


		ErrorMsg* createErrorFromFailedCandidates(TypecheckState* fs, const Location& callLoc, const std::string& name,
			const std::vector<FnCallArgument>& args, const std::vector<std::pair<Locatable*, ErrorMsg*>>& fails);


		namespace misc
		{
			std::pair<TypeParamMap_t, ErrorMsg*> canonicalisePolyArguments(TypecheckState* fs, ast::Parameterisable* thing, const PolyArgMapping_t& pams);

			std::vector<FnCallArgument> typecheckCallArguments(TypecheckState* fs, const std::vector<std::pair<std::string, ast::Expr*>>& args);

			template <typename T>
			util::hash_map<std::string, size_t> getNameIndexMap(const std::vector<T>& params)
			{
				util::hash_map<std::string, size_t> ret;
				for(size_t i = 0; i < params.size(); i++)
				{
					const auto& arg = params[i];
					ret[arg.name] = i;
				}

				return ret;
			}
		}

		namespace internal
		{
			std::pair<TCResult, std::vector<FnCallArgument>> resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc,
				const std::vector<std::pair<Defn*, std::vector<FnCallArgument>>>& cands, const PolyArgMapping_t& gmaps, bool allowImplicitSelf,
				fir::Type* return_infer);
		}
	}
}








