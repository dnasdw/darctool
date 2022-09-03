#ifndef DARCTOOL_H_
#define DARCTOOL_H_

#include <sdw.h>

class CDarcTool
{
public:
	enum EParseOptionReturn
	{
		kParseOptionReturnSuccess,
		kParseOptionReturnIllegalOption,
		kParseOptionReturnNoArgument,
		kParseOptionReturnUnknownArgument,
		kParseOptionReturnOptionConflict
	};
	enum EAction
	{
		kActionNone,
		kActionExtract,
		kActionCreate,
		kActionHelp
	};
	struct SOption
	{
		const UChar* Name;
		int Key;
		const UChar* Doc;
	};
	CDarcTool();
	~CDarcTool();
	int ParseOptions(int a_nArgc, UChar* a_pArgv[]);
	int CheckOptions();
	int Help();
	int Action();
	static SOption s_Option[];
private:
	EParseOptionReturn parseOptions(const UChar* a_pName, int& a_nIndex, int a_nArgc, UChar* a_pArgv[]);
	EParseOptionReturn parseOptions(int a_nKey, int& a_nIndex, int a_nArgc, UChar* a_pArgv[]);
	bool extractFile();
	bool createFile();
	EAction m_eAction;
	UString m_sFileName;
	UString m_sDirName;
	n32 m_nSharedAlignment;
	map<n32, vector<URegex>> m_mUniqueAlignment;
	bool m_bExcludeRoot;
	bool m_bUseHeaderSize;
	bool m_bVerbose;
	UString m_sMessage;
};

#endif	// DARCTOOL_H_
