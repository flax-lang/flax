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

std::string getSpannedContext(const Location& loc, const std::vector<SpanError::Span>& spans, size_t* adjust, size_t* num_width, size_t* margin,
	bool underline, bool bottompad, std::string underlineColour)
{
	std::string ret;

	iceAssert(adjust && margin && num_width);
	iceAssert((underline == bottompad || bottompad) && "cannot underline without bottom pad");

	if(!std::is_sorted(spans.begin(), spans.end(), [](auto a, auto b) -> bool { return a.loc.col < b.loc.col; }))
		_error_and_exit("spans must be sorted!");

	*num_width = std::to_string(loc.line + 1).length();

	// one spacing line
	*margin = *num_width + 2;
	ret += strprintf("%s |\n", spaces(*num_width));
	ret += strprintf("%d |%s\n", loc.line + 1, fetchContextLine(loc, adjust));

	if(bottompad)
		ret += strprintf("%s |", spaces(*num_width));

	// ok, now loop through each err, and draw the underline.
	if(underline)
	{
		//* cursor represents the 'virtual' position -- excluding the left margin
		size_t cursor = 0;
		for(auto span : spans)
		{
			// pad out.
			auto tmp = strprintf("%s", spaces(*num_width + span.loc.col - *adjust - cursor));
			ret += tmp + strprintf("%s", span.colour.empty() ? underlineColour : span.colour); cursor += tmp.length();

			tmp = strprintf("%s", repeat(UNDERLINE_CHARACTER, span.loc.len));
			ret += tmp + strprintf("%s", COLOUR_RESET); cursor += tmp.length();
		}
	}

	return ret;
}

std::string getSingleContext(const Location& loc, bool underline = true, bool bottompad = true)
{
	size_t a = 0;
	size_t b = 0;
	size_t c = 0;
	return getSpannedContext(loc, { SpanError::Span(loc, "") }, &a, &b, &c, underline, bottompad, COLOUR_RED_BOLD);
}






std::string __error_gen_internal(const Location& loc, const std::string& msg, const char* type, bool context)
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

	std::string filename = frontend::getFilenameFromPath(loc.fileID == 0 ? "(unknown)"
		: frontend::getFilenameFromID(loc.fileID));

	ret += strprintf("%s%s%s:%s %s\n", COLOUR_RESET, colour, type, COLOUR_RESET, msg);

	if(loc.fileID > 0)
	{
		auto location = strprintf("%s:%d:%d", filename, loc.line + 1, loc.col + 1);
		ret += strprintf("%s%sat:%s %s%s", COLOUR_RESET, COLOUR_GREY_BOLD, spaces(strlen(type) - 2), COLOUR_RESET, location);
	}

	ret += "\n";

	if(context && loc.fileID > 0)
		ret += getSingleContext(loc) + "\n";

	return ret;
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
static void outputWithoutContext(const char* type, const Location& loc, const char* fmt, Ts... ts)
{
	strprinterrf("%s", __error_gen_internal(loc, strprintf(fmt, ts...), type, false));
}







void BareError::post()
{
	outputWithoutContext(typestr(this->type).c_str(), Location(), this->msg.c_str());
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
	outputWithoutContext(typestr(this->type).c_str(), this->loc, this->msg.c_str());
	strprinterrf("%s%s%s", this->wordsBeforeContext, this->wordsBeforeContext.size() > 0 ? "\n" : "",
		this->printContext ? getSingleContext(this->loc) + "\n\n" : "");

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

static bool operator == (const SpanError::Span& a, const SpanError::Span& b) { return a.loc == b.loc && a.msg == b.msg; }

void SpanError::post()
{
	this->top.printContext = false;
	this->top.post();

	{
		size_t adjust = 0;
		size_t margin = 0;
		size_t num_width = 0;

		if(this->highlightActual)
		{
			auto sp = Span(this->top.loc, "");
			sp.colour = COLOUR_RED_BOLD;

			this->spans.push_back(sp);
		}

		std::sort(this->spans.begin(), this->spans.end(), [](auto a, auto b) -> bool { return a.loc.col < b.loc.col; });
		strprinterrf("%s\n", getSpannedContext(this->top.loc, this->spans, &adjust, &num_width, &margin, true, true, COLOUR_CYAN_BOLD));

		// ok now remove the extra thing.
		if(this->highlightActual)
			this->spans.erase(std::find(this->spans.begin(), this->spans.end(), Span(this->top.loc, "")));

		size_t cursor = 0;
		size_t width = (size_t) (0.85 * platform::getTerminalWidth());

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
						strprinterrf("%s|>%s ", COLOUR_CYAN_BOLD, COLOUR_RESET);

						spanscopy[i].msg = remaining.substr(segment.length());
						cursor += strprinterrf("%s", segment);
					}
				}
				else
				{
					cursor += 1 + strprinterrf("%s", spaces(num_width + col - adjust - cursor));
					strprinterrf("%s|%s", COLOUR_CYAN_BOLD, COLOUR_RESET);
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
			spe->highlightActual = false;
			spe->post();
		}
	}

	for(auto sub : this->subs)
		sub->post();
}






[[noreturn]] void doTheExit(bool trace)
{
	fprintf(stderr, "There were errors, compilation cannot continue\n");

	#ifdef NDEBUG
		exit(1);
	#else
		abort();
	#endif
}













