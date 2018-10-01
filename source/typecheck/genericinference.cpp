// genericinference.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "sst.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"


namespace sst
{
	struct LocatedType
	{
		LocatedType() { }
		LocatedType(fir::Type* t) : type(t) { }
		LocatedType(fir::Type* t, const Location& l) : type(t), loc(l) { }

		fir::Type* type = 0;
		Location loc;
	};


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

	struct SolutionSet_t
	{
		void addSolution(const std::string& x, const LocatedType& y)
		{
			this->solutions[x] = y.type;
		}

		void addSubstitution(fir::Type* x, fir::Type* y)
		{
			if(auto it = this->substitutions.find(x); it != this->substitutions.end())
			{
				if(it->second != y) error("conflicting substitutions for '%s': '%s' and '%s'", x, y, it->second);
				debuglogln("substitution: '%s' -> '%s'", x, y);
			}

			this->substitutions[x] = y;
		}

		bool hasSolution(const std::string& n)
		{
			return this->solutions.find(n) != this->solutions.end();
		}

		LocatedType getSolution(const std::string& n)
		{
			if(auto it = this->solutions.find(n); it != this->solutions.end())
				return it->second;

			else
				return LocatedType(0);
		}

		fir::Type* substitute(fir::Type* x)
		{
			if(auto it = this->substitutions.find(x); it != this->substitutions.end())
				return it->second;

			else
				return x;
		}


		void resubstituteIntoSolutions()
		{
			// iterate through everything
			for(auto& [ n, t ] : this->solutions)
				t = this->substitute(t);
		}


		bool operator == (const SolutionSet_t& other) const
		{
			return other.distance == this->distance
				&& other.solutions == this->solutions
				&& other.substitutions == this->substitutions;
		}

		bool operator != (const SolutionSet_t& other) const
		{
			return !(other == *this);
		}

		// incorporate distance so we can use this shit for our function resolution.
		int distance = 0;
		std::unordered_map<std::string, fir::Type*> solutions;
		std::unordered_map<fir::Type*, fir::Type*> substitutions;
	};

	using ProblemSpace_t = std::unordered_map<std::string, TypeConstraints_t>;

	static int polySessionId = 0;

