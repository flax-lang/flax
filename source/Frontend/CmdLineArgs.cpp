// CmdLineArgs.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>

#include <vector>
#include <algorithm>

#include "backend.h"
#include "compiler.h"


#include <dlfcn.h>


static std::string parseQuotedString(char** argv, int& i)
{
	std::string ret;
	if(strlen(argv[i]) > 0)
	{
		if(argv[i][0] == '"' || argv[i][0] == '\'')
		{
			while(std::string(argv[i]).back() != '\'' && std::string(argv[i]).back() != '"')
			{
				ret += " " + std::string(argv[i]);
				i++;
			}
		}
		else
		{
			ret = argv[i];
		}
	}
	return ret;
}

#define VERSION_STRING	"0.92.0"



#define ARG_COMPILE_ONLY						"-c"
#define ARG_BACKEND								"-backend"
#define ARG_EMIT_LLVM_IR						"-emit-llvm"
#define ARG_LINK_FRAMEWORK						"-framework"
#define ARG_FRAMEWORK_SEARCH_PATH				"-F"
#define ARG_HELP								"-help"
#define ARG_JITPROGRAM							"-jit"
#define ARG_LINK_LIBRARY						"-l"
#define ARG_LIBRARY_SEARCH_PATH					"-L"
#define ARG_MCMODEL								"-mcmodel"
#define ARG_DISABLE_AUTO_GLOBAL_CONSTRUCTORS	"-no-auto-gconstr"
#define ARG_OUTPUT_FILE							"-o"
#define ARG_OPTIMISATION_LEVEL_SELECT			"-O"
#define ARG_POSINDEPENDENT						"-pic"
#define ARG_PRINT_FIR							"-print-fir"
#define ARG_PRINT_LLVMIR						"-print-lir"
#define ARG_PROFILE								"-profile"
#define ARG_RUNPROGRAM							"-run"
#define ARG_SHOW_CLANG_OUTPUT					"-show-clang"
#define ARG_SYSROOT								"-sysroot"
#define ARG_TARGET								"-target"

#define WARNING_DISABLE_ALL						"-w"
#define WARNINGS_AS_ERRORS						"-Werror"


// actual warnings
#define WARNING_ENABLE_UNUSED_VARIABLE			"-Wunused-variable"
#define WARNING_ENABLE_VARIABLE_CHECKER			"-Wvar-checker"

#define WARNING_DISABLE_UNUSED_VARIABLE			"-Wno-unused-variable"
#define WARNING_DISABLE_VARIABLE_CHECKER		"-Wno-var-checker"



static std::vector<std::pair<std::string, std::string>> list;
static void setupMap()
{
	list.push_back({ ARG_COMPILE_ONLY, "Output an object file; do not call the linker" });
	list.push_back({ ARG_BACKEND + std::string(" <backend>"), "Change the backend used for compilation" });
	list.push_back({ ARG_EMIT_LLVM_IR, "Emit a bitcode (.bc) file instead of a program" });
	list.push_back({ ARG_LINK_FRAMEWORK + std::string(" <framework>"), "Link to a framework (macOS only)" });
	list.push_back({ ARG_LINK_FRAMEWORK + std::string(" <path>"), "Link to a framework (macOS only)" });
	list.push_back({ ARG_HELP, "Print this message" });
	list.push_back({ ARG_JITPROGRAM, "Use LLVM JIT to run the program, instead of compiling to a file" });
	list.push_back({ ARG_LINK_LIBRARY + std::string(" <library>"), "Link to a library" });
	list.push_back({ ARG_LIBRARY_SEARCH_PATH + std::string(" <path>"), "Search for libraries in <path>" });
	list.push_back({ ARG_MCMODEL + std::string(" <model>"), "Change the mcmodel of the code" });
	list.push_back({ ARG_DISABLE_AUTO_GLOBAL_CONSTRUCTORS, "Disable calling constructors before main()" });
	list.push_back({ ARG_OUTPUT_FILE + std::string(" <file>"), "Set the name of the output file" });
	list.push_back({ ARG_OPTIMISATION_LEVEL_SELECT + std::string("<level>"), "Change the optimisation level; -O0, -O1, -O2, -O3, and -Ox are valid options" });

	list.push_back({ ARG_POSINDEPENDENT, "Generate position independent code" });
	list.push_back({ ARG_PRINT_FIR, "Print the FlaxIR before compilation" });
	list.push_back({ ARG_PRINT_LLVMIR, "Print the LLVM IR before compilation" });
	list.push_back({ ARG_PROFILE, "Print internal compiler profiling statistics" });
	list.push_back({ ARG_RUNPROGRAM, "Use LLVM JIT to run the program, instead of compiling to a file" });
	list.push_back({ ARG_SHOW_CLANG_OUTPUT, "Show the output of calling the final compiler" });
	list.push_back({ ARG_SYSROOT + std::string(" <dir>"), "Set the directory used as the sysroot" });
	list.push_back({ ARG_TARGET + std::string(" <target>"), "Change the compilation target" });

	list.push_back({ WARNING_DISABLE_ALL, "Disable all warnings" });
	list.push_back({ WARNINGS_AS_ERRORS, "Treat all warnings as errors" });


	list.push_back({ WARNING_ENABLE_UNUSED_VARIABLE, "Enable warnings for unused variables" });
	list.push_back({ WARNING_ENABLE_VARIABLE_CHECKER, "Enable warnings from the variable state checker" });
	list.push_back({ WARNING_DISABLE_UNUSED_VARIABLE, "Disable warnings for unused variables" });
	list.push_back({ WARNING_DISABLE_VARIABLE_CHECKER, "Disable warnings from the variable state checker" });
}

