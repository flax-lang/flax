// profile.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>

#include <string>
#include <chrono>

#define PROFGROUP_TOP			0
#define PROFGROUP_MISC			1
#define PROFGROUP_LLVM			2

namespace prof
{
	using clock = std::chrono::high_resolution_clock;

	struct Profile
	{
		Profile(std::string name);
		Profile(int group, std::string name);
		~Profile();

		bool operator == (const Profile&) const;
		bool operator != (const Profile&) const;

		void finish();

		std::chrono::time_point<clock> begin;
		std::string name;

		int group = 0;
		bool didRecord = 0;
	};

	void printResults();
}
















