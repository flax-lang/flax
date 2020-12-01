// msvcfinder.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "frontend.h"

#include <sstream>
#include <fstream>

#if OS_WINDOWS

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include <windows.h>

namespace platform {
namespace compiler
{
	// the techniques used here are with reference to Jon Blow's "microsoft_craziness.h" file.
	// it was released under the MIT license. see: https://gist.github.com/machinamentum/a2b587a68a49094257da0c39a6c4405f

	struct DECLSPEC_UUID("B41463C3-8866-43B5-BC33-2B0676F7F42E") DECLSPEC_NOVTABLE ISetupInstance : public IUnknown
	{
		STDMETHOD(GetInstanceId)(_Out_ BSTR* pbstrInstanceId) = 0;
		STDMETHOD(GetInstallDate)(_Out_ LPFILETIME pInstallDate) = 0;
		STDMETHOD(GetInstallationName)(_Out_ BSTR* pbstrInstallationName) = 0;
		STDMETHOD(GetInstallationPath)(_Out_ BSTR* pbstrInstallationPath) = 0;
		STDMETHOD(GetInstallationVersion)(_Out_ BSTR* pbstrInstallationVersion) = 0;
		STDMETHOD(GetDisplayName)(_In_ LCID lcid, _Out_ BSTR* pbstrDisplayName) = 0;
		STDMETHOD(GetDescription)(_In_ LCID lcid, _Out_ BSTR* pbstrDescription) = 0;
		STDMETHOD(ResolvePath)(_In_opt_z_ LPCOLESTR pwszRelativePath, _Out_ BSTR* pbstrAbsolutePath) = 0;
	};

	struct DECLSPEC_UUID("6380BCFF-41D3-4B2E-8B2E-BF8A6810C848") DECLSPEC_NOVTABLE IEnumSetupInstances : public IUnknown
	{
		STDMETHOD(Next)(_In_ ULONG celt, _Out_writes_to_(celt, *pceltFetched) ISetupInstance** rgelt,
			_Out_opt_ _Deref_out_range_(0, celt) ULONG* pceltFetched) = 0;

		STDMETHOD(Skip)(_In_ ULONG celt) = 0;
		STDMETHOD(Reset)(void) = 0;
		STDMETHOD(Clone)(_Deref_out_opt_ IEnumSetupInstances** ppenum) = 0;
	};

	struct DECLSPEC_UUID("42843719-DB4C-46C2-8E7C-64F1816EFD5B") DECLSPEC_NOVTABLE ISetupConfiguration : public IUnknown
	{
		STDMETHOD(EnumInstances)(_Out_ IEnumSetupInstances** ppEnumInstances) = 0;
		STDMETHOD(GetInstanceForCurrentProcess)(_Out_ ISetupInstance** ppInstance) = 0;
		STDMETHOD(GetInstanceForPath)(_In_z_ LPCWSTR wzPath, _Out_ ISetupInstance** ppInstance) = 0;
	};

	struct DECLSPEC_UUID("42B21B78-6192-463E-87BF-D577838F1D5C") DECLSPEC_NOVTABLE ISetupHelper : public IUnknown
	{
		STDMETHOD(ParseVersion)(_In_ LPCOLESTR pwszVersion, _Out_ PULONGLONG pullVersion) = 0;
		STDMETHOD(ParseVersionRange)(_In_ LPCOLESTR pwszVersionRange, _Out_ PULONGLONG pullMinVersion, _Out_ PULONGLONG pullMaxVersion) = 0;
	};



	struct VersionData
	{
		int32_t bestVersion[4];
		std::wstring bestName;
	};

	struct FindResult
	{
		int windowsVersion;
		std::string windowsSDKRoot;
		std::string vsBinDirectory;
		std::string vsLibDirectory;
	};

	static bool checkFileExists(const std::wstring& name)
	{
		auto attrib = GetFileAttributesW(name.c_str());
		return attrib != INVALID_FILE_ATTRIBUTES;
	}

