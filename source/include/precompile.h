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

#if __has_include(<string_view>)
	#include <string_view>
	#define STRING_VIEW_IS_EXP 0
#elif __has_include(<experimental/string_view>)
	#include <experimental/string_view>
	#define STRING_VIEW_IS_EXP 1
#else
	#error "Please switch to a compiler that supports 'string_view', or change your c++ standard version"
#endif

#include "../external/mpreal/mpreal.h"

#define TINYFORMAT_ERROR(x)
#include "../external/tinyformat/tinyformat.h"

#endif
