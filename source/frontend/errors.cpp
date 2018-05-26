// errors.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"

#include "ast.h"
#include "sst.h"
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

static std::string spaces(size_t n)
{
	return repeat(" ", n);
}


#define UNDERLINE_CHARACTER " ̅"
// #define UNDERLINE_CHARACTER "^"

std::string printContext(HighlightOptions ops)
{
	std::string retstr;

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
		auto part1 = strprintf("%s%s", _convertTab(), str.c_str());

		size_t cursorX = 1;
		bool uline = ops.caret.len >= 5 && ops.underlines.empty();

		std::string part2 = _convertTab();

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
						uline ? repeat(UNDERLINE_CHARACTER, num1).c_str() : std::string(num1, ' '),
						ops.drawCaret && !uline ? "^" : UNDERLINE_CHARACTER,
						uline ? repeat(UNDERLINE_CHARACTER, num2).c_str() : std::string(num2, ' '),
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
				part2 += strprintf("%s%s%s", COLOUR_GREEN_BOLD, repeat(UNDERLINE_CHARACTER, ul.len).c_str(), COLOUR_RESET);
				cursorX += ul.len;
			}
		}


		// ok, now we print the fancy left side pipes and line numbers and all
		{
			auto num_width = std::to_string(ops.caret.line).length();

			// one spacing line
			retstr =  strprintf("%s |\n", spaces(num_width));
			retstr += strprintf("%d |%s\n", ops.caret.line, part1);
			retstr += strprintf("%s |%s\n", spaces(num_width), part2);
		}
	}
	else
	{
		retstr = strprintf("(no context)");
	}

	return retstr;
}

static std::vector<Locatable*> errorLocationStack;
void pushErrorLocation(Locatable* l)
{
	errorLocationStack.push_back(l);
}

void popErrorLocation()
{
	iceAssert(errorLocationStack.size() > 0);
	errorLocationStack.pop_back();
}





std::string __error_gen_internal(const HighlightOptions& ops, const std::string& msg, const char* type)
{
	std::string ret;

	auto colour = COLOUR_RED_BOLD;
	if(strcmp(type, "warning") == 0)
		colour = COLOUR_MAGENTA_BOLD;

	else if(strcmp(type, "note") == 0)
		colour = COLOUR_GREY_BOLD;

	// bool empty = strcmp(type, "") == 0;
	// bool dobold = strcmp(type, "note") != 0;

	// todo: do we want to truncate the file path?
	// we're doing it now, might want to change (or use a flag)

	std::string filename = frontend::getFilenameFromPath(ops.caret.fileID == 0 ? "(unknown)"
		: frontend::getFilenameFromID(ops.caret.fileID));

	ret += strprintf("%s%s%s:%s %s\n", COLOUR_RESET, colour, type, COLOUR_RESET, msg);

	if(ops.caret.fileID > 0)
	{
		auto location = strprintf("%s:%d:%d", filename, ops.caret.line + 1, ops.caret.col + 1);
		ret += strprintf("%s%sat:%s %s%s", COLOUR_RESET, COLOUR_GREY_BOLD, spaces(strlen(type) - 2), COLOUR_RESET, location);
	}

	ret += "\n";

	if(ops.caret.fileID > 0)
		ret += printContext(ops);

	ret += "\n";
	return ret;
}



#define MAX_BACKTRACE_DEPTH 0

//! prevents re-entrant calling
// static bool isBacktracing = false;
std::string __error_gen_backtrace(const HighlightOptions& ops)
{
	std::string ret;
	#if 0

	if(isBacktracing || MAX_BACKTRACE_DEPTH == 0) return ret;

	isBacktracing = true;

	// give more things.
	auto numlocs = errorLocationStack.size();

	std::vector<Location> seen;

	if(numlocs > 1)
	{
		int done = 0;
		for(size_t i = numlocs; i-- > 2; )
		{
			// skip the boring stuff.
			auto e = errorLocationStack[i];
			if(dcast(ast::Block, e) || dcast(sst::Block, e) || ops.caret == e->loc || std::find(seen.begin(), seen.end(), e->loc) != seen.end())
				continue;

			std::string oper = (dcast(ast::Stmt, e) ? "typechecking" : "code generation");
			ret += __error_gen(HighlightOptions(e->loc), strprintf("In %s of %s here:",
				oper, e->readableName).c_str(), "Note", false);

			done++;

			seen.push_back(e->loc);

			int64_t skip = (int64_t) numlocs - done - (int64_t) i;
			if(done == 2 && i - MAX_BACKTRACE_DEPTH > 1 && skip > 0)
			{
				ret += strprintf("... skipping %d intermediarie%s...\n\n", skip, skip == 1 ? "" : "s");

				continue;
			}
		}
	}

	isBacktracing = false;
	#endif

	return ret;
}

[[noreturn]] void doTheExit(bool trace)
{
	if(trace)
		fprintf(stderr, "%s", __error_gen_backtrace(HighlightOptions()).c_str());

	fprintf(stderr, "There were errors, compilation cannot continue\n");

	#ifdef NDEBUG
		exit(1);
	#else
		abort();
	#endif
}



[[noreturn]] void postErrorsAndQuit(const ComplexError& error)
{
	for(const auto& [ kind, loc, estr ] : error._strs)
	{
		if(kind == ComplexError::Kind::Error)            exitless_error(loc, "%s", estr);
		else if(kind == ComplexError::Kind::Warning)     warn(loc, "%s", estr);
		else if(kind == ComplexError::Kind::Info)        info(loc, "%s", estr);
	}

	doTheExit();
}

































