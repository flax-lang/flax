// errors.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "frontend.h"
#include "precompile.h"

template <typename... Ts>
inline void debuglog(const char* s, Ts... ts)
{
	tinyformat::format(std::cerr, s, ts...);
}


// error shortcuts

// Expected $, found '$' instead
void expected(const Location& loc, std::string, std::string) __attribute__((noreturn));

// Expected $ after $, found '$' instead
void expectedAfter(const Location& loc, std::string, std::string, std::string) __attribute__((noreturn));

// Unexpected $
void unexpected(const Location& loc, std::string) __attribute__((noreturn));


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








namespace ast
{
	struct Expr;
}


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

// Location getHighlightExtent(ast::Expr* e);

[[noreturn]] void doTheExit();
std::string printContext(HighlightOptions ops);

template <typename... Ts>
std::string __error_gen(HighlightOptions ops, const char* msg, const char* type, Ts... ts)
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
		ret += strprintf("%s(%s:%zu:%zu) ", COLOUR_BLACK_BOLD, filename, ops.caret.line + 1, ops.caret.col + 1);

	if(empty)	ret += strprintf("%s%s%s%s", colour, type, COLOUR_RESET, dobold ? COLOUR_BLACK_BOLD : "");
	else		ret += strprintf("%s%s%s%s: ", colour, type, COLOUR_RESET, dobold ? COLOUR_BLACK_BOLD : "");

	ret += tinyformat::format(msg, ts...);

	ret += strprintf("%s\n", COLOUR_RESET);

	if(ops.caret.fileID > 0)
	{
		std::vector<std::string> lines;
		if(ops.caret.fileID > 0)
			ret += printContext(ops);
	}

	ret += "\n";
	return ret;
}


#define ERROR_FUNCTION(name, type, attr, doexit)																				\
template <typename... Ts> [[attr]] void name (const char* fmt, Ts... ts)														\
{ fputs(__error_gen(HighlightOptions(), fmt, type, ts...).c_str(), stderr); if(doexit) doTheExit(); }							\
																																\
template <typename... Ts> [[attr]] void name (Locatable* e, const char* fmt, Ts... ts)											\
{ fputs(__error_gen(HighlightOptions(e ? e->loc : Location()), fmt, type, ts...).c_str(), stderr); if(doexit) doTheExit(); }	\
																																\
template <typename... Ts> [[attr]] void name (const Location& l, const char* fmt, Ts... ts)										\
{ fputs(__error_gen(HighlightOptions(l), fmt, type, ts...).c_str(), stderr); if(doexit) doTheExit(); }							\
																																\
template <typename... Ts> [[attr]] void name (const Location& l, HighlightOptions ops, const char* fmt, Ts... ts)				\
{																																\
	if(ops.caret.fileID == 0) ops.caret = (l.fileID != 0 ? l : Location());														\
	fputs(__error_gen(ops, fmt, type, ts...).c_str(), stderr);																	\
	if(doexit) doTheExit();																										\
}																																\
																																\
template <typename... Ts> [[attr]] void name (Locatable* e, HighlightOptions ops, const char* fmt, Ts... ts)					\
{																																\
	if(ops.caret.fileID == 0) ops.caret = (e ? e->loc : Location());															\
	fputs(__error_gen(ops, fmt, type, ts...).c_str(), stderr);																	\
	if(doexit) doTheExit();																										\
}


#define STRING_ERROR_FUNCTION(name, type)																			\
template <typename... Ts> std::string name (const char* fmt, Ts... ts)												\
{ return __error_gen(HighlightOptions(), fmt, type, ts...); }														\
																													\
template <typename... Ts> std::string name (Locatable* e, const char* fmt, Ts... ts)								\
{ return __error_gen(HighlightOptions(e ? e->loc : Location()), fmt, type, ts...); }								\
																													\
template <typename... Ts> std::string name (const Location& l, const char* fmt, Ts... ts)							\
{ return __error_gen(HighlightOptions(l), fmt, type, ts...); }														\
																													\
template <typename... Ts> std::string name (const Location& l, HighlightOptions ops, const char* fmt, Ts... ts)		\
{																													\
	if(ops.caret.fileID == 0) ops.caret = (l.fileID != 0 ? l : Location());											\
	fputs(__error_gen(ops, fmt, type, ts...).c_str(), stderr);														\
}																													\
																													\
template <typename... Ts> std::string name (Locatable* e, HighlightOptions ops, const char* fmt, Ts... ts)			\
{																													\
	if(ops.caret.fileID == 0) ops.caret = e ? e->loc : Location();													\
	return __error_gen(ops, fmt, type, ts...);																		\
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



// template <typename... Ts> [[noreturn]] void error(const char* fmt, Ts... ts)
// {
// 	fputs(__error_gen(HighlightOptions(), fmt, "Error", true, ts...), stderr);
// 	abort();
// }

// template <typename... Ts> [[noreturn]] void error(Locatable* e, const char* fmt, Ts... ts)
// {
// 	fputs(__error_gen(HighlightOptions(e ? e->loc : Location()), fmt, "Error", true, ts...), stderr);
// 	abort();
// }

// template <typename... Ts> [[noreturn]] void error(const Location& loc, const char* fmt, Ts... ts)
// {
// 	fputs(__error_gen(HighlightOptions(loc), fmt, "Error", true, ts...), stderr);
// 	abort();
// }

// template <typename... Ts> [[noreturn]] void error(Locatable* e, HighlightOptions ops, const char* fmt, Ts... ts)
// {
// 	if(ops.caret.fileID == 0)
// 		ops.caret = e ? e->loc : Location();

// 	fputs(__error_gen(ops, fmt, "Error", true, ts...), stderr);

// 	abort();
// }
















