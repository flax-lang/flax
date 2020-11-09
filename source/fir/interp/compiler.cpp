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

		ret.result = finstr->realOutput;
		ret.opcode = static_cast<uint64_t>(finstr->opKind);

		for(auto a : finstr->operands)
			ret.args.push_back(a);

		return ret;
	}

	static interp::Block compileBlock(InterpState* is, fir::Function* parent, fir::IRBlock* fib)
	{
		iceAssert(fib);

		interp::Block ret;
		ret.blk = fib;
		ret.instructions = zfu::map(fib->getInstructions(), [is, parent](fir::Instruction* i) -> interp::Instruction {
			return compileInstruction(is, parent, i);
		});

		return ret;
	}

	interp::Function& InterpState::compileFunction(fir::Function* fn)
	{
		iceAssert(fn);

		interp::Function ret;
		ret.func = fn;

		ret.blocks = zfu::map(fn->getBlockList(), [fn, this](fir::IRBlock* b) -> interp::Block {
			return compileBlock(this, fn, b);
		});

		if(fn->isCStyleVarArg())
			iceAssert(ret.blocks.empty());

		if(ret.blocks.empty())
			ret.isExternal = true, ret.extFuncName = fn->getName().name;


		// add it.
		this->compiledFunctions[ret.func] = ret;
		return this->compiledFunctions[ret.func];
	}
}
}

















