#include "darctool.h"
#include "darc.h"

CDarcTool::SOption CDarcTool::s_Option[] =
{
	{ USTR("extract"), USTR('x'), USTR("extract the darc file") },
	{ USTR("create"), USTR('c'), USTR("create the darc file") },
	{ USTR("file"), USTR('f'), USTR("the darc file") },
	{ USTR("dir"), USTR('d'), USTR("the dir for the darc file") },
	{ USTR("shared-alignment"), USTR('a'), USTR("the shared alignment, default is 32") },
	{ USTR("unique-alignment"), USTR('r'), USTR("the path regex pattern and the unique alignment") },
	{ USTR("exclude-root"), USTR('e'), USTR("exclude the root dot dir") },
	{ USTR("use-header-size"), 0, USTR("use header size instead of data offset") },
	{ USTR("verbose"), USTR('v'), USTR("show the info") },
	{ USTR("help"), USTR('h'), USTR("show this help") },
	{ nullptr, 0, nullptr }
};

CDarcTool::CDarcTool()
	: m_eAction(kActionNone)
	, m_nSharedAlignment(32)
	, m_bExcludeRoot(false)
	, m_bUseHeaderSize(false)
	, m_bVerbose(false)
{
}

CDarcTool::~CDarcTool()
{
}

int CDarcTool::ParseOptions(int a_nArgc, UChar* a_pArgv[])
{
	if (a_nArgc <= 1)
	{
		return 1;
	}
	for (int i = 1; i < a_nArgc; i++)
	{
		int nArgpc = static_cast<int>(UCslen(a_pArgv[i]));
		if (nArgpc == 0)
		{
			continue;
		}
		int nIndex = i;
		if (a_pArgv[i][0] != USTR('-'))
		{
			UPrintf(USTR("ERROR: illegal option\n\n"));
			return 1;
		}
		else if (nArgpc > 1 && a_pArgv[i][1] != USTR('-'))
		{
			for (int j = 1; j < nArgpc; j++)
			{
				switch (parseOptions(a_pArgv[i][j], nIndex, a_nArgc, a_pArgv))
				{
				case kParseOptionReturnSuccess:
					break;
				case kParseOptionReturnIllegalOption:
					UPrintf(USTR("ERROR: illegal option\n\n"));
					return 1;
				case kParseOptionReturnNoArgument:
					UPrintf(USTR("ERROR: no argument\n\n"));
					return 1;
				case kParseOptionReturnUnknownArgument:
					UPrintf(USTR("ERROR: unknown argument \"%") PRIUS USTR("\"\n\n"), m_sMessage.c_str());
					return 1;
				case kParseOptionReturnOptionConflict:
					UPrintf(USTR("ERROR: option conflict\n\n"));
					return 1;
				}
			}
		}
		else if (nArgpc > 2 && a_pArgv[i][1] == USTR('-'))
		{
			switch (parseOptions(a_pArgv[i] + 2, nIndex, a_nArgc, a_pArgv))
			{
			case kParseOptionReturnSuccess:
				break;
			case kParseOptionReturnIllegalOption:
				UPrintf(USTR("ERROR: illegal option\n\n"));
				return 1;
			case kParseOptionReturnNoArgument:
				UPrintf(USTR("ERROR: no argument\n\n"));
				return 1;
			case kParseOptionReturnUnknownArgument:
				UPrintf(USTR("ERROR: unknown argument \"%") PRIUS USTR("\"\n\n"), m_sMessage.c_str());
				return 1;
			case kParseOptionReturnOptionConflict:
				UPrintf(USTR("ERROR: option conflict\n\n"));
				return 1;
			}
		}
		i = nIndex;
	}
	return 0;
}

