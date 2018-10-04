// polymorph_internal.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "defs.h"

namespace pts
{
	struct Type;
}

namespace fir
{
	struct LocatedType;
}

namespace sst
{
	struct TypecheckState;

	namespace poly
	{
		struct Solution_t;
		using ProblemSpace_t = std::unordered_map<std::string, TypeConstraints_t>;

		enum class TrfType
		{
			None,
			Slice,
			Pointer,
			FixedArray,
			DynamicArray,
			VariadicArray,
		};

		struct Trf
		{
			bool operator == (const Trf& other) const { return this->type == other.type && this->data == other.data; }
			bool operator != (const Trf& other) const { return !(*this == other); }

			Trf(TrfType t, size_t d = 0) : type(t), data(d) { }

			TrfType type = TrfType::None;
			size_t data = 0;
		};

		int getNextSessionId();

		fir::Type* applyTransforms(fir::Type* base, const std::vector<Trf>& trfs);

		std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTransforms(fir::Type* t, size_t max);
		std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTransforms(pts::Type* t);

		SimpleError solveSingleTypeList(Solution_t* soln, const std::vector<fir::LocatedType>& target,
			const std::vector<fir::LocatedType>& given, bool isFnCall);

		namespace misc
		{
			fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b);
			std::vector<fir::Type*> convertPtsTypeList(TypecheckState* fs, const ProblemSpace_t& problems,
				const std::vector<pts::Type*>& input, int polysession);

			fir::Type* convertPtsType(TypecheckState* fs, const ProblemSpace_t& problems,
				pts::Type* input, int polysession);

			std::vector<fir::LocatedType> unwrapFunctionCall(TypecheckState* fs, const ProblemSpace_t& problems,
				const std::vector<ast::FuncDefn::Arg>& args, int polysession);
		}


		std::vector<std::string> getMissingSolutions(const ProblemSpace_t& needed, const TypeParamMap_t& solution,
			bool allowPlaceholders);

		std::pair<poly::Solution_t, SimpleError> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing,
			const ProblemSpace_t& problems, const std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial, fir::Type* infer, bool isFnCall);

		std::pair<TCResult, Solution_t> attemptToInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing,
			const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>* args, bool fillplaceholders);

		std::pair<TCResult, Solution_t> solvePolymorphWithPlaceholders(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& partial);
	}
}














