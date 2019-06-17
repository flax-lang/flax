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

		ret.origRes = finstr->realOutput;
		ret.result = finstr->realOutput->id;
		ret.opcode = (uint64_t) finstr->opKind;

		for(auto a : finstr->operands)
			ret.args.push_back(a->id);

		return ret;
	}

	static interp::Block compileBlock(InterpState* is, fir::Function* parent, fir::IRBlock* fib)
	{
		iceAssert(fib);

		interp::Block ret;
		ret.id = fib->id;
		ret.instructions = util::map(fib->getInstructions(), [is, parent](fir::Instruction* i) -> interp::Instruction {
			return compileInstruction(is, parent, i);
		});

		return ret;
	}

	interp::Function& InterpState::compileFunction(fir::Function* fn)
	{
		iceAssert(fn);

		interp::Function ret;
		ret.id = fn->id;
		ret.origFunction = fn;

		ret.blocks = util::map(fn->getBlockList(), [fn, this](fir::IRBlock* b) -> interp::Block {
			return compileBlock(this, fn, b);
		});

		if(ret.blocks.empty())
			ret.isExternal = true, ret.extFuncName = fn->getName().name;


		// add it.
		this->compiledFunctions[ret.id] = ret;
		this->functionNameMap[fn->getName().mangled()] = ret.id;

		return this->compiledFunctions[ret.id];
	}
}
}

















