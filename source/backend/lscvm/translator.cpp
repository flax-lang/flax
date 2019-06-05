// lscvm/translator.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <fstream>

#include "ir/module.h"

#include "frontend.h"
#include "backends/lscvm.h"



constexpr char OP_ADD                   = 'A';
constexpr char OP_HALT                  = 'B';
constexpr char OP_CALL                  = 'C';
constexpr char OP_DROP                  = 'D';
constexpr char OP_READ_MEM              = 'E';
constexpr char OP_FETCH_STACK           = 'F';
constexpr char OP_JMP_REL_FWD           = 'G';
constexpr char OP_FETCH_DEL_STACK       = 'H';
constexpr char OP_PRINT_INT             = 'I';
constexpr char OP_COMPARE               = 'J';
constexpr char OP_WRITE_MEM             = 'K';
constexpr char OP_MULTIPLY              = 'M';
constexpr char OP_PRINT_CHAR            = 'P';
constexpr char OP_RETURN                = 'R';
constexpr char OP_SUBTRACT              = 'S';
constexpr char OP_DIVIDE                = 'V';
constexpr char OP_JMP_REL_IF_ZERO       = 'Z';

constexpr char INTRINSIC_PRINT_CHAR[]   = "lscvm.P";


std::string createNumber(int num)
{
	if(num < 0)
	{
		return "a" + createNumber(-num) + "S";
	}
	else if(num < 10)
	{
		return std::string(1, (char) (num + 'a'));
	}
	else if(num == 10)
	{
		return "cfM";
	}
	else
	{
		switch(num)
		{
			case 'a': return "jjMjAhA";
			case 'b': return "jjMjAiA";
			case 'c': return "jjMjAjA";
			case 'd': return "cfMcfMM";
			case 'e': return "cfMcfMMbA";
			case 'f': return "jiAcdMM";
			case 'g': return "jiAcdMMbA";
			case 'h': return "jeAiM";
			case 'i': return "hdfMM";
			case 'j': return "hdfMMbA";
			case 'k': return "hdfMMcA";
			case 'l': return "ggdMM";
			case 'm': return "ggdMMbA";
			case 'n': return "fgAfMcM";
			case 'o': return "fgAfMcMbA";
			case 'p': return "fgAfMcMcA";
			case 'q': return "fgAfMcMdA";
			case 'r': return "fgAfMcMeA";
			case 's': return "fgAfMcMfA";
			case 't': return "fgAfMcMgA";
			case 'u': return "fgAfMcMhA";
			case 'v': return "fgAfMcMiA";
			case 'w': return "fgAfMcMjA";
			case 'x': return "gcfcMMM";
			case 'y': return "fgAfgAM";
			case 'z': return "fgAfgAMbA";
			case '_': return "gfdMMfA";
			case '!': return "fgAdM";
			case '-': return "fddMM";

			// if we run into code size problems, i'm sure this algo can be optimised.
			default: {
				std::string ret;

				int x = num / 10;
				ret = createNumber(x) + createNumber(10) + "M";

				int y = num % 10;
				ret += createNumber(y) + "A";

				return ret;
			} break;
		}
	}
}


namespace backend
{
	struct State
	{
		fir::Module* firmod = 0;

		std::string program;

		int32_t memoryWatermark = 0;
		int32_t relocationOffset = 0;

		util::hash_map<size_t, int32_t> memoryValueMap;

		// this must run first to set up all our constants.
		std::vector<std::string> memoryInitialisers;

		// so we can jump around.
		util::hash_map<size_t, int32_t> functionLocations;
		util::hash_map<size_t, int32_t> basicBlockLocations;

		// map from the instruction index (ie. index in program) to the fir ID of the target block
		// we need to replace the instruction (or value) at that location with the real address...
		util::hash_map<int32_t, size_t> relocations;




		util::hash_map<fir::ConstantValue*, std::string> cachedConstants;
	};

	constexpr int32_t CONSTANT_OFFSET_IN_MEMORY = 0x10000;
	constexpr int32_t MAX_RELOCATION_SIZE       = 32;

	// spaces are also no-ops, so that's good.
	constexpr char EmptyRelocation[]            = "                                ";






	static std::string createConstant(State* st, fir::ConstantValue* c)
	{
		if(auto ci = dcast(fir::ConstantInt, c))
		{
			if(ci->getType()->toPrimitiveType()->isSigned())
				return st->cachedConstants[c] = createNumber(ci->getSignedValue());

			else
				return st->cachedConstants[c] = createNumber(ci->getUnsignedValue());
		}
		else
		{
			return "";
		}
	}


	static std::string getValue(State* st, fir::Value* fv)
	{
		if(auto fn = dcast(fir::Function, fv))
		{
			// hmm.
			return "";
		}
		else if(auto cv = dcast(fir::ConstantValue, fv))
		{
			return createConstant(st, cv);
		}
		else
		{
			return "";
		}
	}



