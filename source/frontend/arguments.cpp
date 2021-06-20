// frontend.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "frontend.h"
#include "backend.h"

#define FLAX_VERSION_STRING "0.41.7-pre"

#define ARG_COMPILE_ONLY                        "-c"
#define ARG_BACKEND                             "-backend"
#define ARG_EMIT_LLVM_IR                        "-emit-llvm"
#define ARG_LINK_FRAMEWORK                      "-framework"
#define ARG_FRAMEWORK_SEARCH_PATH               "-F"
#define ARG_HELP                                "--help"
#define ARG_VERSION                             "--version"
#define ARG_JITPROGRAM                          "-jit"
#define ARG_LINK_LIBRARY                        "-l"
#define ARG_LIBRARY_SEARCH_PATH                 "-L"
#define ARG_MCMODEL                             "-mcmodel"
#define ARG_OUTPUT_FILE                         "-o"
#define ARG_OPTIMISATION_LEVEL_SELECT           "-O"
#define ARG_POSINDEPENDENT                      "-pic"
#define ARG_PRINT_FIR                           "-print-fir"
#define ARG_PRINT_LLVMIR                        "-print-lir"
#define ARG_PROFILE                             "-profile"
#define ARG_RUNPROGRAM                          "-run"
#define ARG_SYSROOT                             "-sysroot"
#define ARG_TARGET                              "-target"
#define ARG_FFI_ESCAPE                          "--ffi-escape"
#define ARG_FREESTANDING                        "--freestanding"
#define ARG_NOSTDLIB                            "--nostdlib"
#define ARG_NO_RUNTIME_CHECKS                   "--no-runtime-checks"
#define ARG_NO_RUNTIME_ERROR_STRINGS            "--no-runtime-error-strings"
#define ARG_REPL                                "-repl"

// for internal use!
#define ARG_ABORT_ON_ERROR                      "-abort-on-error"

#define WARNING_DISABLE_ALL                     "-w"
#define WARNINGS_AS_ERRORS                      "-Werror"


// actual warnings
#define WARNING_ENABLE_UNUSED_VARIABLE          "-Wunused-variable"
#define WARNING_ENABLE_VARIABLE_CHECKER         "-Wvar-checker"

#define WARNING_DISABLE_UNUSED_VARIABLE         "-Wno-unused-variable"
#define WARNING_DISABLE_VARIABLE_CHECKER        "-Wno-var-checker"


