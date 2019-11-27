// backtrace.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <inttypes.h>

#include "errors.h"
#include "platform.h"

#if OS_DARWIN
#include <cxxabi.h>

// note: we declare these ourselves because there's some issue with execinfo.h on osx ):
extern "C" {
	int backtrace(void**, int);
	char** backtrace_symbols(void* const*, int);
}
#elif OS_UNIX
#include <cxxabi.h>
#include <execinfo.h>
#endif


namespace platform
{
	constexpr size_t MAX_FRAMES     = 128;
	constexpr size_t SKIP_FRAMES    = 1;

	struct piece_t
	{
		size_t num;
		uintptr_t address;
		size_t offset;

		std::string modName;
		std::string mangledName;
		std::string demangledName;
	};

	void printStackTrace()
	{
		#if OS_WINDOWS


		#else

		void* arr[MAX_FRAMES] { };
		size_t num = backtrace(arr, MAX_FRAMES);
		char** strs = backtrace_symbols(arr, num);

		std::vector<piece_t> pieces;

		for(size_t i = SKIP_FRAMES; i < num; i++)
		{
			// platform-specific output!
			if constexpr (OS_DARWIN)
			{
				piece_t piece;
				piece.num = i;

				char modname[1024] { };
				char funcname[1024] { };

				sscanf(strs[i], "%*s %s %zx %s %*s %zu", &modname[0], &piece.address, &funcname[0], &piece.offset);

				piece.mangledName = funcname;
				piece.modName = modname;

				if(piece.mangledName.find("_Z") == 0)
				{
					int status = -1;
					char* demangledName = abi::__cxa_demangle(piece.mangledName.c_str(), nullptr, nullptr, &status);
					if(status == 0)
					{
						std::string deman = demangledName;
						free(demangledName);

						// do replacements.
						std::map<std::string, std::string> replacements = {
							{ "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >", "std::string" }
						};

						for(const auto& [ from, to ] : replacements)
						{
							size_t it = -1;
							while((it = deman.find(from)) != std::string::npos)
								deman.replace(it, from.size(), to);
						}

						piece.demangledName = deman;
					}
					else
					{
						piece.demangledName = "??";
					}

					// skip some.
					if(piece.demangledName == "sst::Stmt::codegen(cgn::CodegenState*, fir::Type*)")
						continue;
				}
				else
				{
					piece.demangledName = piece.mangledName;
				}

				debuglogln("    %2d: %12x   |   %s + %#x", piece.num, piece.address, piece.demangledName, piece.offset);
			}
			else
			{
				// TODO.
			}
		}
		#endif
	}
}










