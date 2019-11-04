// platform.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

// platform-specific things

#pragma once

#include "defs.h"

namespace platform
{
	#define ALLOCATE_MEMORY_FUNC						"malloc"
	#define REALLOCATE_MEMORY_FUNC						"realloc"
	#define FREE_MEMORY_FUNC							"free"

	#if defined(_WIN32)
		#define OS_WINDOWS  1
		#define OS_DARWIN   0
		#define OS_UNIX     0
	#elif __APPLE__
		#define OS_WINDOWS  0
		#define OS_DARWIN   1
		#define OS_UNIX     0
	#elif __unix__
		#define OS_WINDOWS  0
		#define OS_DARWIN   0
		#define OS_UNIX     1
	#else
		#error "invalid host os!"
	#endif

	#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
		#define ARCH_64 1
		#define ARCH_32 0
	#else
		#define ARCH_32 1
		#define ARCH_64 0
	#endif




	#if OS_WINDOWS
		using filehandle_t = void*;

		#define CRT_FDOPEN			"_fdopen"
		#define PLATFORM_NEWLINE	"\r\n"

		#define PLATFORM_EXPORT_FUNCTION    extern "C" __declspec(dllexport)

		std::wstring convertStringToWChar(const std::string& s);
		std::string convertWCharToString(const std::wstring& s);
	#else
		#include <unistd.h>
		#include <sys/stat.h>

		using filehandle_t = int;

		#define CRT_FDOPEN			"fdopen"
		#define PLATFORM_NEWLINE	"\n"
		#define PLATFORM_EXPORT_FUNCTION    extern "C" __attribute__((visibility("default")))
	#endif

	extern filehandle_t InvalidFileHandle;

	#define REFCOUNT_SIZE		8

	filehandle_t openFile(const char* name, int mode, int flags);
	void closeFile(filehandle_t fd);

	size_t readFile(filehandle_t fd, void* buf, size_t count);
	size_t writeFile(filehandle_t fd, void* buf, size_t count);

	size_t getFileSize(const std::string& path);
	bool checkFileExists(const std::string& path);

	std::string_view readEntireFile(const std::string& path);
	void cachePreExistingFile(const std::string& path, const std::string& contents);

	std::string getFullPath(const std::string& partial);

	size_t getTerminalWidth();
	void setupTerminalIfNecessary();

	std::string getEnvironmentVar(const std::string& name);

	void pushEnvironmentVar(const std::string& name, const std::string& value);
	void popEnvironmentVar(const std::string& name);

	void printStackTrace();
	void setupCrashHandlers();

	namespace compiler
	{
		void performSelfDlOpen();
		void performSelfDlClose();

		void* getSymbol(const std::string& name);

		// call these as a pair
		void addLibrarySearchPaths();
		void restoreLibrarySearchPaths();

		std::vector<std::string> getDefaultSharedLibraries();

		std::string getExecutableName(const std::string& name);
		std::string getObjectFileName(const std::string& name);
		std::string getSharedLibraryName(const std::string& name);

		std::string getCompilerCommandLine(const std::vector<std::string>& inputObjects, const std::string& outputFilename);

		#if OS_WINDOWS
			std::string getWindowsSDKLocation();
			std::string getVSToolchainLibLocation();
			std::string getVSToolchainBinLocation();
		#endif
	}
}















