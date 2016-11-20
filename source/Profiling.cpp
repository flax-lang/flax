// Profiling.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "profile.h"
#include "iceassert.h"

#include <vector>
#include <unordered_map>

namespace prof
{
	struct Data
	{
		Data(std::string n)
		{
			this->calls = 0;
			this->name = n;
			this->parent = 0;

			this->totalTime = 0;
			this->avgTime = 0;
		}

		size_t calls;
		Data* parent;
		std::string name;
		std::vector<Data*> subs;
		std::vector<double> timings;


		double totalTime;
		double avgTime;
	};

	// Functor to compare by the Mth element

	static bool compare(Data* t1, Data* t2)
	{
		return t2->avgTime < t1->avgTime;
	}



	static std::unordered_map<int, Data*> database;
	static std::unordered_map<std::string, Data*> namemap;
	static std::unordered_map<int, std::vector<Data*>> stack;



	Profile::Profile(std::string n) : Profile(PROFGROUP_MISC, n) { }
	Profile::Profile(int grp, std::string n)
	{
		this->name = n;
		this->begin = prof::clock::now();
		this->didRecord = false;
		this->group = 0;

		Data* nd = namemap[this->name];
		if(!nd)
		{
			nd = new Data(this->name);
			namemap[this->name] = nd;
		}



		if(stack[this->group].size() > 0)
		{
			auto parent = stack[this->group].back();
			auto& subs = stack[this->group].back()->subs;

			// must check that (a) we don't become our own parent, and (b) we handle recursion properly (check siblings of parent)
			if(nd != parent && std::find(subs.begin(), subs.end(), nd) == subs.end() && (parent->parent == 0
				|| std::find(parent->parent->subs.begin(), parent->parent->subs.end(), nd) == parent->parent->subs.end()))
			{
				subs.push_back(nd);
				nd->parent = parent;
			}
		}
		else
		{
			database[this->group] = nd;
		}

		stack[this->group].push_back(nd);
	}







	void Profile::finish()
	{
		iceAssert(stack.size() > 0);

		this->didRecord = true;

		using namespace std::chrono;
		using unit = duration<double, std::milli>;

		auto end = prof::clock::now();

		auto nd = namemap[this->name];
		iceAssert(nd);

		nd->calls++;
		nd->timings.push_back(duration_cast<unit>(end - this->begin).count());

		if(nd != stack[this->group].back())
			_error_and_exit("Overlapping profiling regions at the same scope level are not supported");

		stack[this->group].pop_back();
	}



	Profile::~Profile()
	{
		if(!this->didRecord)
			this->finish();

		this->didRecord = true;
	}




	bool Profile::operator == (const Profile& other) const
	{
		return this->begin == other.begin && this->name == other.name && this->group == other.group;
	}

	bool Profile::operator != (const Profile& other) const
	{
		return !(*this == other);
	}










	using namespace std::chrono;
	static void sortRecords(std::vector<Data*>& subs)
	{
		std::sort(subs.begin(), subs.end(), prof::compare);
	}

	static void recursivelyCalculateRecords(Data* record)
	{
		double sum = 0;
		for(auto n : record->timings)
			sum += n;

		record->totalTime = sum;
		record->avgTime = sum / (double) record->calls;

		for(auto s : record->subs)
			recursivelyCalculateRecords(s);

		sortRecords(record->subs);
	}

	static void printRecordSet(Data* record, size_t nest)
	{
		if(record == 0) return;

		recursivelyCalculateRecords(record);

		std::string spaces;
		if(nest > 0)
		{
			for(size_t i = 0; i < 1 + (3 * (nest - 1)); i++)
				spaces += " ";

			spaces += "- ";
		}

		printf("| %-67s | %-5zu | %-10.3lf | %-15.4lf |\n", (spaces + record->name).c_str(), record->calls,
			record->avgTime, record->totalTime);

		for(auto a : record->subs)
			printRecordSet(a, nest + 1);
	}

	void printResults()
	{
		printf("\n");
		printf("|                                name                                 | calls |   avg/ms   |    total/ms     |\n");
		printf("|---------------------------------------------------------------------|-------|------------|-----------------|\n");
		printRecordSet(database[0], 0);
		printf("|------------------------------------------------------------------------------------------------------------|\n");
		// printRecordSet(database[PROFGROUP_MISC], 0);
		// printf("|------------------------------------------------------------------------------------------------------------|\n");
		// printRecordSet(database[PROFGROUP_LLVM], 0);
		// printf("|------------------------------------------------------------------------------------------------------------|\n");

		printf("\n\n");
	}
}











































