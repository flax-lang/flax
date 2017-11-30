// precompile.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#ifndef PRECOMPILE_H
#define PRECOMPILE_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include <map>
#include <stack>
#include <deque>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <functional>
#include <unordered_map>

#ifndef __has_include
#error "Please switch to a compiler that supports '__has_include'"
#endif

/*
	STRING_VIEW_TYPE documentation

	0: normal, std::string_view
	1: experimental, std::experimental::string_view
	2: external, stx::string_view
*/
#if __has_include(<string_view>) && _HAS_CXX17
	#include <string_view>
	#define STRING_VIEW_TYPE 0
#elif __has_include(<experimental/string_view>) && _HAS_CXX17
	#include <experimental/string_view>
	#define STRING_VIEW_TYPE 1
#else
	// #error "Please switch to a compiler that supports 'string_view', or change your c++ standard version"
	#include "../external/stx/string_view.hpp"
	#define STRING_VIEW_TYPE 2
#endif

#include "../external/mpreal/mpreal.h"

enum class VisibilityLevel;

namespace fir { struct Type; }
namespace tinyformat { void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, fir::Type* ty); }
namespace tinyformat { void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, VisibilityLevel vl); }

#define TINYFORMAT_ERROR(x)
#include "../external/tinyformat/tinyformat.h"

#endif






