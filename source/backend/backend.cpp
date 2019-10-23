// Backend.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "backends/llvm.h"
#include "backends/interp.h"

namespace backend
{
	Backend* Backend::getBackendFromOption(BackendOption opt, CompiledData& cd,
		const std::vector<std::string>& in, const std::string& out)
	{
		switch(opt)
		{
			case BackendOption::LLVM:
				return new LLVMBackend(cd, in, out);

			case BackendOption::Interpreter:
				return new FIRInterpBackend(cd, in, out);

			case BackendOption::Assembly_x64:
				return new x64Backend(cd, in, out);

			case BackendOption::None:
				return nullptr;

			case BackendOption::Invalid:
			default:
				_error_and_exit("invalid backend\n");
		}
	}

	std::string capabilitiesToString(BackendCaps::Capabilities caps)
	{
		std::vector<std::string> list;
		if(caps & BackendCaps::EmitAssembly)
			list.push_back("'emit assembly'");

		if(caps & BackendCaps::EmitObject)
			list.push_back("'emit object file'");

		if(caps & BackendCaps::EmitProgram)
			list.push_back("'emit compiled program'");

		if(caps & BackendCaps::JIT)
			list.push_back("'JIT'");


		if(list.size() == 1)
		{
			return list.front();
		}
		else if(list.size() == 2)
		{
			return list[0] + " and " + list[1];
		}
		else
		{
			std::string ret;
			for(size_t i = 0; i < list.size() - 1; i++)
				ret += list[i] + ", ";

			ret += "and " + list.back();

			return ret;
		}
	}
}