static void printHelp()
{
	if(list.empty())
		setupMap();

	printf("Flax Compiler - Version %s\n\n", VERSION_STRING);
	printf("Usage: flaxc [options] <inputs>\n\n");

	printf("Options:\n");

	size_t maxl = 0;
	for(auto p : list)
	{
		if(p.first.length() > maxl)
			maxl = p.first.length();
	}

	maxl += 4;

	// ok
	for(auto p : list)
		printf("  %s%s%s\n", p.first.c_str(), std::string(maxl - p.first.length(), ' ').c_str(), p.second.c_str());

	printf("\n");
}








namespace Compiler
{
	static std::deque<std::string> librarySearchPaths;
	std::deque<std::string> getLibrarySearchPaths()
	{
		return librarySearchPaths;
	}

	static std::deque<std::string> librariesToLink;
	std::deque<std::string> getLibrariesToLink()
	{
		return librariesToLink;
	}

	static std::deque<std::string> frameworksToLink;
	std::deque<std::string> getFrameworksToLink()
	{
		return frameworksToLink;
	}

	static std::deque<std::string> frameworkSearchPaths;
	std::deque<std::string> getFrameworkSearchPaths()
	{
		return frameworkSearchPaths;
	}

	static bool doProfiler = false;
	bool showProfilerOutput()
	{
		return doProfiler;
	}

	static bool printLLVMIR = false;
	bool getDumpLlvm()
	{
		return printLLVMIR;
	}

	static bool printFIR = false;
	bool getDumpFir()
	{
		return printFIR;
	}

	#define DEFAULT_PREFIX		"/usr/local/lib/flaxlibs/"
	std::string getPrefix()
	{
		return DEFAULT_PREFIX;
	}

	static std::string sysroot;
	std::string getSysroot()
	{
		return sysroot;
	}

	static std::string target;
	std::string getTarget()
	{
		return target;
	}

	static uint64_t Flags;
	bool getFlag(Flag f)
	{
		return (Flags & (uint64_t) f);
	}

	static bool isPIC = false;
	bool getIsPositionIndependent()
	{
		return isPIC;
	}

	static std::string mcmodel;
	std::string getCodeModel()
	{
		return mcmodel;
	}

	static bool printClangOutput;
	bool getPrintClangOutput()
	{
		return printClangOutput;
	}

	static bool noAutoGlobalConstructor = false;
	bool getNoAutoGlobalConstructor()
	{
		return noAutoGlobalConstructor;
	}


	static ProgOutputMode outputMode = ProgOutputMode::Program;
	ProgOutputMode getOutputMode()
	{
		return outputMode;
	}

	static OptimisationLevel optLevel = OptimisationLevel::None;
	OptimisationLevel getOptimisationLevel()
	{
		return optLevel;
	}

	// llvm by default, duh
	static BackendOption backendOpt = BackendOption::LLVM;
	BackendOption getSelectedBackend()
	{
		return backendOpt;
	}




	static std::vector<Warning> enabledWarnings {
		// Warning::UnusedVariable,
		// Warning::UseBeforeAssign,
		// Warning::UseAfterFree
	};

	bool getWarningEnabled(Warning warning)
	{
		return std::find(enabledWarnings.begin(), enabledWarnings.end(), warning) != enabledWarnings.end();
	}

