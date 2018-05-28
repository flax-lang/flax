// genericinference.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "sst.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include <set>


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

struct UnsolvedType
{
	UnsolvedType() { }
	UnsolvedType(const Location& l, pts::Type* t) : loc(l), type(t) { }

	Location loc;
	pts::Type* type = 0;
};

struct SolvedType
{
	SolvedType() { }
	SolvedType(const Location& l, fir::Type* t) : lastLoc(l), soln(t) { }

	Location lastLoc;
	fir::Type* soln = 0;
};


using Param_t = sst::FunctionDecl::Param;
using Solution_t = std::unordered_map<std::string, SolvedType>;
using ProblemSpace_t = std::unordered_map<std::string, TypeConstraints_t>;


static std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTrfs(fir::Type* t);
static std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTrfs(pts::Type* t);

static fir::Type* applyTrfs(fir::Type* t, const std::vector<Trf>& trfs);
static std::vector<Trf> reduceTrfsCompatibly(const std::vector<Trf>& master, const std::vector<Trf>& slave);

static fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b);
static std::set<std::string> extractGenericProblems(pts::Type* t, const ProblemSpace_t& problemSpace);






static SimpleError solveSingleArgument(UnsolvedType& problem, const Param_t& input, Solution_t& workingSoln,
	const std::set<std::string>& tosolve, const ProblemSpace_t& problemSpace)
{
	/*
		so what we want to achieve here is that, for some input, for instance '&&&&int',
		and some problem '&T', we want to reduce it so we can tell that 'T = &&&int'.
		also, for some problem '&K' and some input '[int:]', we cannot allow this.

		so the problem must always be reduced to a base type (non compound and definitely without Trfs).
	 */

	auto [ prob, probtrfs ] = decomposeIntoTrfs(problem.type);
	auto [ inpt, inpttrfs ] = decomposeIntoTrfs(input.type);

	if(probtrfs.size() > inpttrfs.size())
	{
		return SimpleError::make(MsgType::Note, input.loc, "Incompatible transformations between input type '%s' and wanted type '%s'",
			input.type, problem.type->str());
	}

	if(prob->isNamedType())
	{
		auto name = prob->toNamedType()->name;

		// check if this is a problem that needs to be solved
		if(tosolve.find(name) != tosolve.end())
		{
			// check if we're conflicting.
			if(auto it = workingSoln.find(name); it != workingSoln.end())
			{
				// actually check if the existing solution is a fake number;
				if(!it->second.soln->isConstantNumberType())
				{
					return SimpleError().append(SpanError(MsgType::Note)
							.set(SimpleError::make(input.loc, "Conflicting solutions for type parameter '%s': '%s' and '%s'", name, it->second.soln, inpt))
							.add(SpanError::Span(it->second.lastLoc, strprintf("Previously solved here as '%s'", it->second.soln)))
							.add(SpanError::Span(input.loc, strprintf("New, conflicting solution '%s' here", inpt)))
						);
				}
				else
				{
					inpt = mergeNumberTypes(inpt, it->second.soln);
				}
			}

			// great.
			workingSoln[name] = SolvedType(input.loc, inpt);
			debuglog("solved %s = %s", name, inpt);
		}
	}
	else
	{
		// ok, we might need to enlist the help of the iterative solver now.

		error("not supported!!!");
	}


	return SimpleError();
}







//* this thing only infers to the best of its abilities; it cannot be guaranteed to return a complete solution.
//* please check for the completeness of said solution before using it.
std::pair<TypeParamMap_t, BareError> sst::TypecheckState::inferTypesForGenericEntity(ast::Parameterisable* _target,
	const std::vector<Param_t>& _input, const TypeParamMap_t& partial, fir::Type* infer)
{
	Solution_t solution;
	for(const auto& p : partial)
		solution[p.first] = SolvedType { this->loc(), p.second };

	std::vector<Param_t> input(_input.size());
	std::vector<UnsolvedType> problem;
	auto problemSpace = _target->generics;

	if(dcast(ast::FuncDefn, _target) || dcast(ast::InitFunctionDefn, _target))
	{
		// strip out the name information, and do purely positional things.
		std::unordered_map<std::string, size_t> nameToIndex;

		{
			std::vector<ast::FuncDefn::Arg> _args;
			if(auto ifd = dcast(ast::InitFunctionDefn, _target))   _args = ifd->args;
			else if(auto fd = dcast(ast::FuncDefn, _target))       _args = fd->args;

			for(size_t i = 0; i < _args.size(); i++)
			{
				const auto& arg = _args[i];
				nameToIndex[arg.name] = i;

				problem.push_back(UnsolvedType(arg.loc, arg.type));
			}
		}

		int counter = 0;
		for(const auto& i : _input)
		{
			if(!i.name.empty() && nameToIndex.find(i.name) == nameToIndex.end())
			{
				return { partial, BareError()
					.append(SimpleError::make(MsgType::Note, i.loc, "Function '%s' does not have a parameter named '%s'",
					_target->name, i.name)).append(SimpleError::make(MsgType::Note, _target, "Function was defined here:"))
				};
			}

			input[i.name.empty() ? counter : nameToIndex[i.name]] = i;
			counter++;
		}
	}
	else
	{
		return { partial, BareError().append(SimpleError::make(MsgType::Note, this->loc(),
			"Unable to infer types for unsupported entity '%s'", _target->name))
		};
	}


	bool variadic = problem.back().type->isVariadicArrayType();


	if(input.size() != problem.size())
	{
		// if(variadic) return { partial,  };
		if(variadic)
		{
			error("variadic things not supported!");
		}
		else
		{
			return { partial, BareError().append(SimpleError::make(MsgType::Note, this->loc(),
				"Mismatched number of arguments; expected %d, got %d instead", problem.size(), input.size()))
				.append(SimpleError::make(MsgType::Note, _target, "Function was defined here:"))
			};
		}
	}


	std::set<std::string> tosolve;
	for(const auto& p : problemSpace)
		tosolve.insert(p.first);

	BareError retError;
	for(size_t i = 0; i < problem.size(); i++)
	{
		auto err = solveSingleArgument(problem[i], input[i], solution, tosolve, problemSpace);

		if(err.hasErrors())
			retError.append(err);
	}








	// convert the thing.
	{
		TypeParamMap_t ret;
		for(const auto& soln : solution)
		{
			auto s = soln.second.soln;
			if(s->isConstantNumberType())
				s = this->inferCorrectTypeForLiteral(s->toConstantNumberType());

			ret[soln.first] = s;
		}

		return { ret, retError };
	}
}















































