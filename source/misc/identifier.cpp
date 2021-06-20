// identifier.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "sst.h"
#include "frontend.h"

#include "ir/type.h"


sst::Stmt* TCResult::stmt() const
{
	if(this->_kind == RK::Error)
	{
		this->_pe->postAndQuit();
		// throw ErrorException(this->_pe);
	}

	switch(this->_kind)
	{
		case RK::Statement:     return this->_st;
		case RK::Expression:    return this->_ex;
		case RK::Definition:    return this->_df;
		default:                _error_and_exit("not stmt\n");
	}
}

sst::Expr* TCResult::expr() const
{
	if(this->_kind == RK::Error)
	{
		this->_pe->postAndQuit();
		// throw ErrorException(this->_pe);
	}

	if(this->_kind != RK::Expression)
		_error_and_exit("not expr\n");

	return this->_ex;
}

sst::Defn* TCResult::defn() const
{
	if(this->_kind == RK::Error)
	{
		this->_pe->postAndQuit();
		// throw ErrorException(this->_pe);
	}

	if(this->_kind != RK::Definition)
		_error_and_exit("not defn\n");

	return this->_df;
}




void PolyArgMapping_t::add(const std::string& name, pts::Type* t)
{
	SingleArg arg;
	arg.name = name;
	arg.type = t;
	arg.index = static_cast<size_t>(-1);

	this->maps.push_back(arg);
}

void PolyArgMapping_t::add(size_t idx, pts::Type* t)
{
	SingleArg arg;
	arg.name = "";
	arg.type = t;
	arg.index = idx;

	this->maps.push_back(arg);
}














bool Identifier::operator == (const Identifier& other) const
{
	return (other.name == this->name) && (other.str() == this->str());
}
bool Identifier::operator != (const Identifier& other) const
{
	return !(other == *this);
}


std::string Identifier::str() const
{
	std::string ret;
	for(const auto& s : this->scope.components())
		ret += s + ".";

	ret += this->name;

	if(this->kind == IdKind::Function)
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

fir::Name Identifier::convertToName() const
{
	switch(this->kind)
	{
		case IdKind::Name: return fir::Name::of(this->name, this->scope.components());
		case IdKind::Type: return fir::Name::type(this->name, this->scope.components());
		case IdKind::Function: return fir::Name::function(this->name, this->scope.components(), this->params, this->returnType);
		default: iceAssert(0 && "invalid identifier");
	}
}

std::string Location::toString() const
{
	return strprintf("(%s:%d:%d)", frontend::getFilenameFromID(this->fileID), this->line + 1, this->col + 1);
}

std::string Location::shortString() const
{
	return strprintf("(%s:%d:%d)", frontend::getFilenameFromPath(frontend::getFilenameFromID(this->fileID)),
		this->line + 1, this->col + 1);
}






namespace util
{
	std::string typeParamMapToString(const std::string& name, const TypeParamMap_t& map)
	{
		if(map.empty())
			return name;

		std::string ret;
		for(auto m : map)
			ret += (m.first + ":" + m.second->encodedStr()) + ",";

		// shouldn't be empty.
		iceAssert(ret.size() > 0);
		return strprintf("%s<%s>", name, ret.substr(0, ret.length() - 1));
	}
}

namespace zpr
{
	std::string print_formatter<Identifier>::print(const Identifier& x, const format_args&)
	{
		return x.str();
	}

	std::string print_formatter<VisibilityLevel>::print(const VisibilityLevel& x, const format_args&)
	{
		switch(x)
		{
			case VisibilityLevel::Invalid:  return "invalid";
			case VisibilityLevel::Public:   return "public";
			case VisibilityLevel::Private:  return "private";
			case VisibilityLevel::Internal: return "internal";
			default:                        return "unknown";
		}
	}
}