	static void setWarning(Warning warning, bool enabled)
	{
		auto it = std::find(enabledWarnings.begin(), enabledWarnings.end(), warning);
		if(it == enabledWarnings.end() && enabled == true)
			enabledWarnings.push_back(warning);

		else if(it != enabledWarnings.end() && enabled == false)
			enabledWarnings.erase(it);
	}

	std::pair<std::string, std::string> parseCmdLineArgs(int argc, char** argv)
	{
		// parse arguments
		std::deque<std::string> filenames;
		std::string outname;

		if(argc > 1)
		{
			// parse the command line opts
			for(int i = 1; i < argc; i++)
			{
				if(!strcmp(argv[i], ARG_HELP))
				{
					printHelp();
					exit(0);
				}
				else if(!strcmp(argv[i], ARG_LINK_FRAMEWORK))
				{
					if(i != argc - 1)
					{
						i++;
						frameworksToLink.push_back(argv[i]);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected framework name after '-framework' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_FRAMEWORK_SEARCH_PATH))
				{
					if(i != argc - 1)
					{
						i++;
						frameworkSearchPaths.push_back(argv[i]);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected path after '-F' option\n");
						exit(-1);
					}
				}
				else if(strstr(argv[i], ARG_LINK_LIBRARY) == argv[i])
				{
					// handles -lfoo

					if(strlen(argv[i]) > strlen(ARG_LINK_LIBRARY))
					{
						argv[i] += strlen(ARG_LINK_LIBRARY);
						librariesToLink.push_back(parseQuotedString(argv, i));
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected library name after '-l' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_LINK_LIBRARY))
				{
					// handles -l foo

					if(i != argc - 1)
					{
						i++;
						librariesToLink.push_back(argv[i]);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected library name after '-l' option\n");
						exit(-1);
					}
				}
				else if(strstr(argv[i], ARG_LIBRARY_SEARCH_PATH) == argv[i])
				{
					// handles -Lpath
					if(strlen(argv[i]) > strlen(ARG_LIBRARY_SEARCH_PATH))
					{
						argv[i] += strlen(ARG_LIBRARY_SEARCH_PATH);
						librarySearchPaths.push_back(parseQuotedString(argv, i));
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected path after '-L' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_LIBRARY_SEARCH_PATH))
				{
					// handles -L path

					if(i != argc - 1)
					{
						i++;
						librarySearchPaths.push_back(argv[i]);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected path after '-L' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_SYSROOT))
				{
					if(i != argc - 1)
					{
						i++;
						Compiler::sysroot = parseQuotedString(argv, i);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected directory name after '-sysroot' option\n");
						exit(-1);
					}
				}
				if(!strcmp(argv[i], ARG_TARGET))
				{
					if(i != argc - 1)
					{
						i++;
						Compiler::target = parseQuotedString(argv, i);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected target string after '-target' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_BACKEND))
				{
					if(i != argc - 1)
					{
						i++;
						auto str = parseQuotedString(argv, i);

						if(str == "llvm")
						{
							Compiler::backendOpt = BackendOption::LLVM;
						}
						else if(str == "x64asm")
						{
							Compiler::backendOpt = BackendOption::Assembly_x64;
						}
						else
						{
							fprintf(stderr, "Error: '%s' is not a valid backend (valid options are 'llvm' and 'x64asm')\n", str.c_str());
							exit(-1);
						}

						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected backend name after '-backend' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_OUTPUT_FILE))
				{
					if(i != argc - 1)
					{
						i++;
						outname = parseQuotedString(argv, i);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected filename name after '-o' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_POSINDEPENDENT))
				{
					Compiler::isPIC = true;
				}
				else if(!strcmp(argv[i], ARG_PROFILE))
				{
					Compiler::doProfiler = true;
				}
				else if(!strcmp(argv[i], ARG_MCMODEL))
				{
					if(i != argc - 1)
					{
						i++;
						std::string mm = parseQuotedString(argv, i);
						if(mm != "kernel" && mm != "small" && mm != "medium" && mm != "large")
						{
							fprintf(stderr, "Error: valid options for '-mcmodel' are 'small', 'medium', 'large' and 'kernel'.\n");
							exit(-1);
						}

						Compiler::mcmodel = mm;
					}
					else
					{
						fprintf(stderr, "Error: Expected mcmodel name after '-mcmodel' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], WARNINGS_AS_ERRORS))
				{
					Compiler::Flags |= (uint64_t) Compiler::Flag::WarningsAsErrors;
				}
				else if(!strcmp(argv[i], WARNING_DISABLE_ALL))
				{
					Compiler::Flags |= (uint64_t) Compiler::Flag::NoWarnings;
				}
				else if(!strcmp(argv[i], ARG_PRINT_LLVMIR))
				{
					Compiler::printLLVMIR = true;
				}
				else if(!strcmp(argv[i], ARG_PRINT_FIR))
				{
					Compiler::printFIR = true;
				}
				else if(!strcmp(argv[i], ARG_COMPILE_ONLY))
				{
					if(Compiler::outputMode != ProgOutputMode::RunJit && Compiler::outputMode != ProgOutputMode::LLVMBitcode)
					{
						Compiler::outputMode = ProgOutputMode::ObjectFile;
					}
					else
					{
						fprintf(stderr, "Error: Cannot use '-c' option simultaneously with either '-emit-llvm' or '-jit'\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_SHOW_CLANG_OUTPUT))
				{
					Compiler::printClangOutput = true;
				}
				else if(!strcmp(argv[i], ARG_JITPROGRAM) || !strcmp(argv[i], ARG_RUNPROGRAM))
				{
					if(Compiler::outputMode != ProgOutputMode::ObjectFile && Compiler::outputMode != ProgOutputMode::LLVMBitcode)
					{
						Compiler::outputMode = ProgOutputMode::RunJit;
					}
					else
					{
						fprintf(stderr, "Error: Cannot use '-jit'/'-run' option simultaneously with either '-emit-llvm' or '-c'\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_DISABLE_AUTO_GLOBAL_CONSTRUCTORS))
				{
					Compiler::noAutoGlobalConstructor = true;
				}
				else if(strstr(argv[i], ARG_OPTIMISATION_LEVEL_SELECT) == argv[i])
				{
					// make sure we have at least 3 chars
					if(strlen(argv[i]) < 3)
					{
						fprintf(stderr, "Error: '-O' is not a valid option on its own\n");
						exit(-1);
					}
					else if(strlen(argv[i]) > 3)
					{
						fprintf(stderr, "Error: '%s' is not a valid option\n", argv[i]);
						exit(-1);
					}

					if(argv[i][2] == 'x')
					{
						// literally nothing
						Compiler::optLevel = OptimisationLevel::Debug;
					}
					else
					{
						switch(argv[i][2])
						{
							case '0':	Compiler::optLevel = OptimisationLevel::None;		break;
							case '1':	Compiler::optLevel = OptimisationLevel::Minimal;	break;
							case '2':	Compiler::optLevel = OptimisationLevel::Normal;		break;
							case '3':	Compiler::optLevel = OptimisationLevel::Aggressive;	break;

							default:
								fprintf(stderr, "Error: '%c' is not a valid optimisation level (must be between 0 and 3)\n", argv[i][2]);
								exit(-1);
						}
					}
				}

				// warnings.
				else if(!strcmp(argv[i], WARNING_DISABLE_UNUSED_VARIABLE))
				{
					Compiler::setWarning(Compiler::Warning::UnusedVariable, false);
				}
				else if(!strcmp(argv[i], WARNING_ENABLE_UNUSED_VARIABLE))
				{
					Compiler::setWarning(Compiler::Warning::UnusedVariable, true);
				}

				else if(!strcmp(argv[i], WARNING_DISABLE_VARIABLE_CHECKER))
				{
					Compiler::setWarning(Compiler::Warning::UseAfterFree, false);
					Compiler::setWarning(Compiler::Warning::UseBeforeAssign, false);
				}
				else if(!strcmp(argv[i], WARNING_ENABLE_VARIABLE_CHECKER))
				{
					Compiler::setWarning(Compiler::Warning::UseAfterFree, true);
					Compiler::setWarning(Compiler::Warning::UseBeforeAssign, true);
				}
				else if(argv[i][0] == '-')
				{
					fprintf(stderr, "Error: Unrecognised option '%s'\n", argv[i]);

					exit(-1);
				}
				else
				{
					filenames.push_back(argv[i]);
					continue;
				}
			}
		}
		else
		{
			fprintf(stderr, "Expected at least one argument\n");
			exit(-1);
		}

		if(filenames.empty())
		{
			fprintf(stderr, "Error: no input files\n");
			exit(-1);
		}

		if(filenames.size() > 1)
		{
			fprintf(stderr, "Only one input file is supported at the moment\n");
			exit(-1);
		}

		return { filenames[0], outname };
	}
}


















