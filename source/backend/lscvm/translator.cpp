// lscvm/translator.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <fstream>

#include "ir/module.h"

#include "frontend.h"
#include "backends/lscvm.h"


const std::string CONST_0               = "a";
const std::string CONST_1               = "b";
const std::string CONST_2               = "c";
const std::string CONST_3               = "d";
const std::string CONST_4               = "e";
const std::string CONST_5               = "f";
const std::string CONST_6               = "g";
const std::string CONST_7               = "h";
const std::string CONST_8               = "i";
const std::string CONST_9               = "j";

const std::string OP_ADD                = "A";
const std::string OP_HALT               = "B";
const std::string OP_CALL               = "C";
const std::string OP_DROP               = "D";
const std::string OP_READ_MEM           = "E";
const std::string OP_FETCH_STACK        = "F";
const std::string OP_JMP_REL_FWD        = "G";
const std::string OP_FETCH_DEL_STACK    = "H";
const std::string OP_PRINT_INT          = "I";
const std::string OP_COMPARE            = "J";
const std::string OP_WRITE_MEM          = "K";
const std::string OP_MULTIPLY           = "M";
const std::string OP_PRINT_CHAR         = "P";
const std::string OP_RETURN             = "R";
const std::string OP_SUBTRACT           = "S";
const std::string OP_DIVIDE             = "V";
const std::string OP_JMP_REL_IF_ZERO    = "Z";

