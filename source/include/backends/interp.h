// interp.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "backend.h"

namespace fir::interp
{
	struct InterpState;
}

namespace backend
{
	struct FIRInterpBackend : Backend
	{
		FIRInterpBackend(CompiledData& dat, const std::vector<std::string>& inputs, const std::string& output);
		virtual ~FIRInterpBackend();

		virtual void performCompilation() override;
		virtual void optimiseProgram() override;
		virtual void writeOutput() override;

		virtual std::string str() override;

		protected:
			fir::interp::InterpState* is = 0;
	};
}
