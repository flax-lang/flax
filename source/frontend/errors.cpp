// errors.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"

#include "ast.h"
#include "sst.h"
#include "frontend.h"

#include "memorypool.h"

#define LEFT_PADDING TAB_WIDTH


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
	if(loc.fileID == 0) return "";

	auto lines = frontend::getFileLines(loc.fileID);
	if(lines.size() > loc.line)
	{
		std::string orig = util::to_string(lines[loc.line]);
		std::stringstream ln;

		// skip all leading whitespace.
		bool ws = true;
		for(auto c : orig)
		{
			if(ws && c == '\t')                 { *adjust += TAB_WIDTH; continue;   }
			else if(ws && c == ' ')             { *adjust += 1; continue;           }
			else if(c == '\n')                  { break;                            }
			else                                { ws = false; ln << c;              }
		}

		return strprintf("%s%s", spaces(LEFT_PADDING), ln.str().c_str());
	}

	return "";
}













// #define UNDERLINE_CHARACTER "^"
#define UNDERLINE_CHARACTER "\u203e"


static std::string getSpannedContext(const Location& loc, const std::vector<util::ESpan>& spans, size_t* adjust, size_t* num_width,
	size_t* margin, bool underline, bool bottompad, const std::string& underlineColour, const std::string& overrideLineContent)
{
	std::string ret;

	iceAssert(adjust && margin && num_width);
	iceAssert((underline == bottompad || bottompad) && "cannot underline without bottom pad");

	if(!std::is_sorted(spans.begin(), spans.end(), [](const auto& a, const auto& b) -> bool { return a.loc.col < b.loc.col; }))
		_error_and_exit("spans must be sorted!\n");

	*num_width = std::to_string(loc.line + 1).length();

	// one spacing line
	*margin = *num_width + 2;
	ret += strprintf("%s |\n", spaces(*num_width));

	if(overrideLineContent.empty()) ret += strprintf("%d |%s\n", loc.line + 1, fetchContextLine(loc, adjust));
	else                            ret += strprintf("%s |%s%s\n", spaces(*num_width), spaces(LEFT_PADDING), overrideLineContent);

	if(bottompad)
		ret += strprintf("%s |", spaces(*num_width));

	// ok, now loop through each err, and draw the underline.
	if(underline)
	{
		//* cursor represents the 'virtual' position -- excluding the left margin
		size_t cursor = 0;

		// columns actually start at 1 for some reason.
		ret += spaces(LEFT_PADDING - 1);

		for(auto span : spans)
		{
			std::string underliner = (span.loc.len < 3 ? "^" : UNDERLINE_CHARACTER);

			// pad out.
			auto tmp = strprintf("%s", spaces(1 + span.loc.col - *adjust - cursor)); cursor += tmp.length();
			ret += tmp + strprintf("%s", span.colour.empty() ? underlineColour : span.colour);

			tmp = strprintf("%s", repeat(underliner, span.loc.len)); cursor += span.loc.len;
			ret += tmp + strprintf("%s", COLOUR_RESET);
		}
	}

	return ret;
}

static std::string getSingleContext(const Location& loc, const std::string& underlineColour = COLOUR_RED_BOLD,
	const std::string& overrideLineContent = "", bool underline = true, bool bottompad = true)
{
	if(loc.fileID == 0 && overrideLineContent.empty()) return "";

	size_t a = 0;
	size_t b = 0;
	size_t c = 0;
	return getSpannedContext(loc, { util::ESpan(loc, "") }, &a, &b, &c, underline, bottompad, underlineColour, overrideLineContent);
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

	//? do we want to truncate the file path?
	//? we're doing it now, might want to change (or use a flag)

	std::string filename = frontend::getFilenameFromPath(loc.fileID == 0 ? "(unknown)"
		: frontend::getFilenameFromID(loc.fileID));

	ret += strprintf("%s%s%s:%s %s\n", COLOUR_RESET, colour, type, COLOUR_RESET, msg);

	if(loc.fileID > 0)
	{
		auto location = strprintf("%s:%d:%d", filename, loc.line + 1, loc.col + 1);
		ret += strprintf("%s%sat:%s %s%s\n", COLOUR_RESET, COLOUR_GREY_BOLD, spaces(strlen(type) - 2), COLOUR_RESET, location);
	}


	if(context && loc.fileID > 0)
	{
		auto underlineColour = COLOUR_RED_BOLD;

		if(strcmp(type, "note") == 0)
			underlineColour = COLOUR_BLUE_BOLD;

		ret += getSingleContext(loc, underlineColour) + "\n";
	}

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
	if(!this->msg.empty()) outputWithoutContext(typestr(this->type).c_str(), Location(), this->msg.c_str());

	for(auto other : this->subs)
		other->post();
}


