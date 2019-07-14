// transforms.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ir/type.h"

#include "errors.h"
#include "polymorph.h"

namespace sst {
namespace poly
{

	void Solution_t::addSolution(const std::string& x, const fir::LocatedType& y)
	{
		this->solutions[x] = y.type;
	}

	void Solution_t::addSubstitution(fir::Type* x, fir::Type* y)
	{
		if(auto it = this->substitutions.find(x); it != this->substitutions.end())
		{
			if(it->second != y) error("conflicting substitutions for '%s': '%s' and '%s'", x, y, it->second);
			debuglogln("substitution: '%s' -> '%s'", x, y);
		}

		this->substitutions[x] = y;
	}

	bool Solution_t::hasSolution(const std::string& n) const
	{
		return this->solutions.find(n) != this->solutions.end();
	}

	fir::LocatedType Solution_t::getSolution(const std::string& n) const
	{
		if(auto it = this->solutions.find(n); it != this->solutions.end())
			return fir::LocatedType(it->second);

		else
			return fir::LocatedType(0);
	}

	fir::Type* Solution_t::substitute(fir::Type* x) const
	{
		if(auto it = this->substitutions.find(x); it != this->substitutions.end())
			return it->second;

		else
			return x;
	}

	void Solution_t::resubstituteIntoSolutions()
	{
		// iterate through everything
		for(auto& [ n, t ] : this->solutions)
			t = this->substitute(t);
	}

	bool Solution_t::operator == (const Solution_t& other) const
	{
		return other.distance == this->distance
			&& other.solutions == this->solutions
			&& other.substitutions == this->substitutions;
	}

	bool Solution_t::operator != (const Solution_t& other) const
	{
		return !(other == *this);
	}


	namespace internal
	{
		std::pair<fir::Type*, std::vector<Trf>> decomposeIntoTransforms(fir::Type* t, size_t max)
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
					if(t->isVariadicArrayType())    ret.push_back(Trf(TrfType::VariadicArray));
					else                            ret.push_back(Trf(TrfType::Slice, t->toArraySliceType()->isMutable()));

					t = t->getArrayElementType();
				}
				else if(t->isArrayType())
				{
					ret.push_back(Trf(TrfType::FixedArray, t->toArrayType()->getArraySize()));
					t = t->getArrayElementType();
				}
				else if(t->isPointerType())
				{
					ret.push_back(Trf(TrfType::Pointer, t->isMutablePointer()));
					t = t->getPointerElementType();
				}
				else
				{
					break;
				}
			}

			return { t, ret };
		}

		std::pair<pts::Type*, std::vector<Trf>> decomposeIntoTransforms(pts::Type* t)
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
					ret.push_back(Trf(TrfType::Pointer, t->toPointerType()->isMutable));
					t = t->toPointerType()->base;
				}
				else if(t->isVariadicArrayType())
				{
					ret.push_back(Trf(TrfType::VariadicArray));
					t = t->toVariadicArrayType()->base;
				}
				else
				{
					break;
				}
			}

			return { t, ret };
		}


		fir::Type* applyTransforms(fir::Type* base, const std::vector<Trf>& trfs)
		{
			for(auto it = trfs.rbegin(); it != trfs.rend(); it++)
			{
				switch(it->type)
				{
					case TrfType::None:
						break;
					case TrfType::Slice:
						base = fir::ArraySliceType::get(base, (bool) it->data);
						break;
					case TrfType::Pointer:
						base = base->getPointerTo();
						if((bool) it->data) base = base->getMutablePointerVersion();
						break;
					case TrfType::FixedArray:
						base = fir::ArrayType::get(base, it->data);
						break;
					case TrfType::DynamicArray:
						base = fir::DynamicArrayType::get(base);
						break;
					case TrfType::VariadicArray:
						base = fir::ArraySliceType::getVariadic(base);
						break;
					default:
						error("unsupported transformation '%d'", (int) it->type);
				}
			}
			return base;
		}
	}
}
}

















