// misc.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "ir/type.h"
#include "typecheck.h"

#include "errors.h"
#include "polymorph.h"

namespace sst
{
	namespace poly
	{
		namespace internal
		{
			static int polySessionId = 0;
			int getNextSessionId()
			{
				return polySessionId++;
			}

			fir::Type* mergeNumberTypes(fir::Type* a, fir::Type* b)
			{
				if(a->isFloatingPointType() && b->isIntegerType())
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


			fir::Type* convertPtsType(TypecheckState* fs, const ProblemSpace_t& problems, pts::Type* input, int polysession)
			{
				fir::Type* fty = 0;
				auto [ _ty, trfs ] = decomposeIntoTransforms(input);
				pts::Type* ty = _ty;

				if(ty->isNamedType())
				{
					fty = fs->convertParserTypeToFIR(ty, true);
					if(!fty && (std::find_if(problems.begin(), problems.end(), [ty](const auto& a) -> bool {
							return ty->toNamedType()->name == a.first;
						}) != problems.end()))
					{
						fty = fir::PolyPlaceholderType::get(ty->toNamedType()->name, polysession);
					}

					if(!fty) error(fs->loc(), "failed to find type '%s'", input->str());
				}
				else if(ty->isTupleType())
				{
					fty = fir::TupleType::get(convertPtsTypeList(fs, problems, ty->toTupleType()->types, polysession));
				}
				else if(ty->isFunctionType())
				{
					fty = fir::FunctionType::get(convertPtsTypeList(fs, problems, ty->toFunctionType()->argTypes, polysession),
						convertPtsType(fs, problems, ty->toFunctionType()->returnType, polysession));
				}
				else
				{
					error("unsupported pts type '%s'", ty->str());
				}

				return applyTransforms(fty, trfs);
			}

			std::vector<fir::Type*> convertPtsTypeList(TypecheckState* fs, const ProblemSpace_t& problems, const std::vector<pts::Type*>& input,
				int polysession)
			{
				// mm, smells functional.
				return zfu::map(input, [fs, problems, polysession](pts::Type* pt) -> fir::Type* {
					return convertPtsType(fs, problems, pt, polysession);
				});
			}



			std::vector<ArgType> unwrapFunctionParameters(TypecheckState* fs, const ProblemSpace_t& problems,
				const std::vector<ast::FuncDefn::Param>& args, int polysession)
			{
				return zfu::mapIdx(convertPtsTypeList(fs, problems, zfu::map(args,
					[](const ast::FuncDefn::Param& a) -> pts::Type* {
						return a.type;
					}
				), polysession), [args](fir::Type* t, size_t idx) -> ArgType {
					return ArgType(args[idx].name, t, args[idx].loc);
				});
			}
		}
	}





	std::vector<TypeParamMap_t> TypecheckState::getGenericContextStack()
	{
		return this->genericContextStack;
	}

	void TypecheckState::pushGenericContext()
	{
		this->genericContextStack.push_back({ });
	}

	void TypecheckState::addGenericMapping(const std::string& name, fir::Type* ty)
	{
		iceAssert(this->genericContextStack.size() > 0);
		if(auto it = this->genericContextStack.back().find(name); it != this->genericContextStack.back().end())
			error(this->loc(), "mapping for type parameter '%s' already exists in current context (is currently '%s')", name, it->second);

		this->genericContextStack.back()[name] = ty;
	}

	void TypecheckState::removeGenericMapping(const std::string& name)
	{
		iceAssert(this->genericContextStack.size() > 0);
		if(auto it = this->genericContextStack.back().find(name); it == this->genericContextStack.back().end())
			error(this->loc(), "no mapping for type parameter '%s' exists in current context, cannot remove", name);

		else
			this->genericContextStack.back().erase(it);
	}

	void TypecheckState::popGenericContext()
	{
		iceAssert(this->genericContextStack.size() > 0);
		this->genericContextStack.pop_back();
	}


	fir::Type* TypecheckState::findGenericMapping(const std::string& name, bool allowFail)
	{
		// look upwards.
		for(auto it = this->genericContextStack.rbegin(); it != this->genericContextStack.rend(); it++)
			if(auto iit = it->find(name); iit != it->end())
				return iit->second;

		if(allowFail)   return 0;
		else            error(this->loc(), "no mapping for type parameter '%s'", name);
	}
}


//* helper method that abstracts away the common error-checking
std::pair<bool, sst::Defn*> ast::Parameterisable::checkForExistingDeclaration(sst::TypecheckState* fs, const TypeParamMap_t& gmaps)
{
	//! ACHTUNG !
	//* IMPORTANT *
	/*
		? the reason we match the *ENTIRE* generic context stack when checking for an existing definition is because of nesting.
		* if we only checked the current map, then for methods of generic types and/or nested, non-generic types inside generic types,
		* we'd match an existing definition even though all the generic types are probably completely different.

		* so, pretty much the only way to make sure we're absolutely certain it's the same context is to compare the entire type stack.

		? given that a given definition cannot 'move' to another scope, there cannot be circumstances where we can (by chance or otherwise)
		? be typechecking the current definition in another, completely different context, and somehow mistake it for our own -- even if all
		? the generic types match in the stack.

		* note: bug fix: what we should really be checking for is that the stored generic map is a strict child (ie. the last N elements match
		* our stored state, while the preceding ones don't matter). (this is why we use reverse iterators for std::equal)

		* note: bug fix of the bug fix: we should be checking whether our current state is a child of the stored state, instead of the
		* other way around. probably.
	*/

	{
		auto doRootsMatch = [](const std::vector<TypeParamMap_t>& expected, const std::vector<TypeParamMap_t>& given) -> bool {
			if(given.size() > expected.size())
				return false;

			//* reverse iterators
			return std::equal(given.rbegin(), given.rend(), expected.rbegin());
		};

		auto currentGCS = fs->getGenericContextStack();
		if(!gmaps.empty())
			currentGCS.push_back(gmaps);

		for(const auto& gv : this->genericVersions)
		{
			//* note!! Defn::type can be null for enums -- we need to find a way to prevent this!!
			// TODO: prevent this!!
			// TODO: prevent this!!
			// TODO: prevent this!!
			// TODO: prevent this!!
			if(gv.first->type && !gv.first->type->containsPlaceholders() && doRootsMatch(gv.second, currentGCS))
				return { true, gv.first };
		}


		if(this->generics.size() > 0 && gmaps.empty())
		{
			if(const auto& tys = fs->stree->unresolvedGenericDefs[this->name]; std::find(tys.begin(), tys.end(), this) == tys.end())
				fs->stree->unresolvedGenericDefs[this->name].push_back(this);

			return { false, 0 };
		}
		else
		{
			//? note: if we call with an empty map, then this is just a non-generic type/function/thing. Even for such things,
			//? the genericVersions list will have 1 entry which is just the type itself.
			return { true, 0 };
		}
	}
}


std::string PolyArgMapping_t::print() const
{
	std::string ret;
	for(const auto& m : this->maps)
	{
		ret += ", ";
		if(!m.name.empty())
			ret += strprintf("%s: ", m.name);

		ret += m.type->str();
	}

	if(!ret.empty())
		ret = ret.erase(0, 2);

	return ret;
}


