static std::vector<std::pair<std::string, std::string>> helpList;
static void setupMap()
{
	helpList.push_back({ ARG_COMPILE_ONLY, "output an object file; do not call the linker" });
	helpList.push_back({ ARG_BACKEND + std::string(" <backend>"), "change the backend used for compilation" });
	helpList.push_back({ ARG_EMIT_LLVM_IR, "emit a bitcode (.bc) file instead of a program" });
	helpList.push_back({ ARG_LINK_FRAMEWORK + std::string(" <framework>"), "link to a framework (macOS only)" });
	helpList.push_back({ ARG_LINK_FRAMEWORK + std::string(" <path>"), "link to a framework (macOS only)" });
	helpList.push_back({ ARG_HELP, "print this message" });
	helpList.push_back({ ARG_JITPROGRAM, "use LLVM JIT to run the program, instead of compiling to a file" });
	helpList.push_back({ ARG_LINK_LIBRARY + std::string(" <library>"), "link to a library" });
	helpList.push_back({ ARG_LIBRARY_SEARCH_PATH + std::string(" <path>"), "search for libraries in <path>" });
	helpList.push_back({ ARG_MCMODEL + std::string(" <model>"), "change the mcmodel of the code" });
	helpList.push_back({ ARG_OUTPUT_FILE + std::string(" <file>"), "set the name of the output file" });
	helpList.push_back({ ARG_OPTIMISATION_LEVEL_SELECT + std::string("<level>"), "change the optimisation level; (-O[0-3], -Ox)" });

	helpList.push_back({ ARG_FREESTANDING, "generate a freestanding executable or object file" });
	helpList.push_back({ ARG_NOSTDLIB, "do not link with default libraries (libc/libm/msvcrt)" });
	helpList.push_back({ ARG_FFI_ESCAPE, "allow calling external functions (eg. libc) at compile-time" });

	helpList.push_back({ ARG_POSINDEPENDENT, "generate position independent code" });
	helpList.push_back({ ARG_PRINT_FIR, "print the FlaxIR before compilation" });
	helpList.push_back({ ARG_PRINT_LLVMIR, "print the LLVM IR before compilation" });
	helpList.push_back({ ARG_PROFILE, "print internal compiler profiling statistics" });
	helpList.push_back({ ARG_RUNPROGRAM, "run the program directly, instead of compiling to a file; defaults to the llvm backend" });
	helpList.push_back({ ARG_SYSROOT + std::string(" <dir>"), "set the directory used as the sysroot" });
	helpList.push_back({ ARG_TARGET + std::string(" <target>"), "change the compilation target" });
	helpList.push_back({ ARG_REPL, "start in repl mode" });

	helpList.push_back({ WARNING_DISABLE_ALL, "disable all warnings" });
	helpList.push_back({ WARNINGS_AS_ERRORS, "treat all warnings as errors" });

	helpList.push_back({ ARG_NO_RUNTIME_CHECKS, "disable all runtime checks" });
	helpList.push_back({ ARG_NO_RUNTIME_ERROR_STRINGS, "disable runtime error messages (program will just abort)" });

	helpList.push_back({ WARNING_ENABLE_UNUSED_VARIABLE, "enable warnings for unused variables" });
	helpList.push_back({ WARNING_ENABLE_VARIABLE_CHECKER, "enable warnings from the variable state checker" });
	helpList.push_back({ WARNING_DISABLE_UNUSED_VARIABLE, "disable warnings for unused variables" });
	helpList.push_back({ WARNING_DISABLE_VARIABLE_CHECKER, "disable warnings from the variable state checker" });
}