const std::string INTRINSIC_PRINT_CHAR  = "lscvm.P";


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

		// watermark for constant memory -- starts at CONSTANT_OFFSET_IN_MEMORY (0x12000)
		int32_t constantMemoryWatermark = 0;

		util::hash_map<fir::ConstantValue*, std::string> cachedConstants;

		int32_t currentStackFrameSize = 0;
		util::hash_map<size_t, int32_t> stackFrameValueMap;

		size_t currentStackOffset = 0;
		util::hash_map<size_t, int32_t> stackValues;
	};

	constexpr size_t WORD_SIZE                      = 4;

	constexpr int32_t MAX_RELOCATION_SIZE           = 32;

	// spaces are also no-ops, so that's good.
	constexpr char EmptyRelocation[]                = "                                ";

	// limits are imposed by the vm!
	constexpr int32_t MAX_PROGRAM_SIZE              = 0x2000;

	constexpr int32_t STACK_POINTER_IN_MEMORY       = 0x10000;
	constexpr int32_t STACK_FRAME_IN_MEMORY         = 0x10001;

	constexpr int32_t CONSTANT_OFFSET_IN_MEMORY     = 0x12000;
	constexpr int32_t MAX_MEMORY_SIZE               = 0x13880;


	/*
		! convention !
		* multi-word values are stored in BIG-ENDIAN FORMAT!!!


		* function calling
		arguments are pushed RIGHT TO LEFT. ie. the last argument will be pushed first
		this follows cdecl calling convention.

		since we have no registers, return value will be pushed on the stack before a return. in effect,
		doing 'C' will pop the function and any arguments, the push the return value (if any).

		typechecking should have ensured we don't try to do anything funny with void functions

		so before a call, the stack will look like this, for some foo(1, 2, 3)
		[ 3, 2, 1, <foo> ].

		there are no registers so there's nothing to preserve.


		* local variables
		since we're doing SSA, everything is immutable. we can use 'F' to fetch from the stack, so all those
		temporary values can just live on the stack.

		for allocas, we must spill them to memory, because we can't modify the contents of the stack.
	*/















	static size_t getSizeInWords(fir::Type* ty)
	{
		auto sz = fir::getSizeOfType(ty);
		if(sz == 0) return 0;

		return std::max((size_t) 1, sz / WORD_SIZE);
	}

	static std::string makeinstr()
	{
		return "";
	}

	template<typename... Args>
	static std::string makeinstr(const std::string& a, Args... args)
	{
		return a + makeinstr(args...);
	}



	static std::string createConstant(State* st, fir::ConstantValue* c)
	{
		if(auto ci = dcast(fir::ConstantInt, c))
		{
			std::string ret = "";
			if(ci->getType()->toPrimitiveType()->isSigned())
				ret = createNumber(ci->getSignedValue());

			else
				ret = createNumber(ci->getUnsignedValue());

			// we don't support integers > 32-bits, but just fill in the rest with 0s.
			for(size_t i = 1; i < getSizeInWords(c->getType()); i++)
				ret += makeinstr(CONST_0);

			st->cachedConstants[c] = ret;
			return ret;
		}
		else
		{
			return "";
		}
	}


	static std::string calcAddrInStackFrame(State* st, int32_t addr)
	{
		// basically, read from the current stack pointer,
		// subtract the maxstackwatermark, add the address.
		auto ofs = st->currentStackFrameSize - addr;
		return makeinstr(createNumber(ofs), createNumber(STACK_POINTER_IN_MEMORY), OP_READ_MEM, OP_SUBTRACT);
	};


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
			if(auto it = st->stackFrameValueMap.find(fv->id); it != st->stackFrameValueMap.end())
			{
				return calcAddrInStackFrame(st, it->second);
			}
			else if(auto it = st->stackValues.find(fv->id); it != st->stackValues.end())
			{
				// we need to fetch the number from deep in the stack, possibly.
				// calculate how far back we need to go. 0 = it's at the top already, up to a max of
				// st.currentStackOffset - 1.

				std::string ret;

				// fetch however many words it needs.
				for(size_t i = 0; i < getSizeInWords(fv->getType()); i++)
				{
					auto ofs = createNumber(st->currentStackOffset + i - 1 - it->second);

					// fetch it.
					ret += makeinstr(ofs, OP_FETCH_STACK);
				}

				return ret;
			}
			else
			{
				error("no value for id '%zu'", fv->id);
				return "";
			}
		}
	}









	void LSCVMBackend::performCompilation()
	{
		State st;
		st.firmod = this->compiledData.module;

		st.constantMemoryWatermark = CONSTANT_OFFSET_IN_MEMORY;

		for(auto string : st.firmod->_getGlobalStrings())
		{
			std::string init;

			int32_t loc = st.constantMemoryWatermark;
			for(char c : string.first)
			{
				init += makeinstr(createNumber(c), createNumber(st.constantMemoryWatermark), OP_WRITE_MEM);
				st.constantMemoryWatermark++;
			}

			st.memoryInitialisers.push_back(init);
			st.memoryValueMap[string.second->id] = loc;
		}

		// setup the stack pointer.
		{
			auto sp_addr = createNumber(STACK_POINTER_IN_MEMORY);   // 0x10000
			auto sp = createNumber(STACK_FRAME_IN_MEMORY);          // 0x10004

			st.memoryInitialisers.push_back(makeinstr(sp, sp_addr, OP_WRITE_MEM));
		}




		auto decay = [&st](fir::Value* fv, const std::string& lv) -> std::string {
			if(fv->islorclvalue())
				return makeinstr(lv, OP_READ_MEM);

			else
				return lv;
		};

		auto getUndecayedOperand = [&st](fir::Instruction* instr, size_t op) -> std::string {
			iceAssert(op < instr->operands.size());

			auto oper = instr->operands[op];
			return getValue(&st, oper);
		};

		auto getOperand = [&st, &decay](fir::Instruction* instr, size_t op) -> std::string {
			iceAssert(op < instr->operands.size());

			auto oper = instr->operands[op];
			return decay(oper, getValue(&st, oper));
		};






		for(auto fn : st.firmod->getAllFunctions())
		{
			if(fn->getBlockList().empty())
				continue;


			st.program += strprintf("\n\n; function %s\n", fn->getName().str());

			// this one is for the real stack
			st.stackValues.clear();
			st.currentStackOffset = 0;

			// this one is for the stack frame, ie. what lives in memory.
			st.stackFrameValueMap.clear();

			st.functionLocations[fn->id] = st.program.size();

			st.currentStackFrameSize = 0;
			for(auto t : fn->getStackAllocations())
				st.currentStackFrameSize += getSizeInWords(t);


			//* this is the function prologue! essentially
			//* push %rbp; mov %rsp, %rbp; sub $N, %rsp
			{
				st.program += "\n; prologue\n";

				// now that we know how big the stack frame must be, we store the current stack pointer
				// (on the stack, just by reading from it)
				st.program += makeinstr(createNumber(STACK_POINTER_IN_MEMORY), OP_READ_MEM);

				// then, we change the stack pointer. first, since the old value is already on the stack,
				// use 'F' to duplicate it.
				st.program += makeinstr(CONST_0, OP_FETCH_STACK);

				// then, add our 'maxwatermark' to it.
				st.program += makeinstr(createNumber(st.currentStackFrameSize), OP_ADD);

				// finally, store it into the pointer.
				st.program += makeinstr(createNumber(STACK_POINTER_IN_MEMORY), OP_WRITE_MEM);
			}


			int32_t currentStackWatermark = 0;
			auto allocStackMem = [&st, &currentStackWatermark](fir::Type* ty) -> int32_t {

				auto sz = getSizeInWords(ty);
				iceAssert(currentStackWatermark + sz <= st.currentStackFrameSize);

				auto ret = currentStackWatermark;
				currentStackWatermark += sz;

				return ret;
			};



			for(auto block : fn->getBlockList())
			{
				st.program += strprintf("\n\n; block %s\n", block->getName().str());

				st.basicBlockLocations[block->id] = st.program.size();

				for(auto inst : block->getInstructions())
				{
					switch(inst->opKind)
					{
						case fir::OpKind::Signed_Add:
						case fir::OpKind::Signed_Sub:
						case fir::OpKind::Signed_Mul:
						case fir::OpKind::Signed_Div:
						case fir::OpKind::Unsigned_Add:
						case fir::OpKind::Unsigned_Sub:
						case fir::OpKind::Unsigned_Mul:
						case fir::OpKind::Unsigned_Div:
						{
							iceAssert(inst->operands.size() == 2);
							auto b = getOperand(inst, 0);
							auto a = getOperand(inst, 1);

							std::string op;
							switch(inst->opKind)
							{
								case fir::OpKind::Signed_Add: case fir::OpKind::Unsigned_Add:
									op = OP_ADD; break;
								case fir::OpKind::Signed_Sub: case fir::OpKind::Unsigned_Sub:
									op = OP_SUBTRACT; break;
								case fir::OpKind::Signed_Mul: case fir::OpKind::Unsigned_Mul:
									op = OP_MULTIPLY; break;
								case fir::OpKind::Signed_Div: case fir::OpKind::Unsigned_Div:
									op = OP_DIVIDE; break;
								default:
									iceAssert(0); break;
							}

							st.program += makeinstr(a, b, op);

							st.stackValues[inst->realOutput->id] = st.currentStackOffset;
							st.currentStackOffset++;

							break;
						}









						case fir::OpKind::Value_CreateLVal:
						{
							st.program += "\n; create lvalue\n";

							iceAssert(inst->operands.size() == 1);
							fir::Type* ft = inst->operands[0]->getType();

							auto stackaddr = allocStackMem(ft);

							// small opt: only make the base address once, use 'F' to get it subsequently
							st.program += calcAddrInStackFrame(&st, stackaddr);

							for(size_t i = 0; i < getSizeInWords(ft); i++)
							{
								auto ofs = createNumber(i);

								// write 0s.
								st.program += makeinstr(CONST_5, CONST_1, OP_FETCH_STACK, ofs, OP_ADD, OP_WRITE_MEM);
							}

							// throw the thing away
							st.program += makeinstr(OP_DROP);

							st.stackFrameValueMap[inst->realOutput->id] = stackaddr;
							break;
						}

						case fir::OpKind::Value_Store:
						{
							iceAssert(inst->operands.size() == 2);
							auto val = getOperand(inst, 0);
							auto ptr = getUndecayedOperand(inst, 1);

							// see how big it is..
							auto sz = getSizeInWords(inst->operands[0]->getType());

							// presumably the size of the value will be the same!
							st.program += val;

							// same optimisation -- push the address first, then use 'F' to calculate offsets.
							st.program += ptr;

							for(size_t i = 0; i < sz; i++)
							{
								auto ofs = createNumber(i);

								// this is the offset of the 'current word' for multi-word values.
								auto valofs = createNumber(sz - i);

								st.program += makeinstr(valofs, OP_FETCH_STACK, CONST_1, OP_FETCH_STACK, ofs, OP_ADD, OP_WRITE_MEM);
							}

							st.program += makeinstr(OP_DROP);

							for(size_t i = 0; i < sz; i++) // drop the value also
								st.program += makeinstr(OP_DROP);

							break;
						}







						case fir::OpKind::Value_CallFunction:
						{
							st.program += "\n; call\n";
							iceAssert(inst->operands.size() >= 1);

							fir::Function* fn = dcast(fir::Function, inst->operands[0]);
							iceAssert(fn);

							if(fn->isIntrinsicFunction())
							{
								if(fn->getName().str() == INTRINSIC_PRINT_CHAR)
								{
									iceAssert(inst->operands.size() == 2);
									auto arg = getOperand(inst, 1);

									st.program += makeinstr(arg, OP_PRINT_CHAR);
								}
								else
								{
									error("unknown intrinsic '%s'", fn->getName().str());
								}
							}
							else
							{
								// throw in a relocation.
								error("what function '%s'", fn->getName().str());
							}

							break;
						}

						default:
							warn("unhandled: '%s'", inst->str());
							break;
					}
				}
			}

			// drop all the locals
			for(size_t i = 0; i < st.currentStackOffset; i++)
				st.program += makeinstr(OP_DROP);


			//* this is the function epilogue
			//* mov %rbp, %rsp; pop %rbp
			{
				// so what we do is just restore the value on the stack, which, barring any suspicious things, should
				// still be there -- but behind any return values.

				st.program += "\n; epilogue\n";
				size_t returnValueSize = getSizeInWords(fn->getReturnType());
				st.program += makeinstr(createNumber(returnValueSize), OP_FETCH_DEL_STACK);

				// now it's at the top -- we write that to the stack pointer place.
				st.program += makeinstr(createNumber(STACK_POINTER_IN_MEMORY), OP_WRITE_MEM);
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
			printf("\ncompiled program (%#zx bytes):\n\n", this->program.size());
			printf("%s\n\n", this->program.c_str());

			this->program += "?!";
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
