int CDarcTool::CheckOptions()
{
	if (m_eAction == kActionNone)
	{
		UPrintf(USTR("ERROR: nothing to do\n\n"));
		return 1;
	}
	if (m_eAction != kActionHelp)
	{
		if (m_sFileName.empty())
		{
			UPrintf(USTR("ERROR: no --file option\n\n"));
			return 1;
		}
		if (m_sDirName.empty())
		{
			UPrintf(USTR("ERROR: no --dir option\n\n"));
			return 1;
		}
	}
	if (m_eAction == kActionExtract)
	{
		if (!CDarc::IsDarcFile(m_sFileName))
		{
			UPrintf(USTR("ERROR: %") PRIUS USTR(" is not a darc file\n\n"), m_sFileName.c_str());
			return 1;
		}
	}
	return 0;
}

int CDarcTool::Help()
{
	UPrintf(USTR("darctool %") PRIUS USTR(" by dnasdw\n\n"), AToU(DARCTOOL_VERSION).c_str());
	UPrintf(USTR("usage: darctool [option...] [option]...\n\n"));
	UPrintf(USTR("sample:\n"));
	UPrintf(USTR("  darctool -xvfd input.darc outputdir\n"));
	UPrintf(USTR("  darctool -cvfd output.darc inputdir\n"));
	UPrintf(USTR("  darctool -cvfd output.darc inputdir -a 4 -r \\.bclim 128 -r \\.bcfnt 128 -r \\.bcfna 128\n"));
	UPrintf(USTR("  darctool -cvfd output.darc inputdir --exclude-root\n"));
	UPrintf(USTR("  darctool -cvfd output.darc inputdir --use-header-size\n"));
	UPrintf(USTR("\n"));
	UPrintf(USTR("option:\n"));
	SOption* pOption = s_Option;
	while (pOption->Name != nullptr || pOption->Doc != nullptr)
	{
		if (pOption->Name != nullptr)
		{
			UPrintf(USTR("  "));
			if (pOption->Key != 0)
			{
				UPrintf(USTR("-%c,"), pOption->Key);
			}
			else
			{
				UPrintf(USTR("   "));
			}
			UPrintf(USTR(" --%-8") PRIUS, pOption->Name);
			if (UCslen(pOption->Name) >= 8 && pOption->Doc != nullptr)
			{
				UPrintf(USTR("\n%16") PRIUS, USTR(""));
			}
		}
		if (pOption->Doc != nullptr)
		{
			UPrintf(USTR("%") PRIUS, pOption->Doc);
		}
		UPrintf(USTR("\n"));
		pOption++;
	}
	return 0;
}

int CDarcTool::Action()
{
	if (m_eAction == kActionExtract)
	{
		if (!extractFile())
		{
			UPrintf(USTR("ERROR: extract file failed\n\n"));
			return 1;
		}
	}
	if (m_eAction == kActionCreate)
	{
		if (!createFile())
		{
			UPrintf(USTR("ERROR: create file failed\n\n"));
			return 1;
		}
	}
	if (m_eAction == kActionHelp)
	{
		return Help();
	}
	return 0;
}