static void printHelp()
{
	if(helpList.empty())
		setupMap();

	printf("Flax Compiler - Version %s\n\n", FLAX_VERSION_STRING);
	printf("usage: flaxc [options] <inputs>\n\n");

	printf("options:\n");

	size_t maxl = 0;
	for(const auto& p : helpList)
	{
		if(p.first.length() > maxl)
			maxl = p.first.length();
	}

	maxl += 4;

	// ok
	for(const auto& [ opt, desc ] : helpList)
		printf("  %s%s%s\n", opt.c_str(), std::string(maxl - opt.length(), ' ').c_str(), desc.c_str());

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
	std::string getVersion()
	{
		return FLAX_VERSION_STRING;
	}

	static std::vector<std::string> frameworksToLink;
	static std::vector<std::string> frameworkSearchPaths;

	static std::vector<std::string> librariesToLink;
	static std::vector<std::string> librarySearchPaths;


	static bool _isPIC = false;
	static bool _isRepl = false;
	static bool _printFIR = false;
	static bool _ffiEscape = false;
	static bool _doProfiler = false;
	static bool _printLLVMIR = false;
	static bool _abortOnError = false;
	static bool _isFreestanding = false;
	static bool _noStandardLibraries = false;

	static bool _noRuntimeChecks = false;
	static bool _noRuntimeErrorStrings = false;

	static std::string _mcModel;
	static std::string _targetArch;
	static std::string _sysrootPath;
	static const std::string _prefixPath = "/usr/local/";

	using BackendOption = backend::BackendOption;
	using ProgOutputMode = backend::ProgOutputMode;
	using OptimisationLevel = backend::OptimisationLevel;

	static auto _optLevel = OptimisationLevel::Normal;
	static auto _outputMode = ProgOutputMode::Program;
	static auto _backendCodegen = BackendOption::LLVM;

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

	bool getPrintProfileStats()
	{
		return _doProfiler;
	}

	bool getIsNoRuntimeChecks()
	{
		return _noRuntimeChecks;
	}

	bool getIsNoRuntimeErrorStrings()
	{
		return _noRuntimeErrorStrings;
	}

	bool getAbortOnError()
	{
		return _abortOnError;
	}

	bool getCanFFIEscape()
	{
		return _ffiEscape;
	}

	bool getIsReplMode()
	{
		return _isRepl;
	}

	bool getPrintFIR()
	{
		return _printFIR;
	}

	bool getPrintLLVMIR()
	{
		return _printLLVMIR;
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

	bool getIsNoStandardLibraries()
	{
		return _noStandardLibraries;
	}

	bool getIsPositionIndependent()
	{
		return _isPIC;
	}


	std::string getParameter(const std::string& name)
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



	// nothing to do with synchronisation!!
	// TODO: this is a bit dumb given our current boolean flags, but... meh
	static std::unordered_set<std::string> setOptions;
	static util::hash_map<std::string, std::unordered_set<std::string>> mutualExclusions;
	static void setupFlagMutexes()
	{
		// repl is basically incompatible with everything, so list it first.
		mutualExclusions[ARG_REPL].insert(ARG_RUNPROGRAM);
		mutualExclusions[ARG_REPL].insert(ARG_TARGET);
		mutualExclusions[ARG_REPL].insert(ARG_BACKEND);
		mutualExclusions[ARG_REPL].insert(ARG_MCMODEL);
		mutualExclusions[ARG_REPL].insert(ARG_JITPROGRAM);
		mutualExclusions[ARG_REPL].insert(ARG_OUTPUT_FILE);
		mutualExclusions[ARG_REPL].insert(ARG_COMPILE_ONLY);
		mutualExclusions[ARG_REPL].insert(ARG_FREESTANDING);
		mutualExclusions[ARG_REPL].insert(ARG_POSINDEPENDENT);
		mutualExclusions[ARG_REPL].insert(ARG_OPTIMISATION_LEVEL_SELECT);

		// don't try to run/jit and compile/output at the same time
		mutualExclusions[ARG_RUNPROGRAM].insert(ARG_TARGET);
		mutualExclusions[ARG_RUNPROGRAM].insert(ARG_MCMODEL);
		mutualExclusions[ARG_RUNPROGRAM].insert(ARG_OUTPUT_FILE);
		mutualExclusions[ARG_RUNPROGRAM].insert(ARG_COMPILE_ONLY);
		mutualExclusions[ARG_RUNPROGRAM].insert(ARG_FREESTANDING);
		mutualExclusions[ARG_RUNPROGRAM].insert(ARG_POSINDEPENDENT);

		// just copy it.
		mutualExclusions[ARG_JITPROGRAM] = mutualExclusions[ARG_RUNPROGRAM];

		// ok now the trick is, for each exclusion, add the reverse.
		for(const auto& [ x, xs ] : mutualExclusions)
			for(const auto& y : xs)
				mutualExclusions[y].insert(x);
	}

	static void checkOptionExclusivity(const std::string& a)
	{
		for(const auto& ex : mutualExclusions[a])
		{
			if(setOptions.find(ex) != setOptions.end())
				_error_and_exit("error: options '%s' and '%s' are mutually exclusive", a, ex);
		}
	}








	std::pair<std::string, std::string> parseCmdLineOpts(int argc, char** argv)
	{
		setupFlagMutexes();

		// quick thing: usually programs will not do anything if --help or --version is anywhere in the flags.
		for(int i = 1; i < argc; i++)
		{
			if(!strcmp(argv[i], ARG_HELP))
			{
				printHelp();
				exit(0);
			}
			else if(!strcmp(argv[i], ARG_VERSION))
			{
				printf("Flax Compiler (flaxc), version %s\n", FLAX_VERSION_STRING);
				exit(0);
			}
		}

		// parse arguments
		std::vector<std::string> filenames;
		std::string outname;

		if(argc > 1)
		{
			// parse the command line opts
			for(int i = 1; i < argc; i++)
			{
				bool wasFilename = false;
				if(!strcmp(argv[i], ARG_LINK_FRAMEWORK))
				{
					if(i != argc - 1)
					{
						i++;
						frameworksToLink.push_back(argv[i]);
						continue;
					}
					else
					{
						_error_and_exit("error: expected framework name after '-framework' option\n");
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
						_error_and_exit("error: expected path after '-F' option\n");
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
						_error_and_exit("error: expected library name after '-l' option\n");
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
						_error_and_exit("error: expected library name after '-l' option\n");
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
						_error_and_exit("error: expected path after '-L' option\n");
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
						_error_and_exit("error: expected path after '-L' option\n");
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
						_error_and_exit("error: expected directory name after '-sysroot' option\n");
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
						_error_and_exit("error: expected target string after '-target' option\n");
					}
				}
				else if(!strcmp(argv[i], ARG_FREESTANDING))
				{
					// set freestanding mode
					frontend::_isFreestanding = true;
				}
				else if(!strcmp(argv[i], ARG_NO_RUNTIME_CHECKS))
				{
					frontend::_noRuntimeChecks = true;
				}
				else if(!strcmp(argv[i], ARG_NO_RUNTIME_ERROR_STRINGS))
				{
					frontend::_noRuntimeErrorStrings = true;
				}
				else if(!strcmp(argv[i], ARG_FFI_ESCAPE))
				{
					frontend::_ffiEscape = true;
				}
				else if(!strcmp(argv[i], ARG_REPL))
				{
					frontend::_isRepl = true;
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
						else if(str == "interp")
						{
							frontend::_backendCodegen = BackendOption::Interpreter;
						}
						else if(str == "x64asm")
						{
							frontend::_backendCodegen = BackendOption::Assembly_x64;
						}
						else if(str == "none")
						{
							frontend::_backendCodegen = BackendOption::None;
						}
						else
						{
							_error_and_exit("error: '%s' is not a valid backend (valid options are 'llvm' and 'x64asm')\n", str);
						}

						continue;
					}
					else
					{
						_error_and_exit("error: expected backend name after '-backend' option\n");
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
						_error_and_exit("error: expected filename name after '-o' option\n");
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
							_error_and_exit("error: valid options for '-mcmodel' are 'small', 'medium', 'large' and 'kernel'.\n");
						}

						frontend::_mcModel = mm;
					}
					else
					{
						_error_and_exit("error: expected mcmodel name after '-mcmodel' option\n");
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
				}
				else if(!strcmp(argv[i], ARG_JITPROGRAM) || !strcmp(argv[i], ARG_RUNPROGRAM))
				{
					if(frontend::_outputMode != ProgOutputMode::ObjectFile && frontend::_outputMode != ProgOutputMode::LLVMBitcode)
					{
						frontend::_outputMode = ProgOutputMode::RunJit;
					}
				}
				else if(strstr(argv[i], ARG_OPTIMISATION_LEVEL_SELECT) == argv[i])
				{
					// make sure we have at least 3 chars
					if(strlen(argv[i]) < 3)
					{
						_error_and_exit("error: '-O' is not a valid option on its own\n");
					}
					else if(strlen(argv[i]) > 3)
					{
						_error_and_exit("error: '%s' is not a valid option\n", argv[i]);
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
							case '0':   frontend::_optLevel = OptimisationLevel::None;      break;
							case '1':   frontend::_optLevel = OptimisationLevel::Minimal;   break;
							case '2':   frontend::_optLevel = OptimisationLevel::Normal;    break;
							case '3':   frontend::_optLevel = OptimisationLevel::Aggressive break;

							default:
								_error_and_exit("error: '%c' is not a valid optimisation level (must be between 0 and 3)\n", argv[i][2]);
						}
					}
				}
				else if(!strcmp(argv[i], ARG_ABORT_ON_ERROR))
				{
					frontend::_abortOnError = true;
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
					_error_and_exit("error: unrecognised option '%s'\n", argv[i]);
				}
				else
				{
					wasFilename = true;
					filenames.push_back(argv[i]);
				}


				if(!wasFilename)
				{
					setOptions.insert(argv[i]);
					checkOptionExclusivity(argv[i]);
				}
			}
		}



		if(filenames.empty() && !frontend::_isRepl)
			_error_and_exit("error: no input files\n");

		if(filenames.size() > 1)
			_error_and_exit("only one input file is supported at the moment\n");

		return { filenames.empty() ? "" : filenames[0], outname };
	}
}










