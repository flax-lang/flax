// Errors.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/codegen.h"
#include "../include/compiler.h"

using namespace Ast;

void __error_gen(Expr* relevantast, const char* msg, const char* type, bool ex, va_list ap)
{
	char* alloc = nullptr;
	vasprintf(&alloc, msg, ap);

	fprintf(stderr, "%s(%s:%" PRId64 ")%s Error%s: %s\n\n", COLOUR_BLACK_BOLD, relevantast ? relevantast->posinfo.file.c_str() : "?", relevantast ? relevantast->posinfo.line : 0, COLOUR_RED_BOLD, COLOUR_RESET, alloc);

	va_end(ap);
	if(ex) abort();
}

void error(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(relevantast, msg, "Error", true, ap);
}

void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, msg, "Error", true, ap);
}


void warn(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, msg, "Warning", false, ap);
}


void warn(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(relevantast, msg, "Warning", false, ap);
}



namespace GenError
{
	static const char* SymbolTypeNames[] =
	{
		"identifier",
		"function",
		"variable",
		"type"
	};

	void unknownSymbol(Ast::Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Using undeclared %s '%s'", SymbolTypeNames[(int) st], symname.c_str());
	}

	void useAfterFree(Ast::Expr* e, std::string symname)
	{
		error(e, "Attempted to use variable '%s' after it was deallocated", symname.c_str());
	}

	void duplicateSymbol(Ast::Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Duplicate %s '%s'", SymbolTypeNames[(int) st], symname.c_str());
	}
}
















