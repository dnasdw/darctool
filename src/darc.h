#ifndef DARC_H_
#define DARC_H_

#include "utility.h"

#include MSC_PUSH_PACKED
struct SDarcHeader
{
	u32 Signature;
	u16 ByteOrder;
	u16 HeaderSize;
	u32 Version;
	u32 FileSize;
	u32 EntryOffset;
	u32 EntrySize;
	u32 DataOffset;
} GNUC_PACKED;
#include MSC_POP_PACKED

class CDarc
{
public:
	enum EExtractState
	{
		kExtractStateBegin,
		kExtractStateSiblingDir,
		kExtractStateEnd
	};
	struct SCommonDirEntry
	{
		n32 ParentDirOffset;
		n32 SiblingDirOffset;
	};
	struct SCommonFileEntry
	{
		n32 FileOffset;
		n32 FileSize;
	};
	union UCommonEntry
	{
		SCommonDirEntry Dir;
		SCommonFileEntry File;
	};
	struct SCommonEntry
	{
		n32 NameOffset;
		UCommonEntry Entry;
	};
	struct SExtractStackElement
	{
		n32 EntryOffset;
		SCommonEntry Entry;
		bool IsDir;
		String EntryName;
		n32 ParentSiblingDirOffset;
		String Prefix;
		EExtractState ExtractState;
	};
	struct SEntry
	{
		String Path;
		u16string EntryName;
		bool IsDir;
		n32 ChildCount;
		n32 ChildDirOffset;
		SCommonEntry Entry;
	};
	struct SCreateDequeElement
	{
		n32 EntryOffset;
		vector<n32> ChildOffset;
		n32 ChildIndex;
	};
	CDarc();
	~CDarc();
	void SetFileName(const char* a_pFileName);
	void SetDirName(const char* a_pRomFsDirName);
	void SetVerbose(bool a_bVerbose);
	bool ExtractFile();
	bool CreateFile();
	static bool IsDarcFile(const char* a_pFileName);
	static const u32 s_uSignature;
	static const n32 s_nInvalidOffset;
	static const int s_nEntryAlignment4;
	static const int s_nEntryAlignment128;
private:
	void pushExtractStackElement(n32 a_nEntryOffset, n32 a_nParentSiblingDirOffset, const String& a_sPrefix);
	bool extractDirEntry();
	bool extractFileEntry();
	void setupCreate();
	void buildBlackList();
	void buildAlignList();
	void insertDirEntry(n32 a_nEntryOffset, const String& a_sEntryName, n32 a_nParentDirOffset);
	bool insertFileEntry(n32 a_nEntryOffset, const String& a_sEntryName, n32 a_nParentDirOffset);
	void moveEntry(n32 a_nEntryOffset);
	void addDirOffset(n32& a_nOffset, n32 a_nIndex);
	void pushCreateDequeElement(n32 a_nEntryOffset);
	bool createEntryList();
	bool matchInBlackList(const String& a_sPath);
	void redirectSiblingDirOffset();
	void createEntryName();
	void calculateFileOffset();
	bool matchInAlignList(const String& a_sPath);
	void writeHeader();
	void writeEntry();
	bool writeData();
	bool writeFromFile(const String& a_sPath, n32 a_nOffset, n32 a_nSize);
	const char* m_pFileName;
	String m_sDirName;
	bool m_bVerbose;
	FILE* m_fpDarc;
	SDarcHeader m_DarcHeader;
	n32 m_nEntryCount;
	vector<char16_t> m_vEntryName;
	stack<SExtractStackElement> m_sExtractStack;
	vector<Regex> m_vBlackList;
	vector<Regex> m_vAlignList;
	vector<SEntry> m_vCreateEntry;
	deque<SCreateDequeElement> m_dCreateDeque;
};

#endif	// DARC_H_
