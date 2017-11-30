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
		std::string orig = util::to_string(lines[ops.caret.line]);

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
		std::sort(ops.underlines.begin(), ops.underlines.end(), [=](Location a, Location b) { return a.col < b.col; });

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

[[noreturn]] void doTheExit()
{
	fprintf(stderr, "There were errors, compilation cannot continue\n");

	#ifdef NDEBUG
		exit(1);
	#else
		abort();
	#endif
}


std::string __error_gen_part1(const HighlightOptions& ops, const char* msg, const char* type)
{
	std::string ret;

	auto colour = COLOUR_RED_BOLD;
	if(strcmp(type, "Warning") == 0)
		colour = COLOUR_MAGENTA_BOLD;

	else if(strcmp(type, "Note") == 0)
		colour = COLOUR_GREY_BOLD;

	bool empty = strcmp(type, "") == 0;
	bool dobold = strcmp(type, "Note") != 0;

	// todo: do we want to truncate the file path?
	// we're doing it now, might want to change (or use a flag)

	std::string filename = frontend::getFilenameFromPath(ops.caret.fileID == 0 ? "(unknown)"
		: frontend::getFilenameFromID(ops.caret.fileID));

	// std::string filename = "TODO: filename";

	if(ops.caret.fileID > 0)
		ret += strprintf("%s%s(%s:%zu:%zu) ", COLOUR_RESET, COLOUR_BLACK_BOLD, filename, ops.caret.line + 1, ops.caret.col + 1);

	if(empty)	ret += strprintf("%s%s%s%s", colour, type, COLOUR_RESET, dobold ? COLOUR_BLACK_BOLD : "");
	else		ret += strprintf("%s%s%s%s: ", colour, type, COLOUR_RESET, dobold ? COLOUR_BLACK_BOLD : "");

	return ret;
}

std::string __error_gen_part2(const HighlightOptions& ops)
{
	std::string ret = strprintf("%s\n", COLOUR_RESET);

	if(ops.caret.fileID > 0)
	{
		std::vector<std::string> lines;
		if(ops.caret.fileID > 0)
			ret += printContext(ops);
	}

	ret += "\n";
	return ret;
}









void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(HighlightOptions(), msg, "Error", true, ap);
	va_end(ap);
	doTheExit();
}

void error(Locatable* relevantast, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(relevantast ? relevantast->loc : Location()), msg, "Error", true, ap);
	va_end(ap);
	doTheExit();
}

void error(Locatable* relevantast, HighlightOptions ops, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(ops.caret.fileID == 0)
		ops.caret = relevantast ? relevantast->loc : Location();

	__error_gen(ops, msg, "Error", true, ap);
	va_end(ap);
	doTheExit();
}

void error(const Location& loc, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	__error_gen(HighlightOptions(loc), msg, "Error", true, ap);
	va_end(ap);
	doTheExit();
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































