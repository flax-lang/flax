// compiler.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "ir/value.h"
#include "ir/interp.h"
#include "ir/module.h"
#include "ir/function.h"
#include "ir/instruction.h"

namespace fir {
namespace interp
{
	static interp::Instruction compileInstruction(InterpState* is, fir::Function* parent, fir::Instruction* finstr)
	{
		iceAssert(finstr);

		interp::Instruction ret;

		ret.result = finstr->realOutput->id - parent->id;
		ret.opcode = (uint64_t) finstr->opKind;

		auto allGlobs = is->module->_getAllGlobals();
		for(auto a : finstr->operands)
		{
			uint64_t id = a->id;
			if(allGlobs.find(id) == allGlobs.end())
				id -= parent->id;

			// this should ensure we get relative IDs for everything that's not a global.
			// we can't just compare id < parent->id because globals can have any id
			// (eg we can add a bunch of globals at the end of the program, so they have big ids)
			ret.args.push_back(id);
		}

		return ret;
	}

	static interp::Block compileBlock(InterpState* is, fir::Function* parent, fir::IRBlock* fib)
	{
		iceAssert(fib);

		interp::Block ret;
		ret.id = fib->id - parent->id;
		ret.instructions = util::map(fib->getInstructions(), [is, parent](fir::Instruction* i) -> interp::Instruction {
			return compileInstruction(is, parent, i);
		});

		return ret;
	}

	void InterpState::compileFunction(fir::Function* fn)
	{
		iceAssert(fn);

		interp::Function ret;
		ret.id = fn->id;

		ret.blocks = util::map(fn->getBlockList(), [fn, this](fir::IRBlock* b) -> interp::Block {
			return compileBlock(this, fn, b);
		});

		if(ret.blocks.empty())
			ret.isExternal = true, ret.extFuncName = fn->getName().name;

		// add it.
		this->compiledFunctions[ret.id] = ret;
		this->functionNameMap[fn->getName().mangled()] = ret.id;
	}
}
}

















