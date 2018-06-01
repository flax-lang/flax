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

struct SolutionSet
{
	void addSolution(const std::string& x, const SolvedType& y)
	{
		this->solutions[x] = y;
	}

	void addSubstitution(fir::Type* x, fir::Type* y)
	{
		this->substitutions[x] = y;
	}

	bool hasSolution(const std::string& n)
	{
		return this->solutions.find(n) != this->solutions.end();
	}

	SolvedType getSolution(const std::string& n)
	{
		if(auto it = this->solutions.find(n); it != this->solutions.end())
			return it->second;

		else
			return SolvedType();
	}

	fir::Type* substitute(fir::Type* x)
	{
		if(auto it = this->substitutions.find(x); it != this->substitutions.end())
			return it->second;

		else
			return x;
	}

	std::unordered_map<std::string, SolvedType> solutions;
	std::unordered_map<fir::Type*, fir::Type*> substitutions;
};

static int _indent = 0;

template <typename... Ts>
void dbgprintln(const char* fmt, Ts... ts)
{
	if((false))
	{
		debuglog("%*s", _indent * 4, " ");
		debuglogln(fmt, ts...);
	}
}




using Param_t = sst::FunctionDecl::Param;
using Solution_t = std::unordered_map<std::string, SolvedType>;
using ProblemSpace_t = std::unordered_map<std::string, TypeConstraints_t>;


static fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b);

static std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTrfs(fir::Type* t, size_t max);
static std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTrfs(pts::Type* t);






static SimpleError solveArgumentList(std::vector<UnsolvedType>& problem, const std::vector<Param_t>& input, SolutionSet& workingSoln,
	const std::set<std::string>& tosolve, const ProblemSpace_t& problemSpace);

static SimpleError solveSingleArgument(UnsolvedType& problem, const Param_t& input, SolutionSet& workingSoln,
	const std::set<std::string>& tosolve, const ProblemSpace_t& problemSpace)
{
	/*
		so what we want to achieve here is that, for some input, for instance '&&&&int',
		and some problem '&T', we want to reduce it so we can tell that 'T = &&&int'.
		also, for some problem '&K' and some input '[int:]', we cannot allow this.

		so the problem must always be reduced to a base type (non compound and definitely without Trfs).
	 */

	auto [ prob, probtrfs ] = decomposeIntoTrfs(problem.type);
	auto [ inpt, inpttrfs ] = decomposeIntoTrfs(input.type, probtrfs.size());

	dbgprintln("trfs: (%s => %s) (%d) -> (%s => %s) (%d)", problem.type->str(), prob->str(), probtrfs.size(), input.type, inpt, inpttrfs.size());


	if(auto _inpt = workingSoln.substitute(inpt); inpt != _inpt)
	{
		dbgprintln("substituting %s -> %s", inpt, _inpt);
		inpt = _inpt;
	}

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
			if(auto s = workingSoln.getSolution(name); s.soln != 0)
			{
				// actually check if the existing solution is a fake number;
				if(s.soln->isConstantNumberType())
				{
					inpt = mergeNumberTypes(inpt, s.soln);
				}
				else if(inpt->isPolyPlaceholderType() && !s.soln->isPolyPlaceholderType())
				{
					dbgprintln("add substitution for '%s' -> '%s'", inpt, s.soln);

					workingSoln.addSubstitution(inpt, s.soln);
					inpt = s.soln;
				}
				else
				{
					return SimpleError().append(SpanError(MsgType::Note)
							.set(SimpleError::make(input.loc, "Conflicting solutions for type parameter '%s': '%s' and '%s'", name, s.soln, inpt))
							.add(SpanError::Span(s.lastLoc, strprintf("Previously solved here as '%s'", s.soln)))
							.add(SpanError::Span(input.loc, strprintf("New, conflicting solution '%s' here", inpt)))
						);
				}
			}

			// great.
			workingSoln.addSolution(name, SolvedType(input.loc, inpt));

			dbgprintln("solved %s = %s", name, inpt);
		}
	}
	else
	{
		// ok, we might need to enlist the help of the iterative solver now.

		std::vector<UnsolvedType> problemList;
		std::vector<Param_t> inputList;

		if(prob->isFunctionType())
		{
			if(!inpt->isFunctionType())
			{
				return SimpleError::make(MsgType::Note, input.loc,
					"No solution between value with type '%s' and expected type argument with function type '%s'",
					prob->str()).append(SimpleError::make(MsgType::Note, problem.loc, "here:"));
			}

			for(auto p : prob->toFunctionType()->argTypes)
				problemList.push_back(UnsolvedType(problem.loc, p));    // TODO: add PTS location!

			for(auto p : inpt->toFunctionType()->getArgumentTypes())
				inputList.push_back(Param_t("", input.loc, p));

			problemList.push_back(UnsolvedType(problem.loc, prob->toFunctionType()->returnType));
			inputList.push_back(Param_t("", input.loc, inpt->toFunctionType()->getReturnType()));
		}
		else if(prob->isTupleType())
		{
			if(!inpt->isTupleType())
			{
				return SimpleError::make(MsgType::Note, input.loc,
					"No solution between value with type '%s' and expected type argument with tuple type '%s'",
					prob->str()).append(SimpleError::make(MsgType::Note, problem.loc, "here:"));
			}

			for(auto p : prob->toTupleType()->types)
				problemList.push_back(UnsolvedType(problem.loc, p));    // TODO: add PTS location!

			for(auto p : inpt->toTupleType()->getElements())
				inputList.push_back(Param_t("", input.loc, p));
		}
		else
		{
			error(problem.loc, "unsupported problem type '%s'!!", prob->str());
		}


		return solveArgumentList(problemList, inputList, workingSoln, tosolve, problemSpace);
	}


	return SimpleError();
}


