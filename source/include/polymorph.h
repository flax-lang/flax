// polymorph.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

#include "ast.h"
#include "stcommon.h"

#include "ir/type.h"

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
		using ProblemSpace_t = std::vector<std::pair<std::string, TypeConstraints_t>>;

		struct ArgType
		{
			ArgType() { }
			explicit ArgType(fir::Type* t) : type(t) { }
			ArgType(const std::string& n, fir::Type* t, const Location& l) : name(n), type(t), loc(l) { }
			ArgType(const std::string& n, fir::Type* t, const Location& l, bool opt) : name(n), type(t), loc(l), optional(opt) { }
			ArgType(const std::string& n, fir::Type* t, const Location& l, bool opt, bool ignore) : name(n), type(t), loc(l), optional(opt),
				ignoreName(ignore) { }

			operator fir::Type* () const { return this->type; }
			fir::Type* operator -> () const { return this->type; }
			fir::LocatedType toFLT() const { return fir::LocatedType(this->type, this->loc); }

			std::string name;
			fir::Type* type = 0;
			Location loc;
			bool optional = false;
			bool ignoreName = false;
		};

		struct Solution_t
		{
			Solution_t() { }
			explicit Solution_t(const util::hash_map<std::string, fir::Type*>& p) : solutions(p) { }

			bool hasSolution(const std::string& n) const ;
			fir::LocatedType getSolution(const std::string& n) const;
			void addSolution(const std::string& x, const fir::LocatedType& y);

			fir::Type* substitute(fir::Type* x) const;
			void resubstituteIntoSolutions();
			void addSubstitution(fir::Type* x, fir::Type* y);

			bool operator == (const Solution_t& other) const;
			bool operator != (const Solution_t& other) const;

			// incorporate distance so we can use this shit for our function resolution.
			int distance = 0;
			util::hash_map<std::string, fir::Type*> solutions;
			util::hash_map<fir::Type*, fir::Type*> substitutions;
		};


		std::pair<Solution_t, ErrorMsg*> solveTypeList(const Location& callLoc, const std::vector<ArgType>& target,
			const std::vector<ArgType>& given, const Solution_t& partial, bool isFnCall);

		TCResult fullyInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const TypeParamMap_t& mappings);

		struct PolyRefResult
		{
			PolyRefResult(const TCResult& r, const Solution_t& s, ast::Parameterisable* t) :
				res(r), soln(s), thing(t) { }

			TCResult res;
			Solution_t soln;
			ast::Parameterisable* thing;
		};

		std::vector<PolyRefResult> findPolymorphReferences(TypecheckState* fs, const std::string& name,
			const std::vector<ast::Parameterisable*>& gdefs, const PolyArgMapping_t& _gmaps, fir::Type* return_infer,
			fir::Type* type_infer, bool isFnCall, std::vector<FnCallArgument>* args);

		std::pair<TCResult, Solution_t> attemptToInstantiatePolymorph(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
			const TypeParamMap_t& _gmaps, fir::Type* return_infer, fir::Type* type_infer, bool isFnCall, std::vector<FnCallArgument>* args,
			bool fillplaceholders, fir::Type* problem_infer = 0);


		namespace internal
		{
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

			fir::Type* applyTransforms(fir::Type* base, const std::vector<Trf>& trfs);

			std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTransforms(fir::Type* t, size_t max);
			std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTransforms(pts::Type* t);

			int getNextSessionId();



			fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b);
			std::vector<fir::Type*> convertPtsTypeList(TypecheckState* fs, const ProblemSpace_t& problems,
				const std::vector<pts::Type*>& input, int polysession);

			fir::Type* convertPtsType(TypecheckState* fs, const ProblemSpace_t& problems,
				pts::Type* input, int polysession);

			std::vector<ArgType> unwrapFunctionParameters(TypecheckState* fs, const ProblemSpace_t& problems,
				const std::vector<ast::FuncDefn::Param>& args, int polysession);



			std::pair<TCResult, Solution_t> solvePolymorphWithPlaceholders(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
				const TypeParamMap_t& partial);

			std::vector<std::string> getMissingSolutions(const ProblemSpace_t& needed, const TypeParamMap_t& solution,
				bool allowPlaceholders);


			std::pair<Solution_t, ErrorMsg*> inferTypesForPolymorph(TypecheckState* fs, ast::Parameterisable* thing, const std::string& name,
				const ProblemSpace_t& problems, const std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial,
				fir::Type* return_infer, fir::Type*, bool isFnCall, fir::Type* problem_infer, util::hash_map<std::string, size_t>* origParamOrder);
		}
	}
}




































