// Errors.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cinttypes>
#include "include/parser.h"
#include "include/codegen.h"
#include "include/compiler.h"

using namespace Ast;

static void printContext(Codegen::CodegenInstance* cgi, uint64_t line, uint64_t col)
{
	assert(cgi->rawLines.size() > line - 1);
	std::string ln = cgi->rawLines[line - 1];

	fprintf(stderr, "%s\n", ln.c_str());

	for(uint64_t i = 1; i < col - 1; i++)
	{
		if(ln[i - 1] == '\t')
			fprintf(stderr, "\t");		// 4-wide tabs

		else
			fprintf(stderr, " ");
	}

	fprintf(stderr, "%s^%s", COLOUR_GREEN_BOLD, COLOUR_RESET);
}

static void __error_gen(Codegen::CodegenInstance* cgi, Expr* relevantast, const char* msg, const char* type, bool ex, va_list ap)
{
	char* alloc = nullptr;
	vasprintf(&alloc, msg, ap);

	auto colour = COLOUR_RED_BOLD;
	if(strcmp(type, "Warning") == 0)
		colour = COLOUR_MAGENTA_BOLD;

	uint64_t line = relevantast ? relevantast->posinfo.line : 0;
	uint64_t col = relevantast ? relevantast->posinfo.col : 0;

	if(line > 0 && col > 0 && relevantast)
		fprintf(stderr, "%s(%s:%" PRIu64 ":%" PRIu64 ") ", COLOUR_BLACK_BOLD, relevantast->posinfo.file.c_str(), line, col);

	fprintf(stderr, "%s%s%s: %s\n", colour, type, COLOUR_RESET, alloc);

	if(cgi && line > 0 && col > 0)
		printContext(cgi, line, col);

	fprintf(stderr, "\n");

	va_end(ap);
	if(ex)
	{
		fprintf(stderr, "There were errors, compilation cannot continue\n");
		abort();
	}
}

void error(Codegen::CodegenInstance* cgi, Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(cgi, relevantast, msg, "Error", true, ap);
	abort();
}

void error(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(nullptr, relevantast, msg, "Error", true, ap);
	abort();
}

void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, nullptr, msg, "Error", true, ap);
	abort();
}


void warn(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, nullptr, msg, "Warning", false, ap);

	if(Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
		error("Treating warning as error because -Werror was passed");
}

void warn(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(nullptr, relevantast, msg, "Warning", false, ap);


	if(Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
		error("Treating warning as error because -Werror was passed");
}

void warn(Codegen::CodegenInstance* cgi, Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(cgi, relevantast, msg, "Warning", false, ap);


	if(Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
		error("Treating warning as error because -Werror was passed");
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

	void unknownSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st)
	{
		error(cgi, e, "Using undeclared %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void useAfterFree(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname)
	{
		warn(cgi, e, "Attempted to use variable %s after it was deallocated", symname.c_str());
	}

	void duplicateSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st)
	{
		error(cgi, e, "Duplicate %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void noOpOverload(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, Ast::ArithmeticOp op)
	{
		error(cgi, e, "No valid operator overload for %s on type %s", Parser::arithmeticOpToString(op).c_str(), type.c_str());
	}

	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, llvm::Type* a, llvm::Type* b)
	{
		// note: HACK
		// C++ does static function resolution on struct members, so as long as getReadableType() doesn't use
		// the 'this' pointer (it doesn't) we'll be fine.
		// thus, we don't check whether cgi is null.

		error(cgi, e, "Invalid assignment from type %s to %s", cgi->getReadableType(b).c_str(),
			cgi->getReadableType(a).c_str());
	}

	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, llvm::Value* a, llvm::Value* b)
	{
		invalidAssignment(cgi, e, a->getType(), b->getType());
	}

	void invalidInitialiser(Codegen::CodegenInstance* cgi, Ast::Expr* e, Ast::Struct* str, std::vector<llvm::Value*> args)
	{
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

		error(cgi, e, "No valid init() candidate for type %s taking parameters [%s]", str->name.c_str(), args_str.c_str());
	}

	void expected(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string expect)
	{
		error(cgi, e, "Expected %s", expect.c_str());
	}
}
















