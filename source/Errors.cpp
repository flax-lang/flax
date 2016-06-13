// Errors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <inttypes.h>

#include "errors.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"

#include <signal.h>

using namespace Ast;
using namespace Codegen;

namespace GenError
{
	static void printContext(HighlightOptions ops)
	{
		std::vector<std::string> lines = Compiler::getFileLines(ops.caret.file);
		if(lines.size() > ops.caret.line - 1)
		{
			std::string orig = lines[ops.caret.line - 1];
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

			size_t cursorX = 1;

			if(ops.caret.col > 0 && ops.drawCaret)
			{
				for(uint64_t i = 1; i <= ops.caret.col - 1; i++)
				{
					if(ln[i - 1] == '\t')
					{
						for(size_t j = 0; j < TAB_WIDTH; j++)
						{
							fprintf(stderr, " ");
							cursorX++;
						}
					}
					else
					{
						fprintf(stderr, " ");
						cursorX++;
					}
				}

				// move the caret to the "middle" or average of the entire token
				for(size_t i = 0; i < ops.caret.len / 2; i++)
				{
					fprintf(stderr, " ");
					cursorX++;
				}

				cursorX++;
				fprintf(stderr, "%s^%s", COLOUR_GREEN_BOLD, COLOUR_RESET);
			}


			// sort in reverse order
			// since we can use \b to move left, without actually erasing the cursor
			// but ' ' doesn't work that way
			std::sort(ops.underlines.begin(), ops.underlines.end(), [](Parser::Pin a, Parser::Pin b) { return a.col < b.col; });
			for(auto ul : ops.underlines)
			{
				// fprintf(stderr, "col = %d, x = %d\n", ul.col, cursorX);
				while(ul.col < cursorX)
				{
					cursorX--;
					fprintf(stderr, "\b");
				}

				while(ul.col > cursorX)
				{
					cursorX++;
					fprintf(stderr, " ");
				}


				for(size_t i = 0; i < ul.len; i++)
				{
					// ̅, ﹋, ̅
					fprintf(stderr, "%s̅%s", COLOUR_GREEN_BOLD, COLOUR_RESET);
					// fprintf(stderr, "%s-%s", COLOUR_GREEN_BOLD, COLOUR_RESET);
					cursorX++;
				}
			}
		}
		else
		{
			fprintf(stderr, "(no context)");
		}
	}
}


__attribute__ ((noreturn)) static void doTheExit()
{
	fprintf(stderr, "There were errors, compilation cannot continue\n");
	abort();
}

void __error_gen(HighlightOptions ops, const char* msg, const char* type, bool doExit, va_list ap)
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

	std::string filename = Compiler::getFilenameFromPath(ops.caret.file.empty() ? "(unknown)" : ops.caret.file);

	if(ops.caret.line > 0 && ops.caret.col > 0 && ops.caret.file.size() > 0)
		fprintf(stderr, "%s(%s:%" PRIu64 ":%" PRIu64 ") ", COLOUR_BLACK_BOLD, filename.c_str(), ops.caret.line, ops.caret.col);

	fprintf(stderr, "%s%s%s%s: %s%s\n", colour, type, COLOUR_RESET, COLOUR_BLACK_BOLD, alloc, COLOUR_RESET);

	if(ops.caret.line > 0 && ops.caret.col > 0)
	{
		std::vector<std::string> lines;
		if(ops.caret.file.length() > 0)
		{
			GenError::printContext(ops);
		}
	}

	fprintf(stderr, "\n");

	va_end(ap);
	free(alloc);

	if(doExit)
	{
		doTheExit();
	}
	else if(strcmp(type, "Warning") == 0 && Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
	{
		error("Treating warning as error because -Werror was passed");
	}
}





void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void error(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast->pin), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void error(Expr* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(ops, msg, "Error", true, ap);
	va_end(ap);
	abort();
}





void errorNoExit(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Error", false, ap);
	va_end(ap);
}

void errorNoExit(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast->pin), msg, "Error", false, ap);
	va_end(ap);
}

void errorNoExit(Expr* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(ops, msg, "Error", false, ap);
	va_end(ap);
}










void warn(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Warning", false, ap);
	va_end(ap);
}

void warn(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast->pin), msg, "Warning", false, ap);
	va_end(ap);
}

void warn(Expr* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(ops, msg, "Warning", false, ap);
	va_end(ap);
}



void info(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Note", false, ap);
	va_end(ap);
}

void info(Expr* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast->pin), msg, "Note", false, ap);
	va_end(ap);
}

