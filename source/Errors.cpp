// Errors.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cinttypes>
#include "include/codegen.h"
#include "include/compiler.h"

using namespace Ast;

void __error_gen(Expr* relevantast, const char* msg, const char* type, bool ex, va_list ap)
{
	char* alloc = nullptr;
	vasprintf(&alloc, msg, ap);

	fprintf(stderr, "%s(%s:%" PRIu64 ")%s %s%s: %s\n\n", COLOUR_BLACK_BOLD, relevantast ? relevantast->posinfo.file.c_str() : "?", relevantast ? relevantast->posinfo.line : 0, COLOUR_RED_BOLD, type, COLOUR_RESET, alloc);

	va_end(ap);
	if(ex) abort();
}

void error(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(relevantast, msg, "Error", true, ap);
	abort();
}

void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, msg, "Error", true, ap);
	abort();
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
		error(e, "Using undeclared %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void useAfterFree(Ast::Expr* e, std::string symname)
	{
		error(e, "Attempted to use variable %s after it was deallocated", symname.c_str());
	}

	void duplicateSymbol(Ast::Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Duplicate %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void noOpOverload(Ast::Expr* e, std::string type, Ast::ArithmeticOp op)
	{
		error(e, "No valid operator overload for %s on type %s", Parser::arithmeticOpToString(op).c_str(), type.c_str());
	}

	void invalidAssignment(Ast::Expr* e, llvm::Type* a, llvm::Type* b)
	{
		// note: HACK
		// C++ does static function resolution on struct members, so as long as getReadableType() doesn't use
		// the 'this' pointer (it doesn't) we'll be fine.
		Codegen::CodegenInstance* cgi = 0;

		error(e, "Invalid assignment from type %s to %s", cgi->getReadableType(a).c_str(),
			cgi->getReadableType(b).c_str());
	}

	void invalidAssignment(Ast::Expr* e, llvm::Value* a, llvm::Value* b)
	{
		invalidAssignment(e, a->getType(), b->getType());
	}

	void invalidInitialiser(Ast::Expr* e, Ast::Struct* str, std::vector<llvm::Value*> args)
	{
		// same hack as above
		Codegen::CodegenInstance* cgi = 0;


		std::string args_str;
		for(llvm::Value* v : args)
		{
			if(!v || args[0] == v)
				continue;

			args_str += ", " + cgi->getReadableType(v->getType());
		}

		// remove leading commas
		if(args_str.length() > 2)
			args_str = args_str.substr(2);

		error(e, "No valid init() candidate for type %s taking parameters [%s]", str->name.c_str(), args_str.c_str());
	}
}
