	static bool visitFiles(const std::wstring& dir, VersionData* vd, std::function<void (const std::wstring& shortName,
		const std::wstring& fullName, VersionData* vd)> visitor)
	{
		auto wildcard = dir + L"\\*";

		WIN32_FIND_DATAW findData;
		auto handle = FindFirstFileW(wildcard.c_str(), &findData);
		if(handle == INVALID_HANDLE_VALUE) return false;


		while(true)
		{
			// make sure it's a directory, and don't read '.' or '..'
			if((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (findData.cFileName[0] != '.'))
			{
				auto full = dir + L"\\" + findData.cFileName;
				visitor(findData.cFileName, full, vd);
			}

			auto success = FindNextFileW(handle, &findData);
			if(!success) break;
		}

		FindClose(handle);
		return true;
	}


	static std::wstring readRegistryString(HKEY key, const std::wstring& name)
	{
		// If the registry data changes between the first and second calls to RegQueryValueExW,
		// we may fail to get the entire key, even though it told us initially that our buffer length
		// would be big enough. The only solution is to keep looping until we don't fail.

		DWORD required = 0;
		auto rc = RegQueryValueExW(key, name.c_str(), NULL, NULL, NULL, &required);
		if(rc != 0) return L"";


		wchar_t* value = 0;
		DWORD length = 0;

		while(true)
		{
			length = required + 2;
			value = (wchar_t*) malloc(length + 2);
			if(!value)
				return L"";

			DWORD type;
			rc = RegQueryValueExW(key, name.c_str(), NULL, &type, (LPBYTE) value, &length);
			if(rc == ERROR_MORE_DATA)
			{
				free(value);
				required = length;
				continue;
			}

			// only get strings
			if((rc != 0) || (type != REG_SZ))
			{
				free(value);
				return L"";
			}

			break;
		}

		auto num_wchars = length / 2;
		value[num_wchars] = 0;

		auto ret = std::wstring(value);
		free(value);

		return ret;
	}

	static void getBestWin10Version(const std::wstring& shortName, const std::wstring& fullName, VersionData* vd)
	{
		// find the win10 subdir with the highest version number.
		int i0 = 0;
		int i1 = 0;
		int i2 = 0;
		int i3 = 0;

		auto gots = swscanf_s(shortName.c_str(), L"%d.%d.%d.%d", &i0, &i1, &i2, &i3);
		if(gots < 4) return;

		auto b0 = vd->bestVersion[0];
		auto b1 = vd->bestVersion[1];
		auto b2 = vd->bestVersion[2];
		auto b3 = vd->bestVersion[3];

		// short-circuiting ftw.
		if((b0 > i0) || (b1 > i1) || (b2 > i2) || (b3 > i3))
			return;

		vd->bestName = fullName;
		vd->bestVersion[0] = i0;
		vd->bestVersion[1] = i1;
		vd->bestVersion[2] = i2;
		vd->bestVersion[3] = i3;
	}

	static void getBestWin8Version(const std::wstring& shortName, const std::wstring& fullName, VersionData* vd)
	{
		// find the win8 subdir with the highest version number.
		int i0 = 0;
		int i1 = 0;

		auto gots = swscanf_s(shortName.c_str(), L"winv%d.%d", &i0, &i1);
		if(gots < 2)
			return;

		auto b0 = vd->bestVersion[0];
		auto b1 = vd->bestVersion[1];

		// short-circuiting ftw.
		if((b0 > i0) || (b1 > i1))
			return;

		vd->bestName = fullName;
		vd->bestVersion[0] = i0;
		vd->bestVersion[1] = i1;
	}


	static void findWindowsKitRoot(FindResult* result)
	{
		HKEY key;
		auto rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", 0,
			KEY_QUERY_VALUE | KEY_WOW64_32KEY | KEY_ENUMERATE_SUB_KEYS, &key);

		if(rc != S_OK)
			return;

		defer(RegCloseKey(key));


		// find a windows 10 thing
		auto win10root = readRegistryString(key, L"KitsRoot10");
		if(!win10root.empty())
		{
			auto win10lib = win10root + L"Lib";

			VersionData vd;
			memset(&vd, 0, sizeof(VersionData));

			visitFiles(win10lib, &vd, &getBestWin10Version);

			if(!vd.bestName.empty())
			{
				result->windowsVersion = 10;
				result->windowsSDKRoot = convertWCharToString(vd.bestName);

				return;
			}
		}

		auto win8root = readRegistryString(key, L"KitsRoot81");
		if(!win8root.empty())
		{
			auto win10lib = win10root + L"Lib";

			VersionData vd;
			memset(&vd, 0, sizeof(VersionData));

			visitFiles(win10lib, &vd, &getBestWin8Version);

			if(!vd.bestName.empty())
			{
				result->windowsVersion = 8;
				result->windowsSDKRoot = convertWCharToString(vd.bestName);

				return;
			}
		}
	}