	static fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b);
	static fir::Type* applyTrfs(fir::Type* base, const std::vector<Trf>& trfs);
	static std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTrfs(fir::Type* t, size_t max);
	static std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTrfs(pts::Type* t);








	static SimpleError _internal_solveTypeList(TypecheckState* fs, SolutionSet_t* soln, const std::vector<LocatedType>& target,
		const std::vector<LocatedType>& given);

	static SimpleError _internal_solveSingleType(TypecheckState* fs, SolutionSet_t* soln, const LocatedType& target, const LocatedType& given)
	{
		auto tgt = target.type;
		auto gvn = given.type;


		// if we're just looking at normal types, then just add the cost.
		if(!tgt->containsPlaceholders() && !gvn->containsPlaceholders())
		{
			int dist = fs->getCastDistance(gvn, tgt);
			if(dist >= 0)
			{
				soln->distance += dist;
				return SimpleError();
			}
			else
			{
				soln->distance = -1;
				return SimpleError::make(given.loc, "No valid cast from given type '%s' to target type '%s'", gvn, tgt);
			}
		}
		else
		{
			// limit decomposition of the given types by the number of transforms on the target type.
			auto [ tt, ttrfs ] = decomposeIntoTrfs(tgt, -1);
			auto [ gt, gtrfs ] = decomposeIntoTrfs(gvn, ttrfs.size());

			// substitute if possible.
			if(auto _gt = soln->substitute(gt); _gt != gt)
			{
				debuglogln("substituted '%s' -> '%s'", gt, _gt);
				gt = _gt;
			}

			bool ttpoly = tt->isPolyPlaceholderType();
			bool gtpoly = gt->isPolyPlaceholderType();

			// check what kind of monster we're dealing with.
			if(tt->isPolyPlaceholderType())
			{
				// see if there's a match.
				auto ptt = tt->toPolyPlaceholderType();
				if(auto ltt = soln->getSolution(ptt->getName()); ltt.type != 0)
				{
					// check for conflict.
					if(!ltt.type->isPolyPlaceholderType() && !gtpoly)
					{
						if(ltt.type->isConstantNumberType() || gt->isConstantNumberType())
						{
							gt = mergeNumberTypes(ltt.type, gt);
							if(gt != ltt.type)
								soln->addSolution(ptt->getName(), LocatedType(gt, given.loc));
						}
						else if(ltt.type != gt)
						{
							return SimpleError::make(given.loc, "Conflicting solutions for type parameter '%s': previous: '%s', current: '%s'",
								ptt->getName(), ltt.type, gvn);
						}
					}
					else if(ltt.type->isPolyPlaceholderType() && !gtpoly)
					{
						soln->addSubstitution(ltt.type, gt);
					}
					else if(!ltt.type->isPolyPlaceholderType() && gtpoly)
					{
						soln->addSubstitution(gt, ltt.type);
					}
					else if(ltt.type->isPolyPlaceholderType() && gtpoly)
					{
						error("what???? '%s' and '%s' are both poly??", ltt.type, gt);
					}
				}
				else
				{
					soln->addSolution(ptt->getName(), LocatedType(gt, given.loc));
					debuglogln("solved %s = '%s'", ptt->getName(), gt);
				}
			}
			else if(tt->isFunctionType() || tt->isTupleType())
			{
				// make sure they're the same 'kind' of type first.
				if(tt->isFunctionType() != gt->isFunctionType() || tt->isTupleType() != gt->isTupleType())
					return SimpleError::make(given.loc, "no valid conversion from given type '%s' to target type '%s'", gt, tt);

				std::vector<LocatedType> problem;
				std::vector<LocatedType> input;
				if(gt->isFunctionType())
				{
					input = util::map(gt->toFunctionType()->getArgumentTypes(), [given](fir::Type* t) -> LocatedType {
						return LocatedType(t, given.loc);
					}) + LocatedType(gt->toFunctionType()->getReturnType(), given.loc);

					problem = util::map(tt->toFunctionType()->getArgumentTypes(), [target](fir::Type* t) -> LocatedType {
						return LocatedType(t, target.loc);
					}) + LocatedType(tt->toFunctionType()->getReturnType(), target.loc);
				}
				else
				{
					iceAssert(gt->isTupleType());
					input = util::map(gt->toTupleType()->getElements(), [given](fir::Type* t) -> LocatedType {
						return LocatedType(t, given.loc);
					});

					problem = util::map(tt->toTupleType()->getElements(), [target](fir::Type* t) -> LocatedType {
						return LocatedType(t, target.loc);
					});
				}

				return _internal_solveTypeList(fs, soln, problem, input);
			}
			else
			{
				error("'%s' not supported", tt);
			}
		}

		return SimpleError();
	}

	static SimpleError _internal_solveTypeList(TypecheckState* fs, SolutionSet_t* soln, const std::vector<LocatedType>& target,
		const std::vector<LocatedType>& given)
	{
		// for now just do this.
		if(target.size() != given.size())
			return SimpleError::make(fs->loc(), "mismatched argument count");

		for(size_t i = 0; i < std::min(target.size(), given.size()); i++)
		{
			auto err = _internal_solveSingleType(fs, soln, target[i], given[i]);
			if(err.hasErrors())
				return err;

			// possibly increase solution completion by re-substituting with new information
			soln->resubstituteIntoSolutions();
		}

		return SimpleError();
	}


	static std::pair<SolutionSet_t, SimpleError> _internal_iterativelySolveTypeList(TypecheckState* fs, const std::vector<LocatedType>& target,
		const std::vector<LocatedType>& given)
	{
		SolutionSet_t prevSoln;
		while(true)
		{
			auto soln = prevSoln;
			auto errs = _internal_solveTypeList(fs, &soln, target, given);
			if(errs.hasErrors()) return { soln, errs };

			if(soln == prevSoln)    break;
			else                    prevSoln = soln;
		}

		return { prevSoln, SimpleError() };
	}















	static std::vector<fir::Type*> _internal_convertPtsTypeList(TypecheckState* fs, const ProblemSpace_t& problems, const std::vector<pts::Type*>& input,
		int polysession);

	static fir::Type* _internal_convertPtsType(TypecheckState* fs, const ProblemSpace_t& problems, pts::Type* input, int polysession)
	{
		fir::Type* fty = 0;
		auto [ ty, trfs ] = decomposeIntoTrfs(input);

		if(ty->isNamedType())
		{
			if(problems.find(ty->toNamedType()->name) != problems.end())
				fty = fir::PolyPlaceholderType::get(ty->toNamedType()->name, polysession);

			else
				fty = fs->convertParserTypeToFIR(ty, false);
		}
		else if(ty->isTupleType())
		{
			fty = fir::TupleType::get(_internal_convertPtsTypeList(fs, problems, ty->toTupleType()->types, polysession));
		}
		else if(ty->isFunctionType())
		{
			fty = fir::FunctionType::get(_internal_convertPtsTypeList(fs, problems, ty->toFunctionType()->argTypes, polysession),
				_internal_convertPtsType(fs, problems, ty->toFunctionType()->returnType, polysession));
		}
		else
		{
			error("unsupported pts type '%s'", ty->str());
		}

		return applyTrfs(fty, trfs);
	}

	static std::vector<fir::Type*> _internal_convertPtsTypeList(TypecheckState* fs, const ProblemSpace_t& problems, const std::vector<pts::Type*>& input,
		int polysession)
	{
		// mm, smells functional.
		return util::map(input, [fs, problems, polysession](pts::Type* pt) -> fir::Type* {
			return _internal_convertPtsType(fs, problems, pt, polysession);
		});
	}










	static std::vector<LocatedType> _internal_unwrapFunctionCall(TypecheckState* fs, ast::FuncDefn* fd, int polysession, bool includeReturn)
	{
		auto ret = util::mapidx(_internal_convertPtsTypeList(fs, fd->generics, util::map(fd->args,
			[](const ast::FuncDefn::Arg& a) -> pts::Type* {
				return a.type;
			}
		), polysession), [fd](fir::Type* t, size_t idx) -> LocatedType {
			return LocatedType(t, fd->args[idx].loc);
		});

		if(includeReturn)
			return ret + LocatedType(_internal_convertPtsType(fs, fd->generics, fd->returnType, polysession), fd->loc);

		else
			return ret;
	}



	static std::pair<std::vector<LocatedType>, SimpleError> _internal_unwrapArgumentList(TypecheckState* fs, ast::FuncDefn* fd,
		const std::vector<FnCallArgument>& args)
	{
		std::vector<LocatedType> ret(args.size());

		// strip out the name information, and do purely positional things.
		std::unordered_map<std::string, size_t> nameToIndex;
		{
			for(size_t i = 0; i < fd->args.size(); i++)
			{
				const auto& arg = fd->args[i];
				nameToIndex[arg.name] = i;
			}

			int counter = 0;
			for(const auto& i : args)
			{
				if(!i.name.empty() && nameToIndex.find(i.name) == nameToIndex.end())
				{
					return { { }, SimpleError::make(MsgType::Note, i.loc, "function '%s' does not have a parameter named '%s'",
						fd->name, i.name).append(SimpleError::make(MsgType::Note, fd, "Function was defined here:"))
					};
				}

				ret[i.name.empty() ? counter : nameToIndex[i.name]] = LocatedType(i.value->type, i.loc);
				counter++;
			}
		}

		return { ret, SimpleError() };
	}


	//* might not return a complete solution, and does not check that the solution it returns is complete at all.
	std::pair<SolutionSet_t, SimpleError>
	TypecheckState::inferTypesForGenericEntity2(ast::Parameterisable* _target, std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial,
		fir::Type* infer, bool isFnCall)
	{
		SolutionSet_t soln;
		for(const auto& p : partial)
			soln.addSolution(p.first, LocatedType(p.second));


		if(auto td = dcast(sst::TypeDefn, _target))
		{
			error("type not supported");
		}
		else if(auto fd = dcast(ast::FuncDefn, _target))
		{
			std::vector<LocatedType> given;
			std::vector<LocatedType> target;

			target = _internal_unwrapFunctionCall(this, fd, polySessionId++, /* includeReturn: */ !isFnCall || infer);

			if(isFnCall)
			{
				auto [ gvn, err] = _internal_unwrapArgumentList(this, fd, _input);
				if(err.hasErrors()) return { soln, err };

				given = gvn;
				if(infer)
					given.push_back(LocatedType(infer));
			}
			else
			{
				if(infer == 0)
					return { soln, SimpleError::make(this->loc(), "unable to infer type for reference to '%s'", fd->name) };

				else if(!infer->isFunctionType())
					return { soln, SimpleError::make(this->loc(), "invalid type '%s' inferred for '%s'", infer, fd->name) };

				// ok, we should have it.
				iceAssert(infer->isFunctionType());
				given = util::mapidx(infer->toFunctionType()->getArgumentTypes(), [fd](fir::Type* t, size_t i) -> LocatedType {
					return LocatedType(t, fd->args[i].loc);
				}) + LocatedType(infer->toFunctionType()->getReturnType(), fd->loc);
			}


			auto [ soln, errs ] = _internal_iterativelySolveTypeList(this, target, given);
			for(auto& pair : soln.solutions)
			{
				if(pair.second->isConstantNumberType())
					pair.second = fir::getBestFitTypeForConstant(pair.second->toConstantNumberType());
			}

			return { soln, errs };
		}
		else
		{
			return std::make_pair(soln,
				SimpleError(_target->loc, strprintf("Unable to infer type for unsupported entity '%s'", _target->getKind()))
			);
		}
	}



	//* might not return a complete solution, and does not check that the solution it returns is complete at all.
	std::tuple<TypeParamMap_t, std::unordered_map<fir::Type*, fir::Type*>, SimpleError>
	TypecheckState::inferTypesForGenericEntity(ast::Parameterisable* _target, std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial,
		fir::Type* infer, bool isFnCall)
	{
		auto ret = this->inferTypesForGenericEntity2(_target, _input, partial, infer, isFnCall);
		return std::make_tuple(ret.first.solutions, ret.first.substitutions, ret.second);
	}
































	static std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTrfs(fir::Type* t, size_t max)
	{
		std::vector<Trf> ret;

		for(size_t i = 0; i < max; i++)
		{
			if(t->isDynamicArrayType())
			{
				ret.push_back(Trf(TrfType::DynamicArray));
				t = t->getArrayElementType();
			}
			else if(t->isArraySliceType())
			{
				ret.push_back(Trf(TrfType::Slice, t->toArraySliceType()->isMutable()));
				t = t->getArrayElementType();
			}
			else if(t->isArrayType())
			{
				ret.push_back(Trf(TrfType::FixedArray, t->toArrayType()->getArraySize()));
				t = t->getArrayElementType();
			}
			else if(t->isPointerType())
			{
				ret.push_back(Trf(TrfType::Pointer));
				t = t->getPointerElementType();
			}
			else
			{
				break;
			}
		}

		return { t, ret };
	}

	static std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTrfs(pts::Type* t)
	{
		std::vector<Trf> ret;

		while(true)
		{
			if(t->isDynamicArrayType())
			{
				ret.push_back(Trf(TrfType::DynamicArray));
				t = t->toDynamicArrayType()->base;
			}
			else if(t->isArraySliceType())
			{
				ret.push_back(Trf(TrfType::Slice, t->toArraySliceType()->mut));
				t = t->toArraySliceType()->base;
			}
			else if(t->isFixedArrayType())
			{
				ret.push_back(Trf(TrfType::FixedArray, t->toFixedArrayType()->size));
				t = t->toFixedArrayType()->base;
			}
			else if(t->isPointerType())
			{
				ret.push_back(Trf(TrfType::Pointer));
				t = t->toPointerType()->base;
			}
			else
			{
				break;
			}
		}

		return { t, ret };
	}

	static fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b)
	{
		if(a->isConstantNumberType() && b->isConstantNumberType())
		{
			return fir::unifyConstantTypes(a->toConstantNumberType(), b->toConstantNumberType());
		}
		else if(a->isFloatingPointType() && b->isIntegerType())
		{
			return a;
		}
		else if(a->isIntegerType() && b->isFloatingPointType())
		{
			return b;
		}
		else
		{
			if(a->getBitWidth() > b->getBitWidth())
				return a;

			else
				return b;
		}
	}





	static fir::Type* applyTrfs(fir::Type* base, const std::vector<Trf>& trfs)
	{
		for(auto t : trfs)
		{
			switch(t.type)
			{
				case TrfType::None:
					break;
				case TrfType::Slice:
					base = fir::ArraySliceType::get(base, (bool) t.data);
					break;
				case TrfType::Pointer:
					base = base->getPointerTo();
					break;
				case TrfType::FixedArray:
					base = fir::ArrayType::get(base, t.data);
					break;
				case TrfType::DynamicArray:
					base = fir::DynamicArrayType::get(base);
					break;
				default:
					error("unsupported transformation '%d'", (int) t.type);
			}
		}
		return base;
	}
}