	void LSCVMBackend::performCompilation()
	{
		State st;
		st.firmod = this->compiledData.module;


		for(auto string : st.firmod->_getGlobalStrings())
		{
			std::string init;

			int32_t loc = st.memoryWatermark;
			for(char c : string.first)
			{
				init += createNumber(c);
				init += createNumber(st.memoryWatermark);
				init += "K";

				st.memoryWatermark++;
			}

			st.memoryInitialisers.push_back(init);
			st.memoryValueMap[string.second->id] = loc;
		}



		for(auto fn : st.firmod->getAllFunctions())
		{
			st.functionLocations[fn->id] = st.program.size();

			// map from the fir id (of any temporary values) to the location in the stack.
			util::hash_map<size_t, int32_t> stackLocations;


			auto getOperand = [&st](fir::Instruction* instr, size_t op) -> std::string {
				iceAssert(op < instr->operands.size());

				auto oper = instr->operands[op];
				return getValue(&st, oper);
			};


			for(auto block : fn->getBlockList())
			{
				st.basicBlockLocations[block->id] = st.program.size();

				for(auto inst : block->getInstructions())
				{
					switch(inst->opKind)
					{



						case fir::OpKind::Value_CallFunction:
						{
							iceAssert(inst->operands.size() >= 1);

							fir::Function* fn = dcast(fir::Function, inst->operands[0]);
							iceAssert(fn);

							if(fn->isIntrinsicFunction())
							{
								if(fn->getName().str() == INTRINSIC_PRINT_CHAR)
								{
									iceAssert(inst->operands.size() == 2);
									auto arg = getOperand(inst, 1);

									st.program += arg;
									st.program += OP_PRINT_CHAR;
								}
								else
								{
									error("unknown intrinsic '%s'", fn->getName().str());
								}
							}
							else
							{
								error("what function '%s'", fn->getName().str());
							}

							break;
						}



						default:
							warn("unhandled");
							break;
					}
				}
			}
		}

















		{
			std::string tmp;
			for(const auto& mi : st.memoryInitialisers)
				tmp += mi;


			// add a jump to the global init function.
			std::string tmp2;

			// tmp2 += EmptyRelocation; tmp2 += OP_CALL;
			// st.relocations[0] = firmod->getFunction(Identifier("__global_init_function__", IdKind::Name))->id;

			st.program = (tmp + tmp2 + st.program);
			st.relocationOffset = tmp.size();

			// handle relocations.
			for(auto [ _instr, target ] : st.relocations)
			{
				auto instr = st.relocationOffset + _instr;

				// expect the relocation to be unfilled!
				if(st.program.find(EmptyRelocation, instr) != instr)
					error("wtf? '%s'");

				int32_t loc = 0;
				if(auto it = st.functionLocations.find(target); it != st.functionLocations.end())
					loc = it->second;

				else if(auto it = st.basicBlockLocations.find(target); it != st.basicBlockLocations.end())
					loc = it->second;

				else
					error("no relocation for value id %zu", target);

				loc += st.relocationOffset;
				auto str = createNumber(loc);

				if(str.size() > MAX_RELOCATION_SIZE)
				{
					error("size of constant '%d' exceeds maximum relocation size (%d); generated string was '%s'",
						loc, MAX_RELOCATION_SIZE, str);
				}

				if(str.size() < MAX_RELOCATION_SIZE)
					str += std::string(MAX_RELOCATION_SIZE - str.size(), ' ');

				iceAssert(str.size() == MAX_RELOCATION_SIZE);
				st.program.replace(instr, MAX_RELOCATION_SIZE, str);
			}
		}

		// st.program += "jjAjiAjhAjgAjfAjeAjdAjcAjbA jihgfedcba EPEPEPEPEPEPEPEPEPEPEPEPEP";


		this->program = st.program;
	}












	LSCVMBackend::LSCVMBackend(CompiledData& dat, std::vector<std::string> inputs, std::string output)
		: Backend(BackendCaps::EmitAssembly | BackendCaps::EmitProgram | BackendCaps::JIT, dat, inputs, output)
	{
	}

	void LSCVMBackend::writeOutput()
	{
		if(frontend::getOutputMode() == ProgOutputMode::RunJit)
		{
			printf("\ncompiled program (%#zx bytes):\n", this->program.size());
			printf("%s\n\n", this->program.c_str());

			this->executeProgram(this->program);
		}
		else
		{
			auto out = std::ofstream(this->outputFilename, std::ios::out);
			out << this->program;

			out.close();
		}
	}

	void LSCVMBackend::optimiseProgram()
	{
		// lol what?
	}

	std::string LSCVMBackend::str()
	{
		return "LSCVM";
	}
}
































