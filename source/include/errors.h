// errors.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"

#include "precompile.h"

template <typename... Ts>
inline void debuglog(const char* s, Ts... ts)
{
	tinyformat::format(std::cerr, s, ts...);
}


// error shortcuts

// Expected $, found '$' instead
[[noreturn]] void expected(const Location& loc, std::string, std::string);

// Expected $ after $, found '$' instead
[[noreturn]] void expectedAfter(const Location& loc, std::string, std::string, std::string);

// Unexpected $
[[noreturn]] void unexpected(const Location& loc, std::string);


#define INTUNSPEC_TYPE_STRING			"int"
#define UINTUNSPEC_TYPE_STRING			"uint"

#define FLOAT_TYPE_STRING				"float"
#define DOUBLE_TYPE_STRING				"double"

#define INT8_TYPE_STRING				"i8"
#define INT16_TYPE_STRING				"i16"
#define INT32_TYPE_STRING				"i32"
#define INT64_TYPE_STRING				"i64"
#define INT128_TYPE_STRING				"i128"

#define UINT8_TYPE_STRING				"u8"
#define UINT16_TYPE_STRING				"u16"
#define UINT32_TYPE_STRING				"u32"
#define UINT64_TYPE_STRING				"u64"
#define UINT128_TYPE_STRING				"u128"

#define FLOAT32_TYPE_STRING				"f32"
#define FLOAT64_TYPE_STRING				"f64"
#define FLOAT80_TYPE_STRING				"f80"
#define FLOAT128_TYPE_STRING			"f128"

#define STRING_TYPE_STRING				"string"
#define CHARACTER_TYPE_STRING			"char"

#define UNICODE_STRING_TYPE_STRING		"ustring"
#define UNICODE_CHARACTER_TYPE_STRING	"rune"

#define BOOL_TYPE_STRING				"bool"
#define VOID_TYPE_STRING				"void"

#define ANY_TYPE_STRING					"any"








namespace ast { struct Expr; }
namespace sst { struct Expr; }


struct HighlightOptions
{
	Location caret;
	std::vector<Location> underlines;
	bool drawCaret;

	HighlightOptions() : drawCaret(true) { }
	HighlightOptions(const Location& c) : caret(c), underlines({ }), drawCaret(true) { }
	HighlightOptions(const Location& c, std::vector<Location> u) : caret(c), underlines(u), drawCaret(true) { }
	HighlightOptions(const Location& c, std::vector<Location> u, bool dc) : caret(c), underlines(u), drawCaret(dc) { }
};

void pushErrorLocation(Locatable* l);
void popErrorLocation();

std::string printContext(HighlightOptions ops);

namespace frontend
{
	const std::string& getFilenameFromID(size_t fileID);
	std::string getFilenameFromPath(std::string path);
}

std::string __error_gen_part1(const HighlightOptions& ops, const char* msg, const char* type);
std::string __error_gen_part2(const HighlightOptions& ops);
std::string __error_gen_backtrace(const HighlightOptions& ops);

template <typename... Ts>
std::string __error_gen(const HighlightOptions& ops, const char* msg, const char* type, bool dotrace, Ts... ts)
{
	std::string ret = __error_gen_part1(ops, msg, type);
	ret += tinyformat::format(msg, ts...);
	ret += __error_gen_part2(ops);

	if(dotrace) ret += __error_gen_backtrace(ops);

	return ret;
}


#define ERROR_FUNCTION(name, type, attr, doexit)                                                                                            \
template <typename... Ts> [[attr]] void name (const char* fmt, Ts... ts)                                                                    \
{ fputs(__error_gen(HighlightOptions(), fmt, type, doexit, ts...).c_str(), stderr); if(doexit) doTheExit(false); }							\
																																			\
template <typename... Ts> [[attr]] void name (Locatable* e, const char* fmt, Ts... ts)                                                      \
{ fputs(__error_gen(HighlightOptions(e ? e->loc : Location()), fmt, type, doexit, ts...).c_str(), stderr); if(doexit) doTheExit(false); }   \
																																			\
template <typename... Ts> [[attr]] void name (const Location& l, const char* fmt, Ts... ts)                                                 \
{ fputs(__error_gen(HighlightOptions(l), fmt, type, doexit, ts...).c_str(), stderr); if(doexit) doTheExit(false); }							\
																																			\
template <typename... Ts> [[attr]] void name (const Location& l, HighlightOptions ops, const char* fmt, Ts... ts)                       	\
{                                                                                                                                       	\
	if(ops.caret.fileID == 0) ops.caret = (l.fileID != 0 ? l : Location());                                                             	\
	fputs(__error_gen(ops, fmt, type, doexit, ts...).c_str(), stderr);                                                                  	\
	if(doexit) doTheExit(false);                                                                                                            \
}                                                                                                                                           \
																																		    \
template <typename... Ts> [[attr]] void name (Locatable* e, HighlightOptions ops, const char* fmt, Ts... ts)                                \
{                                                                                                                                           \
	if(ops.caret.fileID == 0) ops.caret = (e ? e->loc : Location());                                                                        \
	fputs(__error_gen(ops, fmt, type, doexit, ts...).c_str(), stderr);                                                                      \
	if(doexit) doTheExit(false);                                                                                                            \
}


#define STRING_ERROR_FUNCTION(name, type)                                                                           \
template <typename... Ts> std::string name (const char* fmt, Ts... ts)                                              \
{ return __error_gen(HighlightOptions(), fmt, type, false, ts...); }                                                \
																													\
template <typename... Ts> std::string name (Locatable* e, const char* fmt, Ts... ts)                                \
{ return __error_gen(HighlightOptions(e ? e->loc : Location()), fmt, type, false, ts...); }                         \
																													\
template <typename... Ts> std::string name (const Location& l, const char* fmt, Ts... ts)                           \
{ return __error_gen(HighlightOptions(l), fmt, type, false, ts...); }                                               \
																													\
template <typename... Ts> std::string name (const Location& l, HighlightOptions ops, const char* fmt, Ts... ts)     \
{                                                                                                                   \
	if(ops.caret.fileID == 0) ops.caret = (l.fileID != 0 ? l : Location());                                         \
	return __error_gen(ops, fmt, type, false, ts...);                                                               \
}                                                                                                                   \
																													\
template <typename... Ts> std::string name (Locatable* e, HighlightOptions ops, const char* fmt, Ts... ts)          \
{                                                                                                                   \
	if(ops.caret.fileID == 0) ops.caret = e ? e->loc : Location();                                                  \
	return __error_gen(ops, fmt, type, false, ts...);                                                               \
}



ERROR_FUNCTION(error, "Error", noreturn, true);
ERROR_FUNCTION(info, "Note", maybe_unused, false);
ERROR_FUNCTION(warn, "Warning", maybe_unused, false);
ERROR_FUNCTION(exitless_error, "Error", maybe_unused, false);

STRING_ERROR_FUNCTION(strinfo, "Note");
STRING_ERROR_FUNCTION(strerror, "Error");
STRING_ERROR_FUNCTION(strwarn, "Warning");

template <typename... Ts> std::string strbold(const char* fmt, Ts... ts)
{
	return strprintf("%s%s", COLOUR_RESET, COLOUR_BLACK_BOLD) + tinyformat::format(fmt, ts...) + strprintf("%s", COLOUR_RESET);
}
















