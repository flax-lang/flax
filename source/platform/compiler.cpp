// compiler.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.


#include <fcntl.h>

#include "errors.h"
#include "frontend.h"

#if OS_WINDOWS
	#define WIN32_LEAN_AND_MEAN 1

	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#include <windows.h>
#else
	#include <dlfcn.h>
	#include <unistd.h>
	#include <sys/mman.h>
	#include <sys/ioctl.h>
#endif


namespace platform {
namespace compiler
{
	#if OS_WINDOWS
		static HMODULE currentModule = 0;
		static std::vector<HMODULE> otherModules;
		static std::vector<DLL_DIRECTORY_COOKIE> loadedDllDirs;
	#else
		void* currentModule = 0;
	#endif


	std::string getCompilerCommandLine(const std::vector<std::string>& inputObjects, const std::string& outputFilename)
	{
		std::string cmdline;
		#if OS_WINDOWS

			// TODO: get set the target properly!!
			// TODO: get set the target properly!!
			// TODO: get set the target properly!!

			cmdline = strprintf("%s\\x64\\link.exe /nologo /incremental:no /out:%s /nodefaultlib",
				getVSToolchainBinLocation(), outputFilename);

			cmdline += strprintf(" /machine:AMD64");

			for(const auto& i : inputObjects)
				cmdline += strprintf(" %s", i);

			if(!frontend::getIsFreestanding() && !frontend::getIsNoStandardLibraries())
			{
				// these dumb paths have spaces in them, so we need to quote it.
				// link.exe handles its own de-quoting, not cmd.exe or whatever shell.
				auto sdkRoot = strprintf("\"%s\"", getWindowsSDKLocation());
				auto vsLibRoot = strprintf("\"%s\"", getVSToolchainLibLocation());

				std::vector<std::string> umLibs     = { "kernel32.lib" };
				std::vector<std::string> ucrtLibs   = { "libucrt.lib" };
				std::vector<std::string> msvcLibs   = {
					"libcmt.lib", "libvcruntime.lib",
					"legacy_stdio_definitions.lib", "legacy_stdio_wide_specifiers.lib"
				};

				for(const auto& l : umLibs)
					cmdline += strprintf(" %s\\um\\x64\\%s", sdkRoot, l);

				for(const auto& l : ucrtLibs)
					cmdline += strprintf(" %s\\ucrt\\x64\\%s", sdkRoot, l);

				for(const auto& l : msvcLibs)
					cmdline += strprintf(" %s\\x64\\%s", vsLibRoot, l);
			}

		#else

			// cc -o <output>
			cmdline = strprintf("cc -o %s", outputFilename);

			for(const auto& i : inputObjects)
				cmdline += strprintf(" %s", i);

			for(const auto& p : frontend::getLibrarySearchPaths())
				cmdline += strprintf(" -L%s", p);

			for(const auto& p : frontend::getFrameworkSearchPaths())
				cmdline += strprintf(" -F%s", p);

			for(const auto& l : frontend::getLibrariesToLink())
				cmdline += strprintf(" -l%s", l);

			for(const auto& f : frontend::getFrameworksToLink())
				cmdline += strprintf(" -framework %s", f);

			if(!frontend::getIsFreestanding() && !frontend::getIsNoStandardLibraries())
				cmdline += strprintf(" -lm -lc");

		#endif

		return cmdline;
	}

























	void addLibrarySearchPaths()
	{
		auto libPaths = frontend::getLibrarySearchPaths();
		auto frameworkPaths = frontend::getFrameworkSearchPaths();

		#if OS_WINDOWS
			for(const auto& path : libPaths)
			{
				if(path.empty()) continue;

				auto wpath = convertStringToWChar(path);
				auto cookie = AddDllDirectory(wpath.c_str());

				if(cookie)
					loadedDllDirs.push_back(cookie);

			}
		#else
			auto env = getEnvironmentVar("LD_LIBRARY_PATH");
			for(auto L : libPaths)
				env += strprintf(":%s", L);

			pushEnvironmentVar("LD_LIBRARY_PATH", env);

			#if OS_DARWIN
			{
				auto env = getEnvironmentVar("DYLD_FRAMEWORK_PATH");
				for(auto L : frameworkPaths)
					env += strprintf(":%s", L);

				pushEnvironmentVar("DYLD_FRAMEWORK_PATH", env);
			}
			#endif
		#endif
	}

	void restoreLibrarySearchPaths()
	{
		#if OS_WINDOWS
			for(auto cookie : loadedDllDirs)
				RemoveDllDirectory(cookie);
		#else
			popEnvironmentVar("LD_LIBRARY_PATH");

			#if OS_DARWIN
				popEnvironmentVar("DYLD_FRAMEWORK_PATH");
			#endif
		#endif
	}

	std::vector<std::string> getDefaultSharedLibraries()
	{
		if(frontend::getIsFreestanding() || frontend::getIsNoStandardLibraries())
			return { };

		#if OS_WINDOWS

			//? the name is "vcruntime140.dll", which is apparently specific to MSVC 14.0+, apparently 140 is the only number that
			//? appears to be referenced in online sources.
			return {
				"ucrtbase.dll",
				"vcruntime140.dll"
			};
		#elif OS_DARWIN
			return {
				"libc.dylib", "libm.dylib"
			};
		#elif OS_UNIX
			// note: we do not (and cannot) link libc and libm explicitly under linux.
			// not sure about other unices.
			return {
			};
		#endif
	}

	std::string getSharedLibraryName(const std::string& name)
	{
		#if OS_WINDOWS
			return strprintf("%s.dll", name);
		#elif OS_DARWIN
			return strprintf("lib%s.dylib", name);
		#elif OS_UNIX
			return strprintf("lib%s.so", name);
		#endif
	}

	std::string getExecutableName(const std::string& name)
	{
		#if OS_WINDOWS
			return strprintf("%s.exe", name);
		#else
			return name;
		#endif
	}

	std::string getObjectFileName(const std::string& name)
	{
		#if OS_WINDOWS
			return strprintf("%s.obj", name);
		#else
			return strprintf("%s.o", name);
		#endif
	}















	void performSelfDlOpen()
	{
		#if OS_WINDOWS
			currentModule = GetModuleHandle(nullptr);
		#else
			currentModule = dlopen(nullptr, RTLD_LAZY);
		#endif
	}

	void performSelfDlClose()
	{
		#if OS_WINDOWS
			for(auto mod : otherModules)
				FreeLibrary(mod);
		#endif
	}

	void* getSymbol(const std::string& name)
	{
		if(!currentModule) error("backend: failed to load current module!");

		void* ret = 0;
		#if OS_WINDOWS
			ret = GetProcAddress(currentModule, name.c_str());
			for(size_t i = 0; !ret && i < otherModules.size(); i++)
				ret = GetProcAddress(otherModules[i], name.c_str());
		#else
			ret = dlsym(currentModule, name.c_str());
		#endif

		return ret;
	}

}
}

























