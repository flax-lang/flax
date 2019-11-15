// platform.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include <fcntl.h>
#include <stdlib.h>

#include "errors.h"
#include "frontend.h"
#include "platform.h"


#if OS_WINDOWS
	#define WIN32_LEAN_AND_MEAN 1

	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#include <windows.h>

	#define USE_MMAP false
#else
	#include <dlfcn.h>
	#include <unistd.h>
	#include <signal.h>
	#include <sys/mman.h>
	#include <sys/ioctl.h>

	#define USE_MMAP true

	#ifdef __MACH__
		#include <mach/vm_statistics.h>
		#define EXTRA_MMAP_FLAGS VM_FLAGS_SUPERPAGE_SIZE_2MB
	#elif defined(MAP_HUGE_2MB)
		#define EXTRA_MMAP_FLAGS MAP_HUGE_2MB
	#else
		#define EXTRA_MMAP_FLAGS 0
	#endif
#endif


namespace platform
{
	#if OS_WINDOWS
		filehandle_t InvalidFileHandle = INVALID_HANDLE_VALUE;
	#else
		filehandle_t InvalidFileHandle = -1;
	#endif


	static util::hash_map<std::string, std::vector<std::string>> environmentStack;

	std::string getEnvironmentVar(const std::string& name)
	{
	#if OS_WINDOWS
		char buffer[256] = { 0 };
		size_t len = 0;

		if(getenv_s(&len, buffer, name.c_str()) != 0)
			return "";

		else
			return std::string(buffer, len);
	#else
		if(char* val = getenv(name.c_str()); val)
			return std::string(val);

		else
			return "";
	#endif
	}

	void pushEnvironmentVar(const std::string& name, const std::string& value)
	{
		environmentStack[name].push_back(value);

		#if OS_WINDOWS
			_putenv_s(name.c_str(), value.c_str());
		#else
			setenv(name.c_str(), value.c_str(), /* overwrite: */ 1);
		#endif
	}

	void popEnvironmentVar(const std::string& name)
	{
		auto it = environmentStack.find(name);
		if(it == environmentStack.end() || it->second.empty())
			error("did not push '%s'", name.c_str());

		it->second.pop_back();

		auto restore = it->second.empty() ? "" : it->second.back();

		#if OS_WINDOWS
			_putenv_s(name.c_str(), restore.c_str());
		#else
			setenv(name.c_str(), restore.c_str(), /* overwrite: */ 1);
		#endif
	}

	#if OS_WINDOWS

