// polymorph.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "stcommon.h"

namespace pts
{
	struct Type;
}

namespace ast
{
	struct FuncDefn;
	struct Parameterisable;
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
		struct Solution_t
		{
			bool hasSolution(const std::string& n);
			fir::LocatedType getSolution(const std::string& n);
			void addSolution(const std::string& x, const fir::LocatedType& y);

			fir::Type* substitute(fir::Type* x);
			void resubstituteIntoSolutions();
			void addSubstitution(fir::Type* x, fir::Type* y);

			bool operator == (const Solution_t& other) const;
			bool operator != (const Solution_t& other) const;

			// incorporate distance so we can use this shit for our function resolution.
			int distance = 0;
			std::unordered_map<std::string, fir::Type*> solutions;
			std::unordered_map<fir::Type*, fir::Type*> substitutions;
		};

		SimpleError solveSingleType(Solution_t* soln, const fir::LocatedType& target, const fir::LocatedType& given);
		std::pair<Solution_t, SimpleError> solveTypeList(const std::vector<fir::LocatedType>& target, const std::vector<fir::LocatedType>& given,
			const Solution_t& partial, bool isFnCall);

		TCResult fullyInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& mappings);

		std::vector<std::pair<TCResult, Solution_t>> findPolymorphReferences(TypecheckState* fs, const std::string& name,
			const std::vector<ast::Parameterisable*>& gdefs, const TypeParamMap_t& _gmaps, fir::Type* infer, bool isFnCall,
			std::vector<FnCallArgument>* args);
	}
}




































