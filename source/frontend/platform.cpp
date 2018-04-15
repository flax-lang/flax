// platform.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <fcntl.h>

#include "errors.h"
#include "frontend.h"

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/mman.h>

	#define USE_MMAP true

	#ifdef __MACH__
		#include <mach/vm_statistics.h>
		#define EXTRA_MMAP_FLAGS VM_FLAGS_SUPERPAGE_SIZE_2MB
	#elif defined(MAP_HUGE_2MB)
		#define EXTRA_MMAP_FLAGS MAP_HUGE_2MB
	#else
		#define EXTRA_MMAP_FLAGS 0
	#endif
#else
	#define USE_MMAP false
#endif


namespace platform
{
	#ifdef _WIN32
		filehandle_t InvalidFileHandle = INVALID_HANDLE_VALUE;
	#else
		filehandle_t InvalidFileHandle = -1;
	#endif

	size_t getFileSize(const std::string& path)
	{
		#ifdef _WIN32

			// note: jesus christ this thing is horrendous

			HANDLE hd = CreateFile((LPCSTR) path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(hd == INVALID_HANDLE_VALUE)
				_error_and_exit("Failed to get filesize for '%s' (error code %d)", path, GetLastError());

			// ok, presumably it exists. so, get the size
			LARGE_INTEGER sz;
			bool success = GetFileSizeEx(hd, &sz);
			if(!success)
				_error_and_exit("Failed to get filesize for '%s' (error code %d)", path, GetLastError());

			CloseHandle(hd);

			return (size_t) sz.QuadPart;

		#else

			struct stat st;
			if(stat(path.c_str(), &st) != 0)
				_error_and_exit("Failed to get filesize for '%s' (error code %d / %s)", path, errno, strerror(errno));

			return st.st_size;

		#endif
	}

	util::string_view readEntireFile(const std::string& path)
	{
		// first, get the size of the file
		size_t fileLength = getFileSize(path);

		auto fd = openFile(path.c_str(), O_RDONLY, 0);
		if(fd == platform::InvalidFileHandle)
		{
			perror("There was an error getting opening the file");
			exit(-1);
		}


		// check if we should mmap
		// explanation: if we have EXTRA_MMAP_FLAGS, then we're getting 2MB pages -- in which case we should probably only do it
		// if we have at least 4mb worth of file.
		// if not, then just 2 * pagesize.
		#define MINIMUM_MMAP_THRESHOLD (EXTRA_MMAP_FLAGS ? (2 * 2 * 1024 * 1024) : 2 * getpagesize())
		#define _

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
				contents = (char*) mmap(0, fileLength, PROT_READ, MAP_PRIVATE | EXTRA_MMAP_FLAGS, fd, 0);
				if(contents == MAP_FAILED)
				{
					perror("There was an error reading the file");
					exit(-1);
				}
			}
		}
		#endif

		if(contents == 0)
		{
			// read normally
			contents = new char[fileLength + 1];
			size_t didRead = platform::readFile(fd, contents, fileLength);
			if(didRead != fileLength)
			{
				perror("There was an error reading the file");
				_error_and_exit("Expected %d bytes, but read only %d\n", fileLength, didRead);
				exit(-1);
			}
		}

		iceAssert(contents);
		closeFile(fd);

		return util::string_view(contents, fileLength);
	}

	filehandle_t openFile(const char* name, int mode, int flags)
	{
		#ifdef _WIN32
			bool writing = (mode & O_WRONLY) || (mode & O_RDWR);
			bool create = (mode & O_CREAT);

			HANDLE hd = CreateFile((LPCSTR) name, GENERIC_READ | (writing ? GENERIC_WRITE : 0), FILE_SHARE_READ, 0,
				create ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

			if(hd == INVALID_HANDLE_VALUE)
				return platform::InvalidFileHandle;
				// _error_and_exit("Failed to open file '%s' (error code %d)", name, GetLastError());

			return hd;
		#else
			return open(name, mode, flags);
		#endif
	}

	void closeFile(filehandle_t fd)
	{
		#ifdef _WIN32
			CloseHandle(fd);
		#else
			close(fd);
		#endif
	}

	size_t readFile(filehandle_t fd, void* buf, size_t count)
	{
		#ifdef _WIN32
			DWORD didRead = 0;
			bool success = ReadFile(fd, buf, (platform::DWORD) count, &didRead, 0);
			if(!success)
				_error_and_exit("Failed to read file (wanted %d bytes, read %d bytes); (error code %d)", count, didRead, GetLastError());

			return (size_t) didRead;
		#else
			return read(fd, buf, count);
		#endif
	}

	size_t writeFile(filehandle_t fd, void* buf, size_t count)
	{
		#ifdef _WIN32
			DWORD didWrite = 0;
			bool success = WriteFile(fd, buf, (platform::DWORD) count, &didWrite, 0);
			if(!success)
				_error_and_exit("Failed to write file (wanted %d bytes, wrote %d bytes); (error code %d)", count, didWrite, GetLastError());

			return (size_t) didWrite;
		#else
			return write(fd, buf, count);
		#endif
	}



	bool checkFileExists(const std::string& path)
	{
		#ifdef _WIN32
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
		#ifdef _WIN32
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
				// _error_and_exit("Failed to open file '%s' (error code %d)", partial, GetLastError());

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
				// _error_and_exit("Failed to open file '%s' (error code %d)", partial, GetLastError());
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
}













