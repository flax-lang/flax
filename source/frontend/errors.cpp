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

static std::string fetchContextLine(Location loc, size_t* adjust)
{
	auto lines = frontend::getFileLines(loc.fileID);
	if(lines.size() > loc.line)
	{
		std::string orig = util::to_string(lines[loc.line]);

		size_t adjust1 = 0;
		for(auto c : orig)
		{
			if(c == '\t')		(*adjust) += TAB_WIDTH, adjust1 += (TAB_WIDTH - 1);
			else if(c == ' ')	(*adjust)++;
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

		return strprintf("%s%s", _convertTab(), ln.str().c_str());
	}

	return "";
}


// #define UNDERLINE_CHARACTER " ̅"
#define UNDERLINE_CHARACTER "^"

std::string printContext(HighlightOptions ops)
{
	std::string retstr;

	auto lines = frontend::getFileLines(ops.caret.fileID);
	if(lines.size() > ops.caret.line)
	{
		size_t adjust = 0;
		auto part1 = fetchContextLine(ops.caret, &adjust);

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
			auto num_width = std::to_string(ops.caret.line + 1).length();

			// one spacing line
			retstr =  strprintf("%s |\n", spaces(num_width));
			retstr += strprintf("%d |%s\n", ops.caret.line + 1, part1);
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





std::string __error_gen_internal(const HighlightOptions& ops, const std::string& msg, const char* type, bool context)
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

	if(context && ops.caret.fileID > 0)
		ret += printContext(ops) + "\n";

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








































static std::string typestr(MsgType t)
{
	switch(t)
	{
		case MsgType::Note:     return "note";
		case MsgType::Error:    return "error";
		case MsgType::Warning:  return "warning";
		default:                iceAssert(0); return "";
	}
}

template <typename... Ts>
static size_t strprinterrf(const char* fmt, Ts... ts)
{
	return (size_t) fprintf(stderr, "%s", strprintf(fmt, ts...).c_str());
}

template <typename... Ts>
static void outputWithoutContext(const char* type, const HighlightOptions& ho, const char* fmt, Ts... ts)
{
	strprinterrf("%s", __error_gen_internal(ho, strprintf(fmt, ts...), type, false));
}







void BareError::post()
{
	outputWithoutContext(typestr(this->type).c_str(), HighlightOptions(), this->msg.c_str());
	for(auto other : this->subs)
		other->post();
}

bool BareError::hasErrors() const
{
	return this->msg.size() > 0 || this->subs.size() > 0;
}

BareError* BareError::clone() const
{
	return new BareError(*this);
}



void SimpleError::post()
{
	outputWithoutContext(typestr(this->type).c_str(), HighlightOptions(this->loc), this->msg.c_str());
	strprinterrf("%s%s%s\n", this->wordsBeforeContext, this->wordsBeforeContext.size() > 0 ? "\n" : "", printContext(HighlightOptions(this->loc)));

	for(auto other : this->subs)
		other->post();
}

bool SimpleError::hasErrors() const
{
	return this->msg.size() > 0 || this->subs.size() > 0;
}

SimpleError* SimpleError::clone() const
{
	return new SimpleError(*this);
}



SpanError& SpanError::add(const Span& s)
{
	this->spans.push_back(s);
	return *this;
}

bool SpanError::hasErrors() const
{
	return (this->top.hasErrors() || this->spans.size() > 0) || this->subs.size() > 0;
}

SpanError* SpanError::clone() const
{
	return new SpanError(*this);
}

void SpanError::post()
{
	BareError(this->top.msg, this->top.type).post();

	// ok. so we have our own custom little output thing here.
	{
		// sort the spans.
		std::sort(this->spans.begin(), this->spans.end(), [](auto a, auto b) -> bool { return a.loc.col < b.loc.col; });

		auto num_width = std::to_string(this->top.loc.line + 1).length();

		// one spacing line
		size_t adjust = 0;
		size_t margin = num_width + 2;
		strprinterrf("%s |\n", spaces(num_width));
		strprinterrf("%d |%s\n", this->top.loc.line + 1, fetchContextLine(this->top.loc, &adjust));
		strprinterrf("%s |", spaces(num_width));

		// ok, now loop through each err, and draw the underline.
		// size_t width = std::min((size_t) 80, platform::getTerminalWidth()) - 10;
		size_t width = (size_t) (0.85 * platform::getTerminalWidth());

		//* cursor represents the 'virtual' position -- excluding the left margin
		size_t cursor = 0;
		for(auto span : this->spans)
		{
			// pad out.
			cursor += strprinterrf("%s", spaces(num_width + span.loc.col - adjust - cursor));
			strprinterrf("%s", COLOUR_RED_BOLD);

			cursor += strprinterrf("%s", repeat(UNDERLINE_CHARACTER, span.loc.len));
			strprinterrf("%s", COLOUR_RESET);
		}

		cursor = 0;
		strprinterrf("\n");

		// there's probably a more efficient way to do this, but since we're throwing an error and already going to die,
		// it doesn't really matter.

		// don't mutate the spans, make a copy
		auto spanscopy = this->spans;

		size_t counter = 0;
		while(counter < spanscopy.size())
		{
			strprinterrf("%s", spaces(margin));

			for(size_t i = 0; i < spanscopy.size() - counter; i++)
			{
				auto col = spanscopy[i].loc.col;

				if(i == spanscopy.size() - counter - 1)
				{
					auto remaining = spanscopy[i].msg;

					// complex math shit to predict where the cursor will be once we print,
					// and more importantly whether or not we'll finish printing the message in the current iteration.
					auto realcursor = cursor + 2 + (num_width + col - adjust - cursor + 1) + margin + 1;

					auto splitpos = std::min(remaining.length(), width - realcursor);

					// refuse to split words in half.
					while(splitpos > 0 && splitpos < remaining.length() && remaining[splitpos - 1] != ' ')
						splitpos--;

					auto segment = remaining.substr(0, splitpos);
					if(segment.empty())
					{
						counter++;
						break;
					}
					else
					{
						cursor += 3 + strprinterrf("%s", spaces(num_width + col - adjust - cursor));
						strprinterrf("%s|>%s ", COLOUR_GREY_BOLD, COLOUR_RESET);

						spanscopy[i].msg = remaining.substr(segment.length());
						cursor += strprinterrf("%s", segment);
					}
				}
				else
				{
					cursor += 1 + strprinterrf("%s", spaces(num_width + col - adjust - cursor));
					strprinterrf("%s|%s", COLOUR_GREY_BOLD, COLOUR_RESET);
				}
			}

			cursor = 0;
			strprinterrf("\n");
		}
	}


	for(auto other : this->subs)
		other->post();
}












bool OverloadError::hasErrors() const
{
	return (this->top.hasErrors() || this->cands.size() > 0) || this->subs.size() > 0;
}

void OverloadError::clear()
{
	this->cands.clear();
	this->top = SimpleError();
}

OverloadError* OverloadError::clone() const
{
	return new OverloadError(*this);
}


OverloadError& OverloadError::addCand(Locatable* d, const ErrorMsg& sp)
{
	this->cands[d] = sp.clone();
	return *this;
}



void OverloadError::post()
{
	// first, post the original error.
	this->top.wordsBeforeContext = "(call site)";
	this->top.post();

	// sort the candidates by line number
	// (idk maybe it's a windows thing but it feels like the order changes every so often)
	auto cds = std::vector<std::pair<Locatable*, ErrorMsg*>>(this->cands.begin(), this->cands.end());
	std::sort(cds.begin(), cds.end(), [](auto a, auto b) -> bool { return a.first->loc.line < b.first->loc.line; });

	// go through each candidate.
	int cand_counter = 1;
	for(auto [ loc, emg ] : cds)
	{
		if(emg->kind != ErrKind::Span)
		{
			emg->post();
		}
		else
		{
			auto spe = dynamic_cast<SpanError*>(emg);
			iceAssert(spe);

			spe->set(SimpleError(loc->loc, strprintf("candidate %d was defined here:", cand_counter++), MsgType::Note));
			spe->post();

			strprinterrf("\n");
		}
	}

	for(auto sub : this->subs)
		sub->post();
}








[[noreturn]] void postErrorsAndQuit(ErrorMsg* err)
{
	err->postAndQuit();
}












