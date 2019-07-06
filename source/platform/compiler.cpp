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

	void addLibrarySearchPaths(const std::vector<std::string>& libPaths, const std::vector<std::string>& frameworkPaths)
	{
		#if OS_WINDOWS
			for(const auto& path : libPaths)
			{
				if(path.empty()) continue;

				auto wpath = convertStringToWChar(path);
				iceAssert(wpath);

				auto cookie = AddDllDirectory((LPWSTR) wpath);
				if(cookie)
					loadedDllDirs.push_back(cookie);

				free(wpath);
			}
		#else
			auto env = getEnvironmentVar("LD_LIBRARY_PATH");
			for(auto L : libPaths)
				env += strprintf(":%s", L);

			pushEnvironmentVar("LD_LIBRARY_PATH", env);

			#if OS_DARWIN
				auto env = getEnvironmentVar("DYLD_FRAMEWORK_PATH");
				for(auto L : frameworkPaths)
					env += strprintf(":%s", L);

				pushEnvironmentVar("DYLD_FRAMEWORK_PATH", env);
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

	std::vector<std::string> getDefaultLibraries()
	{
		#if OS_WINDOWS

			//? the name is "vcruntime140.dll", which is apparently specific to MSVC 14.0+, apparently 140 is the only number that
			//? appears to be referenced in online sources.
			return {
				"ucrtbase",
				"vcruntime140"
			};
		#elif OS_DARWIN
			return {
				"c", "m"
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

























