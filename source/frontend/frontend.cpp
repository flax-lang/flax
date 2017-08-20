// frontend.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "frontend.h"
#include "backend.h"

#include <vector>


#define VERSION_STRING	"0.30.0-pre"

#define ARG_COMPILE_ONLY						"-c"
#define ARG_BACKEND								"-backend"
#define ARG_EMIT_LLVM_IR						"-emit-llvm"
#define ARG_LINK_FRAMEWORK						"-framework"
#define ARG_FRAMEWORK_SEARCH_PATH				"-F"
#define ARG_HELP								"-help"
#define ARG_VERSION								"-version"
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
#define ARG_FREESTANDING						"-ffreestanding"

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













namespace frontend
{
	std::vector<std::string> frameworksToLink;
	std::vector<std::string> frameworkSearchPaths;

	std::vector<std::string> librariesToLink;
	std::vector<std::string> librarySearchPaths;

	bool _isPIC = false;
	bool _printFIR = false;
	bool _doProfiler = false;
	bool _printLLVMIR = false;
	bool _isFreestanding = false;
	bool _printClangOutput = false;
	bool _noAutoGlobalConstructor = false;

	std::string _mcModel;
	std::string _targetArch;
	std::string _sysrootPath;
	const std::string _prefixPath = "/usr/local/";

	using ProgOutputMode	= backend::ProgOutputMode;
	using BackendOption		= backend::BackendOption;
	using OptimisationLevel	= backend::OptimisationLevel;

	auto _outputMode		= ProgOutputMode::RunJit;
	auto _backendCodegen	= BackendOption::LLVM;
	auto _optLevel			= OptimisationLevel::Normal;

	OptimisationLevel getOptLevel()
	{
		return _optLevel;
	}

	ProgOutputMode getOutputMode()
	{
		return _outputMode;
	}

	BackendOption getBackendOption()
	{
		return _backendCodegen;
	}


	std::vector<std::string> getFrameworksToLink()
	{
		return frameworksToLink;
	}
	std::vector<std::string> getFrameworkSearchPaths()
	{
		return frameworkSearchPaths;
	}

	std::vector<std::string> getLibrariesToLink()
	{
		return librariesToLink;
	}

	std::vector<std::string> getLibrarySearchPaths()
	{
		return librarySearchPaths;
	}

	bool getIsFreestanding()
	{
		return _isFreestanding;
	}

	bool getIsPositionIndependent()
	{
		return _isPIC;
	}


	std::string getParameter(std::string name)
	{
		if(name == "mcmodel")
			return _mcModel;

		else if(name == "targetarch")
			return _targetArch;

		else if(name == "sysroot")
			return _sysrootPath;

		else if(name == "prefix")
			return _prefixPath;

		else
			iceAssert("invalid");

		return "";
	}