void info(Expr* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(ops, msg, "Note", false, ap);
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

	void unknownSymbol(CodegenInstance* cgi, Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Using undeclared %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void duplicateSymbol(CodegenInstance* cgi, Expr* e, std::string symname, SymbolType st)
	{
		error(e, "Duplicate %s %s", SymbolTypeNames[(int) st], symname.c_str());
	}

	void noOpOverload(CodegenInstance* cgi, Expr* e, std::string type, ArithmeticOp op)
	{
		error(e, "No valid operator overload for %s on type %s", Parser::arithmeticOpToString(cgi, op).c_str(), type.c_str());
	}

	void invalidAssignment(CodegenInstance* cgi, Expr* e, fir::Type* a, fir::Type* b)
	{
		// note: HACK
		// C++ does static function resolution on struct members, so as long as getReadableType() doesn't use
		// the 'this' pointer (it doesn't) we'll be fine.
		// thus, we don't check whether cgi is null.

		error(e, "Invalid assignment from type %s to %s", cgi->getReadableType(b).c_str(),
			cgi->getReadableType(a).c_str());
	}

	void invalidAssignment(CodegenInstance* cgi, Expr* e, fir::Value* a, fir::Value* b)
	{
		invalidAssignment(cgi, e, a->getType(), b->getType());
	}

	void invalidInitialiser(CodegenInstance* cgi, Expr* e, std::string name, std::vector<fir::Value*> args)
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

	void expected(CodegenInstance* cgi, Expr* e, std::string expect)
	{
		error(e, "Expected %s", expect.c_str());
	}

	void nullValue(CodegenInstance* cgi, Expr* expr)
	{
		if(dynamic_cast<BinOp*>(expr) && cgi->isArithmeticOpAssignment(dynamic_cast<BinOp*>(expr)->op))
		{
			auto bo = dynamic_cast<BinOp*>(expr);
			auto op = bo->op;

			HighlightOptions ops;

			ops.caret = expr->pin;
			ops.drawCaret = false;

			ops.underlines.push_back(getHighlightExtent(bo));

			errorNoExit(expr, ops, "Values cannot be yielded from voids");

			info(expr, "Assignment and compound assignment operators (eg. '%s' here) are not expressions, and cannot produce a value",
				Parser::arithmeticOpToString(cgi, op).c_str());

			doTheExit();
		}
		else
		{
			error(expr, "Values cannot be yielded from voids");
		}
	}

	void noSuchMember(CodegenInstance* cgi, Expr* e, std::string type, std::string member)
	{
		error(e, "Type %s does not have a member '%s'", type.c_str(), member.c_str());
	}

	void noFunctionTakingParams(CodegenInstance* cgi, Expr* e, std::string type, std::string name, std::deque<Expr*> ps)
	{
		std::string prs = "";
		for(auto p : ps)
			prs += cgi->getReadableType(p) + ", ";

		if(prs.size() > 0) prs = prs.substr(0, prs.size() - 2);

		error(e, "%s does not contain a function %s taking parameters (%s)", type.c_str(), name.c_str(), prs.c_str());
	}

	void assignToImmutable(CodegenInstance* cgi, Expr* op, Expr* value)
	{
		HighlightOptions ops;
		ops.caret = op->pin;

		ops.underlines.push_back(getHighlightExtent(value));

		error(op, ops, "Cannot assign to immutable expression '%s'", cgi->printAst(op).c_str());
	}





	std::pair<std::string, std::string> getPrettyNoSuchFunctionError(CodegenInstance* cgi, std::deque<Expr*> args, std::deque<FuncPair_t> cands)
	{
		std::vector<std::string> argtypes;
		for(auto a : args)
			argtypes.push_back(cgi->getReadableType(a).c_str());

		std::string argstr;
		for(auto s : argtypes)
			argstr += ", " + s;

		if(argstr.length() > 0)
			argstr = argstr.substr(2);

		std::string candidates;
		std::deque<FuncPair_t> reses;

		for(auto fs : cands)
		{
			if(fs.second)
				candidates += cgi->printAst(fs.second) + "\n";
		}

		return { argstr, candidates };
	}
}




Parser::Pin getHighlightExtent(Ast::Expr* e)
{
	if(MemberAccess* ma = dynamic_cast<MemberAccess*>(e))
	{
		auto left = getHighlightExtent(ma->left);
		auto right = getHighlightExtent(ma->right);

		Parser::Pin ret;

		ret.file = ma->pin.file;
		ret.line = ma->pin.line;
		ret.col = left.col;
		ret.len = (right.col + right.len) - left.len;

		return ret;
	}
	else if(BinOp* bo = dynamic_cast<BinOp*>(e))
	{
		auto left = getHighlightExtent(bo->left);
		auto right = getHighlightExtent(bo->right);

		Parser::Pin ret;

		ret.file = bo->pin.file;
		ret.line = bo->pin.line;
		ret.col = left.col;
		ret.len = (right.col + right.len) - left.col;

		return ret;
	}
	else if(e)
	{
		return e->pin;
	}
	else
	{
		return Parser::Pin();
	}
}








