static std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTrfs(fir::Type* t)
{
	std::vector<Trf> ret;

	while(true)
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



static fir::Type* applyTrfs(fir::Type* t, const std::vector<Trf>& trfs)
{
	for(const auto& trf : trfs)
	{
		switch(trf.type)
		{
			case TrfType::None:
				break;

			case TrfType::Pointer:
				t = t->getPointerTo();
				break;

			case TrfType::DynamicArray:
				t = fir::DynamicArrayType::get(t);
				break;

			case TrfType::FixedArray:
				t = fir::ArrayType::get(t, trf.data);
				break;

			case TrfType::Slice:
				t = fir::ArraySliceType::get(t, trf.data);
				break;

			default:
				iceAssert(0);
		}
	}

	return t;
}

// wow what are with these names?
static std::vector<Trf> reduceTrfsCompatibly(const std::vector<Trf>& master, const std::vector<Trf>& slave)
{
	iceAssert(master.size() <= slave.size());

	auto ret = master;
	for(size_t i = 0; i < master.size(); i++)
	{
		auto at = master[master.size() - i - 1];
		auto bt = slave[slave.size() - i - 1];

		if(at != bt)    break;
		else            ret.pop_back();
	}

	return ret;
}

static void _extractGenericProblems(pts::Type* t, const ProblemSpace_t& problemSpace, std::set<std::string>& list)
{
	if(t->isNamedType())
	{
		if(problemSpace.find(t->toNamedType()->name) != problemSpace.end())
			list.insert(t->toNamedType()->name);

		for(auto m : t->toNamedType()->genericMapping)
			_extractGenericProblems(m.second, problemSpace, list);
	}
	else if(t->isFunctionType())
	{
		for(auto p : t->toFunctionType()->argTypes)
			_extractGenericProblems(p, problemSpace, list);

		_extractGenericProblems(t->toFunctionType()->returnType, problemSpace, list);
	}
	else if(t->isTupleType())
	{
		for(auto m : t->toTupleType()->types)
			_extractGenericProblems(m, problemSpace, list);
	}
	else if(t->isPointerType())
	{
		while(t->isPointerType())
			t = t->toPointerType()->base;

		_extractGenericProblems(t, problemSpace, list);
	}
	else if(t->isDynamicArrayType())
	{
		_extractGenericProblems(t->toDynamicArrayType()->base, problemSpace, list);
	}
	else if(t->isFixedArrayType())
	{
		_extractGenericProblems(t->toFixedArrayType()->base, problemSpace, list);
	}
	else if(t->isVariadicArrayType())
	{
		_extractGenericProblems(t->toVariadicArrayType()->base, problemSpace, list);
	}
	else if(t->isArraySliceType())
	{
		_extractGenericProblems(t->toArraySliceType()->base, problemSpace, list);
	}
}

static std::set<std::string> extractGenericProblems(pts::Type* t, const ProblemSpace_t& problemSpace)
{
	std::set<std::string> ret;
	_extractGenericProblems(t, problemSpace, ret);

	return ret;
}




static fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b)
{
	if(a->isConstantNumberType() && b->isConstantNumberType())
	{
		auto an = a->toConstantNumberType()->getValue();
		auto bn = b->toConstantNumberType()->getValue();

		if(mpfr::isint(an) && mpfr::isint(bn))
			return (mpfr::abs(an) > mpfr::abs(bn) ? a : b);

		else if(mpfr::isint(an) && !mpfr::isint(bn))
			return b;

		else
			return a;
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




