	std::pair<std::string, std::string> parseCmdLineOpts(int argc, char** argv)
	{
		// parse arguments
		std::vector<std::string> filenames;
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
				else if(!strcmp(argv[i], ARG_VERSION))
				{
					printf("Flax Compiler (flaxc), version %s\n", VERSION_STRING);
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
						frontend::_sysrootPath = parseQuotedString(argv, i);
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
						frontend::_targetArch = parseQuotedString(argv, i);
						continue;
					}
					else
					{
						fprintf(stderr, "Error: Expected target string after '-target' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_FREESTANDING))
				{
					// set freestanding mode
					frontend::_isFreestanding = true;
				}
				else if(!strcmp(argv[i], ARG_BACKEND))
				{
					if(i != argc - 1)
					{
						i++;
						auto str = parseQuotedString(argv, i);

						if(str == "llvm")
						{
							frontend::_backendCodegen = BackendOption::LLVM;
						}
						else if(str == "x64asm")
						{
							frontend::_backendCodegen = BackendOption::Assembly_x64;
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
					frontend::_isPIC = true;
				}
				else if(!strcmp(argv[i], ARG_PROFILE))
				{
					frontend::_doProfiler = true;
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

						frontend::_mcModel = mm;
					}
					else
					{
						fprintf(stderr, "Error: Expected mcmodel name after '-mcmodel' option\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], WARNINGS_AS_ERRORS))
				{
					// frontend::Flags |= (uint64_t) frontend::Flag::WarningsAsErrors;
				}
				else if(!strcmp(argv[i], WARNING_DISABLE_ALL))
				{
					// frontend::Flags |= (uint64_t) frontend::Flag::NoWarnings;
				}
				else if(!strcmp(argv[i], ARG_PRINT_LLVMIR))
				{
					frontend::_printLLVMIR = true;
				}
				else if(!strcmp(argv[i], ARG_PRINT_FIR))
				{
					frontend::_printFIR = true;
				}
				else if(!strcmp(argv[i], ARG_COMPILE_ONLY))
				{
					if(frontend::_outputMode != ProgOutputMode::RunJit && frontend::_outputMode != ProgOutputMode::LLVMBitcode)
					{
						frontend::_outputMode = ProgOutputMode::ObjectFile;
					}
					else
					{
						fprintf(stderr, "Error: Cannot use '-c' option simultaneously with either '-emit-llvm' or '-jit'\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_SHOW_CLANG_OUTPUT))
				{
					frontend::_printClangOutput = true;
				}
				else if(!strcmp(argv[i], ARG_JITPROGRAM) || !strcmp(argv[i], ARG_RUNPROGRAM))
				{
					if(frontend::_outputMode != ProgOutputMode::ObjectFile && frontend::_outputMode != ProgOutputMode::LLVMBitcode)
					{
						frontend::_outputMode = ProgOutputMode::RunJit;
					}
					else
					{
						fprintf(stderr, "Error: Cannot use '-jit'/'-run' option simultaneously with either '-emit-llvm' or '-c'\n");
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_DISABLE_AUTO_GLOBAL_CONSTRUCTORS))
				{
					frontend::_noAutoGlobalConstructor = true;
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
						frontend::_optLevel = OptimisationLevel::Debug;
					}
					else
					{
						switch(argv[i][2])
						{
							case '0':	frontend::_optLevel = OptimisationLevel::None;			break;
							case '1':	frontend::_optLevel = OptimisationLevel::Minimal;		break;
							case '2':	frontend::_optLevel = OptimisationLevel::Normal;		break;
							case '3':	frontend::_optLevel = OptimisationLevel::Aggressive;	break;

							default:
								fprintf(stderr, "Error: '%c' is not a valid optimisation level (must be between 0 and 3)\n", argv[i][2]);
								exit(-1);
						}
					}
				}

				// warnings.
				else if(!strcmp(argv[i], WARNING_DISABLE_UNUSED_VARIABLE))
				{
					// frontend::setWarning(frontend::Warning::UnusedVariable, false);
				}
				else if(!strcmp(argv[i], WARNING_ENABLE_UNUSED_VARIABLE))
				{
					// frontend::setWarning(frontend::Warning::UnusedVariable, true);
				}

				else if(!strcmp(argv[i], WARNING_DISABLE_VARIABLE_CHECKER))
				{
					// frontend::setWarning(frontend::Warning::UseAfterFree, false);
					// frontend::setWarning(frontend::Warning::UseBeforeAssign, false);
				}
				else if(!strcmp(argv[i], WARNING_ENABLE_VARIABLE_CHECKER))
				{
					// frontend::setWarning(frontend::Warning::UseAfterFree, true);
					// frontend::setWarning(frontend::Warning::UseBeforeAssign, true);
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


		// sanity check
		// what are you even trying to do m8
		if(frontend::_outputMode == ProgOutputMode::RunJit && frontend::_isFreestanding)
			fprintf(stderr, "Cannot JIT program in freestanding mode");

		return { filenames[0], outname };
	}
}












