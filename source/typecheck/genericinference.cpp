// genericinference.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "sst.h"
#include "ir/type.h"
#include "typecheck.h"


struct UnsolvedType
{
	std::string generic;
	bool ismutable = false;
	int pointers = 0;

	fir::Type* solution = 0;
};


//* this thing only infers to the best of its abilities; it cannot be guaranteed to return a complete solution.
//* please check for the completeness of said solution before using it.
//* thanks, future me.
std::pair<TypeParamMap_t, ErrorMsg*> sst::TypecheckState::inferTypesForGenericEntity(ast::Parameterisable* problem,
	const std::vector<fir::Type*>& input, const TypeParamMap_t& partial)
{
	// duh.
	TypeParamMap_t solution = partial;



	return { solution, new BareError() };
}




























