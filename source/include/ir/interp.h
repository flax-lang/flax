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
	struct IRBlock;
	struct Function;
	struct GlobalValue;
	struct ConstantValue;

	namespace interp
	{
		//? is it bad form to name these the same as our fir structs??

		struct Value
		{
			fir::Value* val = 0;

			fir::Type* type = 0;
			size_t dataSize = 0;
			union {
				void* ptr;
				uint8_t data[32];
			};

			// kinda a dirty hack, we need to know whether or not to write-back the global after the interpreter
			// is done, cases where we modify stuff during compile-time execution. of course, the real value could
			// be *derived* from a global, eg. a GEP on a struct, or something. so, we persistently tag it.
			fir::GlobalValue* globalValTracker = 0;
		};

		struct Instruction
		{
			size_t opcode;
			fir::Value* result = 0;
			std::vector<fir::Value*> args;
		};

		struct Block
		{
			fir::IRBlock* blk = 0;
			std::vector<interp::Instruction> instructions;
		};

		struct Function
		{
			fir::Function* func = 0;
			bool isExternal = false;

			std::string extFuncName;
			interp::Block* entryBlock = 0;
			std::vector<interp::Block> blocks;
		};

		struct InterpState
		{
			InterpState(fir::Module* mod);
			~InterpState();

			void initialise(bool runGlobalInit);
			void finalise();

			interp::Function& compileFunction(fir::Function* fn);
			interp::Value runFunction(const interp::Function& fn, const std::vector<interp::Value>& args);

			interp::Value makeValue(fir::Value* ty);

			fir::ConstantValue* unwrapInterpValueIntoConstant(const interp::Value& val);

			struct Frame
			{
				size_t currentInstrIndex = 0;
				const interp::Block* currentBlock = 0;
				const interp::Block* previousBlock = 0;
				const interp::Function* currentFunction = 0;

				std::vector<void*> stackAllocs;

				fir::Value* callResultOutput = 0;

				std::unordered_map<fir::Value*, interp::Value> values;
			};

			// this is the executing state.
			std::vector<Frame> stackFrames;

			std::unordered_map<fir::Value*, std::pair<interp::Value, bool>> globals;
			std::vector<void*> globalAllocs;

			std::vector<char*> strings;

			// map from the id to the real function.
			// we don't want 'inheritance' here
			std::unordered_map<fir::Value*, interp::Function> compiledFunctions;

			fir::Module* module = 0;
		};
	}
}
















