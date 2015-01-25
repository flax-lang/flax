// stacktrace.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdio.h>
void print_stacktrace(FILE* out = stderr, unsigned int max_frames = 63);
