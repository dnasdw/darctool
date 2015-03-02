#include "utility.h"

#if DARCTOOL_COMPILER != COMPILER_MSC
string g_sLocaleName = "";
#endif

void FSetLocale()
{
	string sLocale = setlocale(LC_ALL, "");
#if DARCTOOL_COMPILER != COMPILER_MSC
	vector<string> vLocale = FSSplit<string>(sLocale, ".");
	if (!vLocale.empty())
	{
		g_sLocaleName = vLocale.back();
	}
#endif
}

#if DARCTOOL_COMPILER == COMPILER_MSC
string FSWToU8(const wstring& a_sString)
{
	static wstring_convert<codecvt_utf8<wchar_t>> c_cvt_u8;
	return c_cvt_u8.to_bytes(a_sString);
}

string FSU16ToU8(const u16string& a_sString)
{
	static wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> c_cvt_u8_u16;
	return c_cvt_u8_u16.to_bytes(a_sString);
}

wstring FSU8ToW(const string& a_sString)
{
	static wstring_convert<codecvt_utf8<wchar_t>> c_cvt_u8;
	return c_cvt_u8.from_bytes(a_sString);
}

wstring FSAToW(const string& a_sString)
{
	static wstring_convert<codecvt<wchar_t, char, mbstate_t>> c_cvt_a(new codecvt<wchar_t, char, mbstate_t>(""));
	return c_cvt_a.from_bytes(a_sString);
}

wstring FSU16ToW(const u16string& a_sString)
{
	return FSU8ToW(FSU16ToU8(a_sString));
}

u16string FSU8ToU16(const string& a_sString)
{
	static wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> c_cvt_u8_u16;
	return c_cvt_u8_u16.from_bytes(a_sString);
}

u16string FSWToU16(const wstring& a_sString)
{
	return FSU8ToU16(FSWToU8(a_sString));
}
#else
string FSWToU8(const wstring& a_sString)
{
	return FSTToT<wstring, string>(a_sString, "WCHAR_T", "UTF-8");
}

string FSU16ToU8(const u16string& a_sString)
{
	return FSTToT<u16string, string>(a_sString, "UTF-16LE", "UTF-8");
}

wstring FSU8ToW(const string& a_sString)
{
	return FSTToT<string, wstring>(a_sString, "UTF-8", "WCHAR_T");
}

wstring FSAToW(const string& a_sString)
{
	return FSTToT<string, wstring>(a_sString, g_sLocaleName, "WCHAR_T");
}

wstring FSU16ToW(const u16string& a_sString)
{
	return FSTToT<u16string, wstring>(a_sString, "UTF-16LE", "WCHAR_T");
}

u16string FSU8ToU16(const string& a_sString)
{
	return FSTToT<string, u16string>(a_sString, "UTF-8", "UTF-16LE");
}

u16string FSWToU16(const wstring& a_sString)
{
	return FSTToT<wstring, u16string>(a_sString, "WCHAR_T", "UTF-16LE");
}
#endif

const String& FGetModuleDir()
{
	const int nMaxPath = 4096;
	static String sDir;
	sDir.resize(nMaxPath, STR('\0'));
#if DARCTOOL_COMPILER == COMPILER_MSC
	GetModuleFileNameW(nullptr, &sDir.front(), nMaxPath);
#elif defined(DARCTOOL_APPLE)
	char path[nMaxPath] = {};
	u32 uPathSize = static_cast<u32>(sizeof(path));
	if (_NSGetExecutablePath(path, &uPathSize) != 0)
	{
		printf("ERROR: _NSGetExecutablePath error\n\n");
	}
	else if (realpath(path, &sDir.front()) == nullptr)
	{
		sDir.erase();
	}
#else
	ssize_t nCount = readlink("/proc/self/exe", &sDir.front(), nMaxPath);
	if (nCount == -1)
	{
		printf("ERROR: readlink /proc/self/exe error\n\n");
	}
	else
	{
		sDir[nCount] = '\0';
	}
#endif
	replace(sDir.begin(), sDir.end(), STR('\\'), STR('/'));
	String::size_type nPos = sDir.rfind(STR('/'));
	if (nPos != String::npos)
	{
		sDir.erase(nPos);
	}
	return sDir;
}

void FCopyFile(FILE* a_fpDest, FILE* a_fpSrc, n64 a_nSrcOffset, n64 a_nSize)
{
	const n64 nBufferSize = 0x100000;
	u8* pBuffer = new u8[nBufferSize];
	FFseek(a_fpSrc, a_nSrcOffset, SEEK_SET);
	while (a_nSize > 0)
	{
		n64 nSize = a_nSize > nBufferSize ? nBufferSize : a_nSize;
		fread(pBuffer, 1, static_cast<size_t>(nSize), a_fpSrc);
		fwrite(pBuffer, 1, static_cast<size_t>(nSize), a_fpDest);
		a_nSize -= nSize;
	}
	delete[] pBuffer;
}

bool FGetFileSize32(const String::value_type* a_pFileName, n32& a_nFileSize)
{
	SStat st;
	if (FStat(a_pFileName, &st) != 0)
	{
		a_nFileSize = 0;
		return false;
	}
	a_nFileSize = static_cast<n32>(st.st_size);
	return true;
}

bool FMakeDir(const String::value_type* a_pDirName)
{
	if (FMkdir(a_pDirName) != 0)
	{
		if (errno != EEXIST)
		{
			return false;
		}
	}
	return true;
}

FILE* FFopenA(const char* a_pFileName, const char* a_pMode)
{
	FILE* fp = fopen(a_pFileName, a_pMode);
	if (fp == nullptr)
	{
		printf("ERROR: open file %s failed\n\n", a_pFileName);
	}
	return fp;
}

#if DARCTOOL_COMPILER == COMPILER_MSC
FILE* FFopenW(const wchar_t* a_pFileName, const wchar_t* a_pMode)
{
	FILE* fp = _wfopen(a_pFileName, a_pMode);
	if (fp == nullptr)
	{
		wprintf(L"ERROR: open file %s failed\n\n", a_pFileName);
	}
	return fp;
}
#endif

bool FSeek(FILE* a_fpFile, n64 a_nOffset)
{
	if (fflush(a_fpFile) != 0)
	{
		return false;
	}
	int nFd = FFileno(a_fpFile);
	if (nFd == -1)
	{
		return false;
	}
	FFseek(a_fpFile, 0, SEEK_END);
	n64 nFileSize = FFtell(a_fpFile);
	if (nFileSize < a_nOffset)
	{
		n64 nOffset = FLseek(nFd, a_nOffset - 1, SEEK_SET);
		if (nOffset == -1)
		{
			return false;
		}
		fputc(0, a_fpFile);
		fflush(a_fpFile);
	}
	else
	{
		FFseek(a_fpFile, a_nOffset, SEEK_SET);
	}
	return true;
}

n64 FAlign(n64 a_nOffset, n64 a_nAlignment)
{
	return (a_nOffset + a_nAlignment - 1) / a_nAlignment * a_nAlignment;
}