CDarcTool::EParseOptionReturn CDarcTool::parseOptions(const UChar* a_pName, int& a_nIndex, int a_nArgc, UChar* a_pArgv[])
{
	if (UCscmp(a_pName, USTR("extract")) == 0)
	{
		if (m_eAction == kActionNone)
		{
			m_eAction = kActionExtract;
		}
		else if (m_eAction != kActionExtract && m_eAction != kActionHelp)
		{
			return kParseOptionReturnOptionConflict;
		}
	}
	else if (UCscmp(a_pName, USTR("create")) == 0)
	{
		if (m_eAction == kActionNone)
		{
			m_eAction = kActionCreate;
		}
		else if (m_eAction != kActionCreate && m_eAction != kActionHelp)
		{
			return kParseOptionReturnOptionConflict;
		}
	}
	else if (UCscmp(a_pName, USTR("file")) == 0)
	{
		if (a_nIndex + 1 >= a_nArgc)
		{
			return kParseOptionReturnNoArgument;
		}
		m_sFileName = a_pArgv[++a_nIndex];
	}
	else if (UCscmp(a_pName, USTR("dir")) == 0)
	{
		if (a_nIndex + 1 >= a_nArgc)
		{
			return kParseOptionReturnNoArgument;
		}
		m_sDirName = a_pArgv[++a_nIndex];
	}
	else if (UCscmp(a_pName, USTR("shared-alignment")) == 0)
	{
		if (a_nIndex + 1 >= a_nArgc)
		{
			return kParseOptionReturnNoArgument;
		}
		UString sSharedAlignment = a_pArgv[++a_nIndex];
		n32 nSharedAlignment = SToN32(sSharedAlignment);
		if (nSharedAlignment < 1)
		{
			m_sMessage = sSharedAlignment;
			return kParseOptionReturnUnknownArgument;
		}
		m_nSharedAlignment = nSharedAlignment;
	}
	else if (UCscmp(a_pName, USTR("unique-alignment")) == 0)
	{
		if (a_nIndex + 2 >= a_nArgc)
		{
			return kParseOptionReturnNoArgument;
		}
		UString sPattern = a_pArgv[++a_nIndex];
		UString sUniqueAlignment = a_pArgv[++a_nIndex];
		n32 nUniqueAlignment = SToN32(sUniqueAlignment);
		if (nUniqueAlignment < 1)
		{
			m_sMessage = sUniqueAlignment;
			return kParseOptionReturnUnknownArgument;
		}
		try
		{
			URegex rPattern(sPattern, regex_constants::ECMAScript | regex_constants::icase);
			m_mUniqueAlignment[nUniqueAlignment].push_back(rPattern);
		}
		catch (regex_error& e)
		{
			UPrintf(USTR("ERROR: %") PRIUS USTR("\n\n"), AToU(e.what()).c_str());
			m_sMessage = sPattern;
			return kParseOptionReturnUnknownArgument;
		}
	}
	else if (UCscmp(a_pName, USTR("exclude-root")) == 0)
	{
		m_bExcludeRoot = true;
	}
	else if (UCscmp(a_pName, USTR("use-header-size")) == 0)
	{
		m_bUseHeaderSize = true;
	}
	else if (UCscmp(a_pName, USTR("verbose")) == 0)
	{
		m_bVerbose = true;
	}
	else if (UCscmp(a_pName, USTR("help")) == 0)
	{
		m_eAction = kActionHelp;
	}
	return kParseOptionReturnSuccess;
}

CDarcTool::EParseOptionReturn CDarcTool::parseOptions(int a_nKey, int& a_nIndex, int a_nArgc, UChar* a_pArgv[])
{
	for (SOption* pOption = s_Option; pOption->Name != nullptr || pOption->Key != 0 || pOption->Doc != nullptr; pOption++)
	{
		if (pOption->Key == a_nKey)
		{
			return parseOptions(pOption->Name, a_nIndex, a_nArgc, a_pArgv);
		}
	}
	return kParseOptionReturnIllegalOption;
}

bool CDarcTool::extractFile()
{
	CDarc darc;
	darc.SetFileName(m_sFileName);
	darc.SetDirName(m_sDirName);
	darc.SetVerbose(m_bVerbose);
	return darc.ExtractFile();
}

bool CDarcTool::createFile()
{
	CDarc darc;
	darc.SetFileName(m_sFileName);
	darc.SetDirName(m_sDirName);
	darc.SetSharedAlignment(m_nSharedAlignment);
	darc.SetUniqueAlignment(m_mUniqueAlignment);
	darc.SetExcludeRoot(m_bExcludeRoot);
	darc.SetUseHeaderSize(m_bUseHeaderSize);
	darc.SetVerbose(m_bVerbose);
	return darc.CreateFile();
}

int UMain(int argc, UChar* argv[])
{
	CDarcTool tool;
	if (tool.ParseOptions(argc, argv) != 0)
	{
		return tool.Help();
	}
	if (tool.CheckOptions() != 0)
	{
		return 1;
	}
	return tool.Action();
}
