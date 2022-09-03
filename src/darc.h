#ifndef DARC_H_
#define DARC_H_

#include <sdw.h>

#include SDW_MSC_PUSH_PACKED
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
} SDW_GNUC_PACKED;
#include SDW_MSC_POP_PACKED

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
		UString EntryName;
		n32 ParentSiblingDirOffset;
		UString Prefix;
		EExtractState ExtractState;
	};
	struct SEntry
	{
		UString Path;
		U16String EntryName;
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
	void SetFileName(const UString& a_sFileName);
	void SetDirName(const UString& a_sDirName);
	void SetSharedAlignment(n32 a_nSharedAlignment);
	void SetUniqueAlignment(const map<n32, vector<URegex>>& a_mUniqueAlignment);
	void SetExcludeRoot(bool a_bExcludeRoot);
	void SetUseHeaderSize(bool a_bUseHeaderSize);
	void SetVerbose(bool a_bVerbose);
	bool ExtractFile();
	bool CreateFile();
	static bool IsDarcFile(const UString& a_sFileName);
	static const u32 s_uSignature;
	static const n32 s_nInvalidOffset;
private:
	void pushExtractStackElement(n32 a_nEntryOffset, n32 a_nParentSiblingDirOffset, const UString& a_sPrefix);
	bool extractDirEntry();
	bool extractFileEntry();
	void setupCreate();
	void buildIgnoreList();
	void insertDirEntry(n32 a_nEntryOffset, const UString& a_sEntryName, n32 a_nParentDirOffset);
	bool insertFileEntry(n32 a_nEntryOffset, const UString& a_sEntryName, n32 a_nParentDirOffset);
	void moveEntry(n32 a_nEntryOffset);
	void addDirOffset(n32& a_nOffset, n32 a_nIndex);
	void pushCreateDequeElement(n32 a_nEntryOffset);
	bool createEntryList();
	bool matchInIgnoreList(const UString& a_sPath) const;
	void redirectSiblingDirOffset();
	void createEntryName();
	void calculateFileOffset();
	n32 getAlignment(const UString& a_sPath) const;
	void writeHeader();
	void writeEntry();
	bool writeData();
	bool writeFromFile(const UString& a_sPath, n32 a_nOffset, n32 a_nSize);
	UString m_sFileName;
	UString m_sDirName;
	n32 m_nSharedAlignment;
	map<n32, vector<URegex>> m_mUniqueAlignment;
	bool m_bExcludeRoot;
	bool m_bUseHeaderSize;
	bool m_bVerbose;
	FILE* m_fpDarc;
	SDarcHeader m_DarcHeader;
	n32 m_nEntryCount;
	vector<Char16_t> m_vEntryName;
	stack<SExtractStackElement> m_sExtractStack;
	vector<URegex> m_vIgnoreList;
	vector<SEntry> m_vCreateEntry;
	deque<SCreateDequeElement> m_dCreateDeque;
};

#endif	// DARC_H_
