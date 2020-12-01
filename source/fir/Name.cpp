// Name.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	bool Name::operator== (const Name& other) const
	{
		return this->name == other.name
			&& this->scope == other.scope
			&& this->params == other.params
			&& this->retty == other.retty
			&& this->kind == other.kind;
	}

	bool Name::operator!= (const Name& other) const
	{
		return !(*this == other);
	}

	Name Name::of(std::string name)
	{
		return Name(NameKind::Name, name, { }, { }, nullptr);
	}

	Name Name::of(std::string name, std::vector<std::string> scope)
	{
		return Name(NameKind::Name, name, scope, { }, nullptr);
	}

	Name Name::type(std::string name)
	{
		return Name(NameKind::Type, name, { }, { }, nullptr);
	}

	Name Name::type(std::string name, std::vector<std::string> scope)
	{
		return Name(NameKind::Type, name, scope, { }, nullptr);
	}

	Name Name::function(std::string name, std::vector<fir::Type*> params, fir::Type* retty)
	{
		return Name(NameKind::Function, name, { }, params, retty);
	}

	Name Name::function(std::string name, std::vector<std::string> scope, std::vector<fir::Type*> params, fir::Type* retty)
	{
		return Name(NameKind::Function, name, scope, params, retty);
	}





	static std::string mangleScopeOnly(const fir::Name& id)
	{
		bool first = true;
		std::string ret;
		for(const auto& s : id.scope)
		{
			ret += (!first ? std::to_string(s.length()) : "") + s;
			first = false;
		}

		return ret;
	}

	static inline std::string lentypestr(const std::string& s)
	{
		return std::to_string(s.length()) + s;
	}

	static std::string mangleScopeName(const fir::Name& id)
	{
		return mangleScopeOnly(id) + lentypestr(id.name);
	}

	static std::string mangleType(fir::Type* t)
	{
		if(t->isPrimitiveType())
		{
			return lentypestr(t->encodedStr());
		}
		if(t->isBoolType())
		{
			return lentypestr(t->encodedStr());
		}
		else if(t->isArrayType())
		{
			return "FA" + lentypestr(mangleType(t->getArrayElementType())) + std::to_string(t->toArrayType()->getArraySize());
		}
		else if(t->isDynamicArrayType())
		{
			return "DA" + lentypestr(mangleType(t->getArrayElementType()));
		}
		else if(t->isArraySliceType())
		{
			return "SL" + lentypestr(mangleType(t->getArrayElementType()));
		}
		else if(t->isVoidType())
		{
			return "v";
		}
		else if(t->isFunctionType())
		{
			std::string ret = "FN" + std::to_string(t->toFunctionType()->getArgumentCount()) + "FA";
			for(auto a : t->toFunctionType()->getArgumentTypes())
			{
				ret += lentypestr(mangleType(a));
			}

			if(t->toFunctionType()->getArgumentTypes().empty())
				ret += "v";

			return ret;
		}
		else if(t->isStructType())
		{
			return lentypestr(mangleScopeName(t->toStructType()->getTypeName()));
		}
		else if(t->isClassType())
		{
			return lentypestr(mangleScopeName(t->toClassType()->getTypeName()));
		}
		else if(t->isTupleType())
		{
			std::string ret = "ST" + std::to_string(t->toTupleType()->getElementCount()) + "SM";
			for(auto m : t->toTupleType()->getElements())
				ret += lentypestr(mangleType(m));

			return ret;
		}
		else if(t->isPointerType())
		{
			return "PT" + lentypestr(mangleType(t->getPointerElementType()));
		}
		else if(t->isStringType())
		{
			return "SR";
		}
		else if(t->isCharType())
		{
			return "CH";
		}
		else if(t->isEnumType())
		{
			return "EN" + lentypestr(mangleType(t->toEnumType()->getCaseType())) + lentypestr(mangleScopeName(t->toEnumType()->getTypeName()));
		}
		else if(t->isAnyType())
		{
			return "AY";
		}
		else
		{
			_error_and_exit("unsupported ir type??? ('%s')\n", t);
		}
	}

	static std::string mangleName(const Name& id, bool includeScope)
	{
		if(id.kind == NameKind::Name || id.kind == NameKind::Type)
		{
			std::string scp;
			if(includeScope)
				scp += mangleScopeOnly(id);

			if(includeScope && id.scope.size() > 0)
				scp += std::to_string(id.name.length());

			return scp + id.name;
		}
		else if(!includeScope)
		{
			if(id.kind == NameKind::Function)
			{
				std::string ret = id.name + "(";

				if(id.params.empty())
				{
					ret += ")";
				}
				else
				{
					for(auto t : id.params)
						ret += t->str() + ",";

					ret = ret.substr(0, ret.length() - 1);
					ret += ")";
				}

				return ret;
			}
			else
			{
				_error_and_exit("invalid\n");
			}
		}
		else
		{
			std::string ret = "_F";

			if(id.kind == NameKind::Function)     ret += "F";
			else if(id.kind == NameKind::Type)    ret += "T";
			else                                ret += "U";

			if(includeScope)
				ret += mangleScopeOnly(id);

			ret += lentypestr(id.name);

			if(id.kind == NameKind::Function)
			{
				ret += "_FA";
				for(auto t : id.params)
					ret += "_" + mangleType(t);

				if(id.params.empty())
					ret += "v";
			}

			return ret;
		}
	}

	std::string Name::str() const
	{
		std::string ret = zfu::join(this->scope, ".");
		ret += this->name;

		if(this->kind == NameKind::Function)
		{
			ret += "(";
			for(const auto& p : this->params)
				ret += p->str() + ", ";

			if(this->params.size() > 0)
				ret.pop_back(), ret.pop_back();

			ret += ")";
		}

		return ret;
	}

	std::string Name::mangled() const
	{
		return mangleName(*this, true);
	}

	std::string Name::mangledWithoutScope() const
	{
		return mangleName(*this, false);
	}

	std::string obfuscateName(const std::string& name)
	{
		return zpr::sprint("__#%s", name);
	}
	std::string obfuscateName(const std::string& name, size_t id)
	{
		return zpr::sprint("__#%s_%d", name, id);
	}
	std::string obfuscateName(const std::string& name, const std::string& extra)
	{
		return zpr::sprint("__#%s_%s", name, extra);
	}

	Name Name::obfuscate(const std::string& name, NameKind kind)
	{
		return Name(kind, obfuscateName(name), { }, { }, nullptr);
	}
	Name Name::obfuscate(const std::string& name, size_t id, NameKind kind)
	{
		return Name(kind, obfuscateName(name, id), { }, { }, nullptr);
	}
	Name Name::obfuscate(const std::string& name, const std::string& extra, NameKind kind)
	{
		return Name(kind, obfuscateName(name, extra), { }, { }, nullptr);
	}
}

namespace zpr
{
	std::string print_formatter<fir::Name>::print(const fir::Name& x, const format_args& args)
	{
		return x.str();
	}
}