void SimpleError::post()
{
	if(!this->msg.empty())
	{
		outputWithoutContext(typestr(this->type).c_str(), this->loc, this->msg.c_str());
		strprinterrf("%s%s%s", this->wordsBeforeContext, this->wordsBeforeContext.size() > 0 ? "\n" : "",
			this->printContext ? getSingleContext(this->loc, this->type == MsgType::Note ? COLOUR_BLUE_BOLD : COLOUR_RED_BOLD) + "\n\n" : "");
	}

	for(auto other : this->subs)
		other->post();
}


void ExampleMsg::post()
{
	outputWithoutContext(typestr(this->type).c_str(), Location(), "for example:");
	strprinterrf("%s\n\n", getSingleContext(Location(), COLOUR_BLUE_BOLD, this->example));

	for(auto other : this->subs)
		other->post();
}


namespace util
{
	static bool operator == (const util::ESpan& a, const util::ESpan& b) { return a.loc == b.loc && a.msg == b.msg; }


	BareError* make_BareError(const std::string& m, MsgType t)
	{
		return util::pool<BareError>(m, t);
	}

	SpanError* make_SpanError(SimpleError* se, const std::vector<ESpan>& s, MsgType t)
	{
		return util::pool<SpanError>(se, s, t);
	}

	SimpleError* make_SimpleError(const Location& l, const std::string& m, MsgType t)
	{
		return util::pool<SimpleError>(l, m, t);
	}

	OverloadError* make_OverloadError(SimpleError* se, MsgType t)
	{
		return util::pool<OverloadError>(se, t);
	}

	ExampleMsg* make_ExampleMsg(const std::string& eg, MsgType t)
	{
		return util::pool<ExampleMsg>(eg, t);
	}
}


SpanError* SpanError::add(const util::ESpan& s)
{
	this->spans.push_back(s);
	return this;
}

void SpanError::post()
{
	this->top->printContext = false;
	this->top->post();

	{
		size_t adjust = 0;
		size_t margin = 0;
		size_t num_width = 0;

		bool didExtra = false;
		if(this->highlightActual && std::find_if(this->spans.begin(), this->spans.end(), [this](const util::ESpan& s) -> bool {
			return s.loc == this->top->loc;
		}) == this->spans.end())
		{
			didExtra = true;

			auto sp = util::ESpan(this->top->loc, "");
			sp.colour = COLOUR_RED_BOLD;

			this->spans.push_back(sp);
		}

		std::sort(this->spans.begin(), this->spans.end(), [](const auto& a, const auto& b) -> bool { return a.loc.col < b.loc.col; });
		this->spans.erase(std::unique(this->spans.begin(), this->spans.end()), this->spans.end());

		strprinterrf("%s\n", getSpannedContext(this->top->loc, this->spans, &adjust, &num_width, &margin, true, true, COLOUR_CYAN_BOLD, ""));

		// ok now remove the extra thing.
		if(didExtra)
			this->spans.erase(std::find(this->spans.begin(), this->spans.end(), util::ESpan(this->top->loc, "")));

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
					auto realcursor = margin + col;

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
						cursor += 3 + strprinterrf("%s", spaces(2 + num_width + col - adjust - cursor));
						strprinterrf("%s|>%s ", COLOUR_CYAN_BOLD, COLOUR_RESET);

						spanscopy[i].msg = remaining.substr(segment.length());
						cursor += strprinterrf("%s", segment);
					}
				}
				else
				{
					cursor += 1 + strprinterrf("%s", spaces(2 + num_width + col - adjust - cursor));
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












void OverloadError::clear()
{
	this->cands.clear();
	this->top = 0;
}

OverloadError& OverloadError::addCand(Locatable* d, ErrorMsg* sp)
{
	this->cands[d] = sp;
	return *this;
}



void OverloadError::post()
{
	// first, post the original error.
	this->top->wordsBeforeContext = "(call site)";
	this->top->post();

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
			emg->type = MsgType::Note;
			emg->post();
		}
		else
		{
			auto spe = dcast(SpanError, emg);
			iceAssert(spe);

			spe->top = SimpleError::make(MsgType::Note, loc->loc, "candidate %d was defined here:", cand_counter++);
			spe->highlightActual = false;
			spe->post();
		}
	}

	for(auto sub : this->subs)
		sub->post();
}






[[noreturn]] void doTheExit(bool trace)
{
	fprintf(stderr, "there were errors, compilation cannot continue\n");

	if(frontend::getAbortOnError()) abort();
	else                            exit(-1);
}













