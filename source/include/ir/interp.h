// interp.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <vector>
#include <string>
#include <unordered_map>

namespace fir
{
	struct Type;
	struct Value;
	struct Module;
	struct Function;
	struct ConstantValue;

	namespace interp
	{
		//? is it bad form to name these the same as our fir structs??

		struct Value
		{
			size_t id = 0;
			fir::Type* type = 0;
			size_t dataSize = 0;
			union {
				void* ptr;
				uint8_t data[32];
			};
		};

		struct Instruction
		{
			size_t opcode;
			size_t result;
			fir::Value* orig = 0;
			std::vector<size_t> args;
		};

		struct Block
		{
			size_t id;
			std::vector<interp::Instruction> instructions;
		};

		struct Function
		{
			size_t id = 0;
			bool isExternal = false;
			fir::Function* origFunction = 0;

			std::string extFuncName;
			interp::Block* entryBlock = 0;
			std::vector<interp::Block> blocks;
		};

		struct InterpState
		{
			InterpState(fir::Module* mod);

			interp::Function& compileFunction(fir::Function* fn);
			interp::Value runFunction(const interp::Function& fn, const std::vector<interp::Value>& args);

			struct Frame
			{
				const interp::Block* currentBlock = 0;
				const interp::Block* previousBlock = 0;
				const interp::Function* currentFunction = 0;

				std::unordered_map<size_t, interp::Value> values;
			};

			// this is the executing state.
			std::vector<Frame> stackFrames;


			std::unordered_map<size_t, interp::Value> globals;

			// map from the id to the real function.
			// we don't want 'inheritance' here
			std::unordered_map<size_t, interp::Function> compiledFunctions;

			// map from name to the key of the map above
			std::unordered_map<std::string, size_t> functionNameMap;


			fir::Module* module = 0;
		};
	}
}
