static SimpleError solveArgumentList(std::vector<UnsolvedType>& problem, const std::vector<Param_t>& input, SolutionSet& workingSoln,
	const std::set<std::string>& tosolve, const ProblemSpace_t& problemSpace)
{
	dbgprintln("{");
	_indent++;

	dbgprintln("input: %s", util::listToString(input, [](auto a) -> std::string { return a.type->str(); }));
	dbgprintln("problems: %s", util::listToString(problem, [](auto a) -> std::string { return a.type->str(); }));

	SimpleError retError;
	for(size_t i = 0; i < problem.size(); i++)
	{
		dbgprintln("arg %d: {", i); _indent++;
		auto err = solveSingleArgument(problem[i], input[i], workingSoln, tosolve, problemSpace);

		if(err.hasErrors())
			retError.append(err);

		_indent--;
		dbgprintln("}\n");
	}

	_indent--;
	dbgprintln("}");

	return retError;
}














//* this thing only infers to the best of its abilities; it cannot be guaranteed to return a complete solution.
//* please check for the completeness of said solution before using it.
std::tuple<TypeParamMap_t, std::unordered_map<fir::Type*, fir::Type*>, BareError>
	sst::TypecheckState::inferTypesForGenericEntity(ast::Parameterisable* _target, std::vector<FnCallArgument>& _input, const TypeParamMap_t& partial, fir::Type* infer, bool isFnCall)
{
	SolutionSet solution;
	for(const auto& p : partial)
		solution.addSolution(p.first, SolvedType(this->loc(), p.second));

	std::vector<Param_t> input(_input.size());
	std::vector<UnsolvedType> problem;
	auto problemSpace = _target->generics;

	if(dcast(ast::FuncDefn, _target) || dcast(ast::InitFunctionDefn, _target))
	{
		// strip out the name information, and do purely positional things.
		std::unordered_map<std::string, size_t> nameToIndex;

		fir::Type* fretty = 0;
		pts::Type* retty = 0;
		if(dcast(ast::InitFunctionDefn, _target))           error("unsupported???");
		else if(auto fd = dcast(ast::FuncDefn, _target))    retty = fd->returnType;


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

		if(isFnCall)
		{
			int counter = 0;
			for(const auto& i : _input)
			{
				if(!i.name.empty() && nameToIndex.find(i.name) == nameToIndex.end())
				{
					return { partial, { }, BareError()
						.append(SimpleError::make(MsgType::Note, i.loc, "Function '%s' does not have a parameter named '%s'",
						_target->name, i.name)).append(SimpleError::make(MsgType::Note, _target, "Function was defined here:"))
					};
				}

				input[i.name.empty() ? counter : nameToIndex[i.name]] = Param_t(i);
				counter++;
			}


			// if infer != 0, then it should be the return type.
			if(infer && !infer->isVoidType())
				fretty = infer;
		}
		else
		{
			if(infer == 0)
			{
				return { partial, { }, BareError().append(SimpleError::make(MsgType::Note, this->loc(),
					"Unable to infer type for function '%s' without additional information", _target->name)) };
			}
			else if(!infer->isFunctionType())
			{
				return { partial, { }, BareError().append(SimpleError::make(MsgType::Note, this->loc(),
					"Invalid inferred type '%s' for polymorphic entity '%s'", infer, _target->name)) };
			}
			else
			{
				// now...
				auto fty = infer->toFunctionType();
				input.resize(fty->getArgumentTypes().size());

				int ctr = 0;
				for(auto arg : fty->getArgumentTypes())
					input[ctr++] = arg;
			}
		}


		if(fretty && (retty->isNamedType() && retty->toNamedType()->name == VOID_TYPE_STRING) != fretty->isVoidType())
		{
			return { partial, { }, BareError().append(SimpleError::make(MsgType::Note, this->loc(), "Mismatch between inferred types '%s' and '%s' in return type of function '%s'", retty->str(), fretty, _target->name)) };
		}

		if(fretty)
		{
			input.push_back(Param_t("", this->loc(), fretty));
			problem.push_back(UnsolvedType(_target->loc, retty));
		}
	}
	else
	{
		return { partial, { }, BareError().append(SimpleError::make(MsgType::Note, this->loc(),
			"Unable to infer types for unsupported entity '%s'", _target->name))
		};
	}


	bool variadic = problem.back().type->isVariadicArrayType();


	if(input.size() != problem.size())
	{
		if(variadic)
		{
			error("variadic things not supported!");
		}
		else
		{
			return { partial, { }, BareError().append(SimpleError::make(MsgType::Note, this->loc(),
				"Mismatched number of arguments; expected %d, got %d instead", problem.size(), input.size()))
				.append(SimpleError::make(MsgType::Note, _target, "Function was defined here:"))
			};
		}
	}


	std::set<std::string> tosolve;
	for(const auto& p : problemSpace)
		tosolve.insert(p.first);

	dbgprintln("SESSION:");
	auto retError = solveArgumentList(problem, input, solution, tosolve, problemSpace);
	dbgprintln("\n");

	// convert the thing.
	{
		TypeParamMap_t ret;
		for(const auto& soln : solution.solutions)
		{
			auto s = soln.second.soln;
			if(s->isConstantNumberType())
				s = this->inferCorrectTypeForLiteral(s->toConstantNumberType());

			ret[soln.first] = s;
		}

		return { ret, solution.substitutions, BareError().append(retError) };
	}
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




















