// errors.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"

#include "ast.h"
#include "frontend.h"

static std::string _convertTab()
{
	return std::string(TAB_WIDTH, ' ');
}

static std::string repeat(std::string str, size_t n)
{
	if(n == 0)
	{
		str.clear();
		str.shrink_to_fit();
		return str;
	}
	else if(n == 1 || str.empty())
	{
		return str;
	}

	auto period = str.size();
	if(period == 1)
	{
		str.append(n - 1, str.front());
		return str;
	}

	str.reserve(period * n);
	size_t m = 2;
	for(; m < n; m *= 2)
		str += str;

	str.append(str.c_str(), (n - (m / 2)) * period);

	return str;
}

std::string printContext(HighlightOptions ops)
{
	std::string ret;

	auto lines = frontend::getFileLines(ops.caret.fileID);
	if(lines.size() > ops.caret.line)
	{
		std::string orig = lines[ops.caret.line].to_string();

		size_t adjust = 0;
		size_t adjust1 = 0;
		for(auto c : orig)
		{
			if(c == '\t')		adjust += TAB_WIDTH, adjust1 += (TAB_WIDTH - 1);
			else if(c == ' ')	adjust++;
			else				break;
		}

		std::stringstream ln;

		for(auto c : orig)
		{
			if(c != '\t' && c != '\n')
			{
				ln << c;
			}
		}

		auto str = ln.str();
		ret += strprintf("%s%s\n", _convertTab(), str.c_str());

		size_t cursorX = 1;

		bool uline = ops.caret.len >= 5 && ops.underlines.empty();

		std::string part2;

		ops.caret.col -= adjust;
		for(auto& u : ops.underlines)
			u.col -= adjust;

		ops.underlines.push_back(ops.caret);
		std::sort(ops.underlines.begin(), ops.underlines.end(), [](Location a, Location b) { return a.col < b.col; });

		for(auto& ul : ops.underlines)
		{
			while(cursorX < ul.col)
				part2 += " ", cursorX++;

			if(ul == ops.caret)
			{
				// debuglog("HI %d, %d\n", ops.drawCaret, ops.caret.line);
				if(ops.caret.fileID > 0)
				{
					auto num1 = (ops.caret.len / 2);
					auto num2 = (ops.caret.len > 0 ? ((ops.caret.len - 1) - (ops.caret.len / 2)) : 0);
					cursorX += num1 + 1 + num2;

					part2 += strprintf("%s%s%s%s%s", COLOUR_GREEN_BOLD,
						uline ? repeat(" ̅", num1).c_str() : std::string(num1, ' '),
						ops.drawCaret && !uline ? "^" : " ̅",
						uline ? repeat(" ̅", num2).c_str() : std::string(num2, ' '),
						COLOUR_RESET);
				}
				else
				{
					part2 += std::string(ul.len, ' ');
					cursorX += ul.len;
				}
			}
			else
			{
				part2 += strprintf("%s%s%s", COLOUR_GREEN_BOLD, repeat(" ̅", ul.len).c_str(), COLOUR_RESET);
				cursorX += ul.len;
			}
		}

		ret += (_convertTab() + part2);
	}
	else
	{
		ret += strprintf("(no context)");
	}

	return ret;
}

#define DEBUG 1

[[noreturn]] void doTheExit()
{
	fprintf(stderr, "There were errors, compilation cannot continue\n");

	#if DEBUG
		abort();
	#else
		exit(1);
	#endif
}












void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void error(Locatable* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast ? relevantast->loc : Location()), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void error(Locatable* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(ops.caret.fileID == 0)
		ops.caret = relevantast ? relevantast->loc : Location();

	__error_gen(ops, msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void error(const Location& loc, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(loc), msg, "Error", true, ap);
	va_end(ap);
	abort();
}



void exitless_error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Error", false, ap);
	va_end(ap);
}

void exitless_error(Locatable* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast ? relevantast->loc : Location()), msg, "Error", false, ap);
	va_end(ap);
}

void exitless_error(Locatable* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(ops.caret.fileID == 0)
		ops.caret = relevantast ? relevantast->loc : Location();

	__error_gen(ops, msg, "Error", false, ap);
	va_end(ap);
}

void exitless_error(const Location& loc, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(loc), msg, "Error", false, ap);
	va_end(ap);
}










void warn(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Warning", false, ap);
	va_end(ap);
}

void warn(Locatable* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast ? relevantast->loc : Location()), msg, "Warning", false, ap);
	va_end(ap);
}

void warn(Locatable* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(ops.caret.fileID == 0)
		ops.caret = relevantast ? relevantast->loc : Location();

	__error_gen(ops, msg, "Warning", false, ap);
	va_end(ap);
}

void warn(const Location& loc, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(loc), msg, "Warning", false, ap);
	va_end(ap);
}






void info(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Note", false, ap);
	va_end(ap);
}

void info(Locatable* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast ? relevantast->loc : Location()), msg, "Note", false, ap);
	va_end(ap);
}

void info(Locatable* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(ops.caret.fileID == 0)
		ops.caret = relevantast ? relevantast->loc : Location();

	__error_gen(ops, msg, "Note", false, ap);
	va_end(ap);
}

void info(const Location& loc, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(loc), msg, "Note", false, ap);
	va_end(ap);
}































