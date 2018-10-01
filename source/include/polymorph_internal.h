// polymorph_internal.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace pts
{
	struct Type;
}

namespace sst
{
	struct LocatedType;
	struct TypecheckState;

	namespace poly
	{
		struct Solution_t;

		enum class TrfType
		{
			None,
			Slice,
			Pointer,
			FixedArray,
			DynamicArray
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

		SimpleError solveSingleTypeList(TypecheckState* fs, Solution_t* soln, const std::vector<LocatedType>& target, const std::vector<LocatedType>& given);

		namespace misc
		{
			fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b);
			std::vector<fir::Type*> convertPtsTypeList(TypecheckState* fs, const std::unordered_map<std::string, TypeConstraints_t>& problems,
				const std::vector<pts::Type*>& input, int polysession);

			fir::Type* convertPtsType(TypecheckState* fs, const std::unordered_map<std::string, TypeConstraints_t>& problems,
				pts::Type* input, int polysession);

			std::vector<LocatedType> unwrapFunctionCall(TypecheckState* fs, ast::FuncDefn* fd, int polysession, bool includeReturn);
			std::pair<std::vector<LocatedType>, SimpleError> unwrapArgumentList(TypecheckState* fs, ast::FuncDefn* fd,
				const std::vector<FnCallArgument>& args);
		}


		std::vector<std::string> getMissingSolutions(const std::unordered_map<std::string, TypeConstraints_t>& needed, const TypeParamMap_t& solution,
			bool allowPlaceholders);

		std::pair<poly::Solution_t, SimpleError> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing,
			const std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial, fir::Type* infer, bool isFnCall);

		std::pair<TCResult, Solution_t> attemptToInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing,
			const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>* args, bool fillplaceholders);
	}
}














