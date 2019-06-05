// lscvm.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "backend.h"

namespace backend
{
	struct LSCVMBackend : Backend
	{
		LSCVMBackend(CompiledData& dat, std::vector<std::string> inputs, std::string output);
		virtual ~LSCVMBackend() { }

		virtual void performCompilation() override;
		virtual void optimiseProgram() override;
		virtual void writeOutput() override;

		virtual std::string str() override;


		private:

		void executeProgram(const std::string& input);

		std::string program;
	};
}
