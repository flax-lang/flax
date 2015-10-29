// Errors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <inttypes.h>

#include "errors.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"

using namespace Ast;

namespace GenError
{
	void printContext(std::string file, uint64_t line, uint64_t col, uint64_t len)
	{
		std::vector<std::string> lines = Compiler::getFileLines(file);
		if(lines.size() > line - 1)
		{
			std::string orig = lines[line - 1];
			std::string ln;

			for(auto c : orig)
			{
				if(c == '\t')
				{
					for(size_t i = 0; i < TAB_WIDTH; i++)
						ln += " ";
				}
				else
				{
					ln += c;
				}
			}


			fprintf(stderr, "%s\n", ln.c_str());

			for(uint64_t i = 1; i < col - 1; i++)
			{
				if(ln[i - 1] == '\t')
				{
					for(size_t i = 0; i < TAB_WIDTH; i++)
						fprintf(stderr, " ");
				}
				else
				{
					fprintf(stderr, " ");
				}
			}

			std::string tildes;
			// for(size_t i = 0; i < len; i++)
			// 	tildes += "~";

			fprintf(stderr, "%s^%s%s", COLOUR_GREEN_BOLD, tildes.c_str(), COLOUR_RESET);
		}
		else
		{
			fprintf(stderr, "(no context)");
		}
	}

	void printContext(Expr* e)
	{
		if(e->pin.line > 0)
			printContext(e->pin.file, e->pin.line, e->pin.col, e->pin.len);
	}
}


void __error_gen(uint64_t line, uint64_t col, uint64_t len, const char* file, const char* msg,
	const char* type, bool doExit, va_list ap)
{
	if(strcmp(type, "Warning") == 0 && Compiler::getFlag(Compiler::Flag::NoWarnings))
		return;


	char* alloc = nullptr;
	vasprintf(&alloc, msg, ap);

	auto colour = COLOUR_RED_BOLD;
	if(strcmp(type, "Warning") == 0)
		colour = COLOUR_MAGENTA_BOLD;

	else if(strcmp(type, "Note") == 0)
		colour = COLOUR_GREY_BOLD;

	// todo: do we want to truncate the file path?
	// we're doing it now, might want to change (or use a flag)

	std::string filename = Compiler::getFilenameFromPath(file);

	if(line > 0 && col > 0)
		fprintf(stderr, "%s(%s:%" PRIu64 ":%" PRIu64 ") ", COLOUR_BLACK_BOLD, filename.c_str(), line, col);

	fprintf(stderr, "%s%s%s: %s\n", colour, type, COLOUR_RESET, alloc);

	if(line > 0 && col > 0)
	{
		std::vector<std::string> lines;
		if(strcmp(file, "") != 0)
		{
			GenError::printContext(file, line, col, len);
		}
	}

	fprintf(stderr, "\n");

	va_end(ap);
	free(alloc);

	if(doExit)
	{
		fprintf(stderr, "There were errors, compilation cannot continue\n");
		abort();
	}
	else if(strcmp(type, "Warning") == 0 && Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
	{
		error("Treating warning as error because -Werror was passed");
	}
}

void error(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	const char* file	= relevantast ? relevantast->pin.file : "";
	uint64_t line		= relevantast ? relevantast->pin.line : 0;
	uint64_t col		= relevantast ? relevantast->pin.col : 0;
	uint64_t len		= relevantast ? relevantast->pin.len : 0;

	__error_gen(line, col, len, file, msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(0, 0, 0, "", msg, "Error", true, ap);
	va_end(ap);
	abort();
}







void warn(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(0, 0, 0, "", msg, "Warning", false, ap);
	va_end(ap);
}

void warn(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	const char* file	= relevantast ? relevantast->pin.file : "";
	uint64_t line		= relevantast ? relevantast->pin.line : 0;
	uint64_t col		= relevantast ? relevantast->pin.col : 0;
	uint64_t len		= relevantast ? relevantast->pin.len : 0;

	__error_gen(line, col, len, file, msg, "Warning", false, ap);
	va_end(ap);
}



void info(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(0, 0, 0, "", msg, "Note", false, ap);
	va_end(ap);
}

void info(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	const char* file	= relevantast ? relevantast->pin.file : "";
	uint64_t line		= relevantast ? relevantast->pin.line : 0;
	uint64_t col		= relevantast ? relevantast->pin.col : 0;
	uint64_t len		= relevantast ? relevantast->pin.len : 0;

	__error_gen(line, col, len, file, msg, "Note", false, ap);
	va_end(ap);
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

	void unknownSymbol(Codegen::CodegenInstance* cgi, Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Using undeclared %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void duplicateSymbol(Codegen::CodegenInstance* cgi, Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Duplicate %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void noOpOverload(Codegen::CodegenInstance* cgi, Expr* e, std::string type, ArithmeticOp op)
	{
		error(e, "No valid operator overload for %s on type %s", Parser::arithmeticOpToString(cgi, op).c_str(), type.c_str());
	}

	void invalidAssignment(Codegen::CodegenInstance* cgi, Expr* e, fir::Type* a, fir::Type* b)
	{
		// note: HACK
		// C++ does static function resolution on struct members, so as long as getReadableType() doesn't use
		// the 'this' pointer (it doesn't) we'll be fine.
		// thus, we don't check whether cgi is null.

		error(e, "Invalid assignment from type %s to %s", cgi->getReadableType(b).c_str(),
			cgi->getReadableType(a).c_str());
	}

	void invalidAssignment(Codegen::CodegenInstance* cgi, Expr* e, fir::Value* a, fir::Value* b)
	{
		invalidAssignment(cgi, e, a->getType(), b->getType());
	}

	void invalidInitialiser(Codegen::CodegenInstance* cgi, Expr* e, std::string name, std::vector<fir::Value*> args)
	{
		std::string args_str;
		for(fir::Value* v : args)
		{
			if(!v || args[0] == v)
				continue;

			args_str += ", " + cgi->getReadableType(v->getType());
		}

		// remove leading commas
		if(args_str.length() > 2)
			args_str = args_str.substr(2);

		error(e, "No valid init() candidate for type %s taking parameters [ %s ]", name.c_str(), args_str.c_str());
	}

	void expected(Codegen::CodegenInstance* cgi, Expr* e, std::string expect)
	{
		error(e, "Expected %s", expect.c_str());
	}

	void nullValue(Codegen::CodegenInstance* cgi, Expr* e, int funcArgument)
	{
		if(funcArgument >= 0)
			error(e, "Invalid (void) value in argument %d of function call", funcArgument + 1);

		else
			error(e, "Invalid (void) value");
	}

	void noSuchMember(Codegen::CodegenInstance* cgi, Expr* e, std::string type, std::string member)
	{
		error(e, "Type %s does not have a member '%s'", type.c_str(), member.c_str());
	}

	void noFunctionTakingParams(Codegen::CodegenInstance* cgi, Expr* e, std::string type, std::string name, std::deque<Expr*> ps)
	{
		std::string prs = "";
		for(auto p : ps)
			prs += cgi->getReadableType(p) + ", ";

		if(prs.size() > 0) prs = prs.substr(0, prs.size() - 2);

		error(e, "%s does not contain a function %s taking parameters (%s)", type.c_str(), name.c_str(), prs.c_str());
	}
}
