	std::wstring convertStringToWChar(const std::string& s)
	{
		if(s.empty())
			return L"";

		if(s.size() > INT_MAX)
			error("string length %d is too large", s.size());

		int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), (int) s.size(), NULL, 0);
		if(required == 0) error("failed to convert string");


		auto buf = (LPWSTR) malloc(sizeof(WCHAR) * (required + 1));
		if(!buf) error("failed to allocate buffer");

		auto ret = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), (int) s.size(), buf, required);
		iceAssert(ret > 0);

		auto wstr = std::wstring(buf, ret);
		free(buf);

		return wstr;
	}

	std::string convertWCharToString(const std::wstring& s)
	{
		if(s.empty())
			return "";

		if(s.size() > INT_MAX)
			error("string length %d is too large", s.size());

		int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.c_str(), -1, NULL, 0, NULL, NULL);
		if(required == 0) error("failed to convert wstring");

		auto buf = (char*) malloc(sizeof(char) * (required + 1));
		if(!buf) error("failed to allocate buffer");

		auto ret = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.c_str(), -1, buf, required, NULL, NULL);
		iceAssert(ret > 0);

		auto str = std::string(buf, ret - 1);
		free(buf);

		return str;
	}

	#endif




	size_t getFileSize(const std::string& path)
	{
		#if OS_WINDOWS

			// note: jesus christ this thing is horrendous

			HANDLE hd = CreateFile((LPCSTR) path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(hd == INVALID_HANDLE_VALUE)
				error("failed to get filesize for '%s' (error code %d)", path, GetLastError());

			// ok, presumably it exists. so, get the size
			LARGE_INTEGER sz;
			bool success = GetFileSizeEx(hd, &sz);
			if(!success)
				error("failed to get filesize for '%s' (error code %d)", path, GetLastError());

			CloseHandle(hd);

			return (size_t) sz.QuadPart;

		#else

			struct stat st;
			if(stat(path.c_str(), &st) != 0)
				error("failed to get filesize for '%s' (error code %d / %s)", path, errno, strerror(errno));

			return st.st_size;

		#endif
	}


	static util::hash_map<std::string, std::string> cachedFileContents;

	void cachePreExistingFile(const std::string& path, const std::string& contents)
	{
		cachedFileContents[path] = contents;

		// this will give cache a new id for us. (over there)
		frontend::cachePreExistingFilename(path);
	}

	std::string_view readEntireFile(const std::string& path)
	{
		if(auto it = cachedFileContents.find(path); it != cachedFileContents.end())
			return it->second;

		// first, get the size of the file
		size_t fileLength = getFileSize(path);

		auto fd = openFile(path.c_str(), O_RDONLY, 0);
		if(fd == platform::InvalidFileHandle)
		{
			perror("there was an error getting opening the file");
			exit(-1);
		}


		// check if we should mmap
		// explanation: if we have EXTRA_MMAP_FLAGS, then we're getting 2MB pages -- in which case we should probably only do it
		// if we have at least 4mb worth of file.
		// if not, then just 2 * pagesize.
		#define MINIMUM_MMAP_THRESHOLD (static_cast<size_t>(EXTRA_MMAP_FLAGS ? (2 * 2 * 1024 * 1024) : 2 * getpagesize()))

		char* contents = 0;

		// here's the thing -- we use USE_MMAP at *compile-time*, because on windows some of the constants we're going to use here aren't available at all
		// if we include it, then it'll be parsed and everything and error out. So, we #ifdef it away.

		// Problem is, there's another scenario in which we won't want to use mmap -- when the file size is too small. so, that's why the stuff
		// below is structured the way it is.
		#if USE_MMAP
		{
			if(fileLength >= MINIMUM_MMAP_THRESHOLD)
			{
				// ok, do an mmap
				contents = static_cast<char*>(mmap(0, fileLength, PROT_READ, MAP_PRIVATE | EXTRA_MMAP_FLAGS, fd, 0));
				if(contents == reinterpret_cast<void*>(-1))
				{
					perror("there was an error reading the file");
					exit(-1);
				}
			}
		}
		#endif

		if(contents == 0)
		{
			// read normally
			//! MEMORY LEAK
			contents = new char[fileLength + 1];
			size_t didRead = platform::readFile(fd, contents, fileLength);
			if(didRead != fileLength)
			{
				perror("there was an error reading the file");
				error("expected %d bytes, but read only %d", fileLength, didRead);
			}

			cachedFileContents[path] = std::string(contents, fileLength);
		}

		iceAssert(contents);
		closeFile(fd);

		return cachedFileContents[path];
	}

	filehandle_t openFile(const char* name, int mode, int flags)
	{
		#if OS_WINDOWS
			bool writing = (mode & O_WRONLY) || (mode & O_RDWR);
			bool create = (mode & O_CREAT);

			HANDLE hd = CreateFile((LPCSTR) name, GENERIC_READ | (writing ? GENERIC_WRITE : 0), FILE_SHARE_READ, 0,
				create ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

			if(hd == INVALID_HANDLE_VALUE)
				return platform::InvalidFileHandle;

			return hd;
		#else
			return open(name, mode, flags);
		#endif
	}

	void closeFile(filehandle_t fd)
	{
		#if OS_WINDOWS
			CloseHandle(fd);
		#else
			close(fd);
		#endif
	}

	void deleteFile(filehandle_t fd)
	{
	}

	size_t readFile(filehandle_t fd, void* buf, size_t count)
	{
		#if OS_WINDOWS
			DWORD didRead = 0;
			bool success = ReadFile(fd, buf, (DWORD) count, &didRead, 0);
			if(!success)
				error("failed to read file (wanted %d bytes, read %d bytes); (error code %d)", count, didRead, GetLastError());

			return (size_t) didRead;
		#else
			return read(fd, buf, count);
		#endif
	}

	size_t writeFile(filehandle_t fd, void* buf, size_t count)
	{
		#if OS_WINDOWS
			DWORD didWrite = 0;
			bool success = WriteFile(fd, buf, (DWORD) count, &didWrite, 0);
			if(!success)
				error("failed to write file (wanted %d bytes, wrote %d bytes); (error code %d)", count, didWrite, GetLastError());

			return (size_t) didWrite;
		#else
			return write(fd, buf, count);
		#endif
	}



	bool checkFileExists(const std::string& path)
	{
		#if OS_WINDOWS
			TCHAR* p = (TCHAR*) path.c_str();
			DWORD dwAttrib = GetFileAttributes(p);
			return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
		#else
			struct stat st;
			return stat(path.c_str(), &st) == 0;
		#endif
	}




	std::string getFullPath(const std::string& partial)
	{
		#if OS_WINDOWS
		{
			// auto checkFileExists = [](const TCHAR* szPath) -> bool {
			// 	DWORD dwAttrib = GetFileAttributes(szPath);
			// 	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
			// };

			std::string p = partial;
			std::replace(p.begin(), p.end(), '/', '\\');


			HANDLE hd = CreateFile((LPCSTR) p.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(hd == INVALID_HANDLE_VALUE)
				return "";

			// ok, presumably it exists.
			defer(CloseHandle(hd));

			TCHAR* out = new TCHAR[MAX_PATH];
			defer(delete[] out);
			auto ret = GetFinalPathNameByHandleA(hd, out, MAX_PATH, VOLUME_NAME_DOS);

			if(ret != 0)
			{
				auto str = std::string(out);

				return str;
			}
			else
			{
				return "";
			}
		}
		#else
		{
			auto ret = realpath(partial.c_str(), 0);
			if(ret == 0) return "";

			auto str = std::string(ret);
			free(ret);

			return str;
		}
		#endif
	}

#ifdef _MSC_VER
#else
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

	size_t getTerminalWidth()
	{
		#if OS_WINDOWS
		{
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
			return csbi.srWindow.Right - csbi.srWindow.Left + 1;
		}
		#else
		{
			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			return w.ws_col;
		}
		#endif
	}

	void setupTerminalIfNecessary()
	{
		#if OS_WINDOWS

			// first, enable ansi colours
			std::vector<DWORD> handles = {
				STD_OUTPUT_HANDLE,
				STD_ERROR_HANDLE
			};

			for(auto x : handles)
			{
				auto h = GetStdHandle(x);
				SetConsoleMode(h, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
			}

			// then, change the codepage to utf-8:
			SetConsoleOutputCP(CP_UTF8);

		#else

		#endif
	}

	void setupCrashHandlers()
	{
		#if OS_WINDOWS

		#else
			signal(SIGSEGV, [](int) -> void {
				constexpr const char* msg = COLOUR_RED_BOLD "\n\ncompiler crash! " COLOUR_RESET "(segmentation fault)\n"
											COLOUR_BLUE_BOLD "stacktrace:\n" COLOUR_RESET;

				write(1, msg, strlen(msg));

				// note: this does not care about being re-entrant in signal handlers.
				// fuck that noise.
				printStackTrace();

				abort();
			});
		#endif
	}

#ifdef _MSC_VER
#else
	#pragma GCC diagnostic pop
#endif

}