	static std::string trim(std::string s)
	{
		auto ltrim = [](std::string& s) -> std::string& {
			s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
			return s;
		};

		auto rtrim = [](std::string& s) -> std::string& {
			s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
			return s;
		};

		return ltrim(rtrim(s));
	}



	static bool findVSToolchain(FindResult* result)
	{
		// for vs >= 2017, we need to do some COM stupidity.

		CoInitializeEx(NULL, COINIT_MULTITHREADED);

		GUID my_uid             = { 0x42843719, 0xDB4C, 0x46C2, { 0x8E, 0x7C, 0x64, 0xF1, 0x81, 0x6E, 0xFD, 0x5B } };
		GUID clsid_setupConfig  = { 0x177F0C4A, 0x1CD3, 0x4DE7, { 0xA3, 0x2C, 0x71, 0xDB, 0xBB, 0x9F, 0xA3, 0x6D } };

		ISetupConfiguration* config = NULL;
		auto hr = CoCreateInstance(clsid_setupConfig, NULL, CLSCTX_INPROC_SERVER, my_uid, (void**) &config);
		if(hr != S_OK) return false;

		defer(config->Release());


		IEnumSetupInstances* instances = NULL;
		hr = config->EnumInstances(&instances);
		if(hr != S_OK)  return false;
		if(!instances)  return false;

		defer(instances->Release());


		ISetupInstance* inst = 0;
		uint64_t newestVersionNum = 0;

		// we look for the newest version that's installed, as opposed to the first.
		while(true)
		{
			ISetupInstance* instance = NULL;
			auto hr = instances->Next(1, &instance, NULL);
			if(hr != S_OK) break;

			BSTR versionString;
			uint64_t versionNum = 0;

			hr = instance->GetInstallationVersion(&versionString);
			if(hr != S_OK) continue;

			defer(SysFreeString(versionString));

			hr = ((ISetupHelper*) config)->ParseVersion(versionString, &versionNum);
			if(hr != S_OK) continue;

			if(newestVersionNum == 0 || versionNum > newestVersionNum)
			{
				inst = instance;
				newestVersionNum = versionNum;
			}
			else
			{
				instance->Release();
			}
		}


		if(!inst)
			return false;

		std::string vsRoot;
		{
			BSTR tmp;
			auto hr = inst->ResolvePath(L"VC", &tmp);
			if(hr != S_OK) return false;

			vsRoot = convertWCharToString(std::wstring(tmp));
			SysFreeString(tmp);

			inst->Release();
		}

		std::string toolchainVersion;
		{
			auto path = strprintf("%s\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt", vsRoot);

			auto in = std::ifstream(path, std::ios::in);
			std::getline(in, toolchainVersion);

			toolchainVersion = trim(toolchainVersion);
		}

		std::string toolchainPath = strprintf("%s\\Tools\\MSVC\\%s", vsRoot, toolchainVersion);
		if(checkFileExists(convertStringToWChar(toolchainPath)))
		{
			//* this is *HOST* architecture, so we can just use our defines.
			result->vsBinDirectory = strprintf("%s\\bin\\Host%s", toolchainPath, ARCH_64 ? "x64" : "x86");
			result->vsLibDirectory = strprintf("%s\\lib", toolchainPath);

			return true;
		}
		else
		{
			return false;
		}
	}


	static FindResult* getResult()
	{
		static bool cached = false;
		static FindResult cachedResult;

		if(!cached)
		{
			memset(&cachedResult, 0, sizeof(FindResult));

			findWindowsKitRoot(&cachedResult);
			auto found = findVSToolchain(&cachedResult);
			if(!found) error("backend: failed to find installed Visual Studio location!");

			cached = true;
		}

		return &cachedResult;
	}

	std::string getWindowsSDKLocation()
	{
		return getResult()->windowsSDKRoot;
	}

	std::string getVSToolchainLibLocation()
	{
		return getResult()->vsLibDirectory;
	}

	std::string getVSToolchainBinLocation()
	{
		return getResult()->vsBinDirectory;
	}
}
}

#endif




















