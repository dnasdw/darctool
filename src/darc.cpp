#include "darc.h"

const u32 CDarc::s_uSignature = SDW_CONVERT_ENDIAN32('darc');
const n32 CDarc::s_nInvalidOffset = -1;

CDarc::CDarc()
	: m_nSharedAlignment(32)
	, m_bExcludeRoot(false)
	, m_bUseHeaderSize(false)
	, m_bVerbose(false)
	, m_fpDarc(nullptr)
	, m_nEntryCount(0)
{
	memset(&m_DarcHeader, 0, sizeof(m_DarcHeader));
}

CDarc::~CDarc()
{
}

void CDarc::SetFileName(const UString& a_sFileName)
{
	m_sFileName = a_sFileName;
}

void CDarc::SetDirName(const UString& a_sDirName)
{
	m_sDirName = a_sDirName;
}

void CDarc::SetSharedAlignment(n32 a_nSharedAlignment)
{
	m_nSharedAlignment = a_nSharedAlignment;
}

void CDarc::SetUniqueAlignment(const map<n32, vector<URegex>>& a_mUniqueAlignment)
{
	m_mUniqueAlignment = a_mUniqueAlignment;
}

void CDarc::SetExcludeRoot(bool a_bExcludeRoot)
{
	m_bExcludeRoot = a_bExcludeRoot;
}

void CDarc::SetUseHeaderSize(bool a_bUseHeaderSize)
{
	m_bUseHeaderSize = a_bUseHeaderSize;
}

void CDarc::SetVerbose(bool a_bVerbose)
{
	m_bVerbose = a_bVerbose;
}

bool CDarc::ExtractFile()
{
	bool bResult = true;
	m_fpDarc = UFopen(m_sFileName, USTR("rb"));
	if (m_fpDarc == nullptr)
	{
		return false;
	}
	fread(&m_DarcHeader, sizeof(m_DarcHeader), 1, m_fpDarc);
	Fseek(m_fpDarc, m_DarcHeader.EntryOffset, SEEK_SET);
	SCommonEntry rootEntry;
	fread(&rootEntry, sizeof(rootEntry), 1, m_fpDarc);
	m_nEntryCount = rootEntry.Entry.Dir.SiblingDirOffset;
	n32 nNameOffset = m_DarcHeader.EntryOffset + m_nEntryCount * sizeof(SCommonEntry);
	n32 nNameSize = (m_DarcHeader.EntryOffset + m_DarcHeader.EntrySize - nNameOffset) / 2;
	m_vEntryName.resize(nNameSize);
	Fseek(m_fpDarc, nNameOffset, SEEK_SET);
	fread(&*m_vEntryName.begin(), 2, nNameSize, m_fpDarc);
	pushExtractStackElement(0, m_nEntryCount, USTR("/"));
	while (!m_sExtractStack.empty())
	{
		SExtractStackElement& current = m_sExtractStack.top();
		if (current.IsDir)
		{
			if (!extractDirEntry())
			{
				bResult = false;
			}
		}
		else if (!extractFileEntry())
		{
			bResult = false;
		}
	}
	fclose(m_fpDarc);
	return bResult;
}

bool CDarc::CreateFile()
{
	bool bResult = true;
	setupCreate();
	buildIgnoreList();
	insertDirEntry(0, USTR(""), 0);
	if (!m_bExcludeRoot)
	{
		insertDirEntry(1, USTR("."), 0);
	}
	pushCreateDequeElement(0);
	while (!m_dCreateDeque.empty())
	{
		if (!createEntryList())
		{
			bResult = false;
		}
	}
	redirectSiblingDirOffset();
	createEntryName();
	calculateFileOffset();
	m_fpDarc = UFopen(m_sFileName, USTR("wb"));
	if (m_fpDarc == nullptr)
	{
		return false;
	}
	Seek(m_fpDarc, m_DarcHeader.FileSize);
	writeHeader();
	writeEntry();
	if (!writeData())
	{
		bResult = false;
	}
	fclose(m_fpDarc);
	return bResult;
}

bool CDarc::IsDarcFile(const UString& a_sFileName)
{
	FILE* fp = UFopen(a_sFileName, USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	SDarcHeader darcHeader;
	fread(&darcHeader, sizeof(darcHeader), 1, fp);
	fclose(fp);
	return darcHeader.Signature == s_uSignature;
}

void CDarc::pushExtractStackElement(n32 a_nEntryOffset, n32 a_nParentSiblingDirOffset, const UString& a_sPrefix)
{
	if (a_nEntryOffset != m_nEntryCount)
	{
		m_sExtractStack.push(SExtractStackElement());
		SExtractStackElement& current = m_sExtractStack.top();
		current.EntryOffset = a_nEntryOffset;
		Fseek(m_fpDarc, m_DarcHeader.EntryOffset + current.EntryOffset * sizeof(SCommonEntry), SEEK_SET);
		fread(&current.Entry, sizeof(current.Entry), 1, m_fpDarc);
		current.IsDir = (current.Entry.NameOffset & 0xFF000000) != 0;
		current.Entry.NameOffset &= 0xFFFFFF;
		current.EntryName = U16ToU(reinterpret_cast<Char16_t*>(reinterpret_cast<u8*>(&*m_vEntryName.begin()) + current.Entry.NameOffset));
		current.ParentSiblingDirOffset = a_nParentSiblingDirOffset;
		current.Prefix = a_sPrefix;
		current.ExtractState = kExtractStateBegin;
	}
}

bool CDarc::extractDirEntry()
{
	SExtractStackElement& current = m_sExtractStack.top();
	if (current.ExtractState == kExtractStateBegin)
	{
		UString sPrefix = current.Prefix;
		UString sDirName = m_sDirName + sPrefix;
		if (current.EntryName != USTR(""))
		{
			sPrefix += current.EntryName + USTR("/");
			sDirName += current.EntryName;
		}
		else
		{
			sDirName.erase(sDirName.end() - 1);
		}
		if (m_bVerbose)
		{
			UPrintf(USTR("save: %") PRIUS USTR("\n"), sDirName.c_str());
		}
		if (!UMakeDir(sDirName))
		{
			m_sExtractStack.pop();
			return false;
		}
		if (current.EntryOffset + 1 == m_nEntryCount)
		{
			m_sExtractStack.pop();
			return true;
		}
		if (current.EntryOffset + 1 == current.ParentSiblingDirOffset)
		{
			m_sExtractStack.pop();
			return true;
		}
		if (current.EntryOffset + 1 != current.Entry.Entry.Dir.SiblingDirOffset)
		{
			pushExtractStackElement(current.EntryOffset + 1, current.Entry.Entry.Dir.SiblingDirOffset, sPrefix);
		}
		current.ExtractState = kExtractStateSiblingDir;
	}
	else if (current.ExtractState == kExtractStateSiblingDir)
	{
		if (current.Entry.Entry.Dir.SiblingDirOffset == m_nEntryCount)
		{
			m_sExtractStack.pop();
			return true;
		}
		if (current.Entry.Entry.Dir.SiblingDirOffset == current.ParentSiblingDirOffset)
		{
			m_sExtractStack.pop();
			return true;
		}
		pushExtractStackElement(current.Entry.Entry.Dir.SiblingDirOffset, current.ParentSiblingDirOffset, current.Prefix);
		current.ExtractState = kExtractStateEnd;
	}
	else if (current.ExtractState == kExtractStateEnd)
	{
		m_sExtractStack.pop();
	}
	return true;
}

bool CDarc::extractFileEntry()
{
	bool bResult = true;
	SExtractStackElement& current = m_sExtractStack.top();
	if (current.ExtractState == kExtractStateBegin)
	{
		UString sPath = m_sDirName + current.Prefix + current.EntryName;
		FILE* fp = UFopen(sPath, USTR("wb"));
		if (fp == nullptr)
		{
			bResult = false;
		}
		else
		{
			if (m_bVerbose)
			{
				UPrintf(USTR("save: %") PRIUS USTR("\n"), sPath.c_str());
			}
			CopyFile(fp, m_fpDarc, current.Entry.Entry.File.FileOffset, current.Entry.Entry.File.FileSize);
			fclose(fp);
		}
		if (current.EntryOffset + 1 == m_nEntryCount)
		{
			m_sExtractStack.pop();
			return bResult;
		}
		if (current.EntryOffset + 1 == current.ParentSiblingDirOffset)
		{
			m_sExtractStack.pop();
			return bResult;
		}
		pushExtractStackElement(current.EntryOffset + 1, current.ParentSiblingDirOffset, current.Prefix);
		current.ExtractState = kExtractStateEnd;
	}
	else if (current.ExtractState == kExtractStateEnd)
	{
		m_sExtractStack.pop();
	}
	return bResult;
}

void CDarc::setupCreate()
{
	m_DarcHeader.Signature = s_uSignature;
	m_DarcHeader.ByteOrder = 0xFEFF;
	m_DarcHeader.HeaderSize = sizeof(m_DarcHeader);
	m_DarcHeader.Version = 0x01000000;
	m_DarcHeader.EntryOffset = m_DarcHeader.HeaderSize;
}

void CDarc::buildIgnoreList()
{
	m_vIgnoreList.clear();
	UString sIgnorePath = UGetModuleDirName() + USTR("/ignore_darctool.txt");
	FILE* fp = UFopen(sIgnorePath, USTR("rb"));
	if (fp != nullptr)
	{
		Fseek(fp, 0, SEEK_END);
		u32 uSize = static_cast<u32>(Ftell(fp));
		Fseek(fp, 0, SEEK_SET);
		char* pTxt = new char[uSize + 1];
		fread(pTxt, 1, uSize, fp);
		fclose(fp);
		pTxt[uSize] = '\0';
		string sTxt(pTxt);
		delete[] pTxt;
		vector<string> vTxt = SplitOf(sTxt, "\r\n");
		for (vector<string>::const_iterator it = vTxt.begin(); it != vTxt.end(); ++it)
		{
			sTxt = Trim(*it);
			if (!sTxt.empty() && !StartWith(sTxt, "//"))
			{
				try
				{
					URegex black(AToU(sTxt), regex_constants::ECMAScript | regex_constants::icase);
					m_vIgnoreList.push_back(black);
				}
				catch (regex_error& e)
				{
					UPrintf(USTR("ERROR: %") PRIUS USTR("\n\n"), AToU(e.what()).c_str());
				}
			}
		}
	}
}

void CDarc::insertDirEntry(n32 a_nEntryOffset, const UString& a_sEntryName, n32 a_nParentDirOffset)
{
	m_vCreateEntry.resize(m_vCreateEntry.size() + 1);
	moveEntry(a_nEntryOffset);
	SEntry& currentEntry = m_vCreateEntry[a_nEntryOffset];
	if ((m_bExcludeRoot && a_nEntryOffset < 1) || (!m_bExcludeRoot && a_nEntryOffset < 2))
	{
		currentEntry.Path = m_sDirName;
	}
	else
	{
		currentEntry.Path = m_vCreateEntry[a_nParentDirOffset].Path + USTR("/") + a_sEntryName;
	}
	currentEntry.EntryName = UToU16(a_sEntryName);
	currentEntry.IsDir = true;
	currentEntry.ChildCount = 0;
	currentEntry.ChildDirOffset = s_nInvalidOffset;
	currentEntry.Entry.NameOffset = s_nInvalidOffset;
	currentEntry.Entry.Entry.Dir.ParentDirOffset = a_nParentDirOffset;
	currentEntry.Entry.Entry.Dir.SiblingDirOffset = s_nInvalidOffset;
	if (m_vCreateEntry[a_nParentDirOffset].ChildDirOffset != s_nInvalidOffset && a_nEntryOffset != m_vCreateEntry[a_nParentDirOffset].ChildDirOffset)
	{
		m_vCreateEntry[a_nEntryOffset - 1].Entry.Entry.Dir.SiblingDirOffset = a_nEntryOffset;
	}
}

bool CDarc::insertFileEntry(n32 a_nEntryOffset, const UString& a_sEntryName, n32 a_nParentDirOffset)
{
	bool bResult = true;
	m_vCreateEntry.resize(m_vCreateEntry.size() + 1);
	moveEntry(a_nEntryOffset);
	SEntry& currentEntry = m_vCreateEntry[a_nEntryOffset];
	currentEntry.Path = m_vCreateEntry[a_nParentDirOffset].Path + USTR("/") + a_sEntryName;
	currentEntry.EntryName = UToU16(a_sEntryName);
	currentEntry.IsDir = false;
	currentEntry.ChildCount = -1;
	currentEntry.ChildDirOffset = s_nInvalidOffset;
	currentEntry.Entry.NameOffset = s_nInvalidOffset;
	currentEntry.Entry.Entry.File.FileOffset = s_nInvalidOffset;
	n64 nFileSize = 0;
	if (!UGetFileSize(currentEntry.Path.c_str(), nFileSize))
	{
		bResult = false;
		UPrintf(USTR("ERROR: %") PRIUS USTR(" stat error\n\n"), currentEntry.Path.c_str());
	}
	else
	{
		currentEntry.Entry.Entry.File.FileSize = static_cast<n32>(nFileSize);
	}
	return bResult;
}

void CDarc::moveEntry(n32 a_nEntryOffset)
{
	for (n32 i = static_cast<n32>(m_vCreateEntry.size()) - 1; i > a_nEntryOffset; i--)
	{
		m_vCreateEntry[i] = m_vCreateEntry[i - 1];
	}
	for (n32 i = 0; i < static_cast<n32>(m_vCreateEntry.size()); i++)
	{
		if (i != a_nEntryOffset && m_vCreateEntry[i].IsDir)
		{
			addDirOffset(m_vCreateEntry[i].ChildDirOffset, a_nEntryOffset);
			addDirOffset(m_vCreateEntry[i].Entry.Entry.Dir.ParentDirOffset, a_nEntryOffset);
			addDirOffset(m_vCreateEntry[i].Entry.Entry.Dir.SiblingDirOffset, a_nEntryOffset);
		}
	}
	for (deque<SCreateDequeElement>::iterator it = m_dCreateDeque.begin(); it != m_dCreateDeque.end(); ++it)
	{
		SCreateDequeElement& current = *it;
		for (vector<n32>::iterator itOffset = current.ChildOffset.begin(); itOffset != current.ChildOffset.end(); ++itOffset)
		{
			n32& nOffset = *itOffset;
			addDirOffset(nOffset, a_nEntryOffset);
		}
	}
}

void CDarc::addDirOffset(n32& a_nOffset, n32 a_nIndex)
{
	if (a_nOffset >= a_nIndex)
	{
		a_nOffset++;
	}
}

void CDarc::pushCreateDequeElement(n32 a_nEntryOffset)
{
	m_dCreateDeque.push_back(SCreateDequeElement());
	SCreateDequeElement& current = m_dCreateDeque.back();
	current.EntryOffset = a_nEntryOffset;
	if (!m_bExcludeRoot && a_nEntryOffset == 0)
	{
		current.ChildOffset.push_back(1);
		current.ChildIndex = 0;
	}
	else
	{
		current.ChildIndex = -1;
	}
}

bool CDarc::createEntryList()
{
	bool bResult = true;
	SCreateDequeElement& current = m_dCreateDeque.back();
	if (current.ChildIndex == -1)
	{
#if SDW_PLATFORM == SDW_PLATFORM_WINDOWS
		WIN32_FIND_DATAW ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		wstring sPattern = m_vCreateEntry[current.EntryOffset].Path + L"/*";
		hFind = FindFirstFileW(sPattern.c_str(), &ffd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			map<wstring, wstring> mDir;
			map<wstring, wstring> mFile;
			do
			{
				if (matchInIgnoreList(m_vCreateEntry[current.EntryOffset].Path.substr(m_sDirName.size()) + L"/" + ffd.cFileName))
				{
					continue;
				}
				wstring sNameUpper = ffd.cFileName;
				transform(sNameUpper.begin(), sNameUpper.end(), sNameUpper.begin(), ::toupper);
				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					mFile.insert(make_pair(sNameUpper, ffd.cFileName));
				}
				else if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0)
				{
					mDir.insert(make_pair(sNameUpper, ffd.cFileName));
				}
			} while (FindNextFileW(hFind, &ffd) != 0);
			for (map<wstring, wstring>::const_iterator it = mFile.begin(); it != mFile.end(); ++it)
			{
				m_vCreateEntry[current.EntryOffset].ChildCount++;
				if (!insertFileEntry(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount, it->second, current.EntryOffset))
				{
					bResult = false;
				}
			}
			for (map<wstring, wstring>::const_iterator it = mDir.begin(); it != mDir.end(); ++it)
			{
				m_vCreateEntry[current.EntryOffset].ChildCount++;
				insertDirEntry(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount, it->second, current.EntryOffset);
				if (m_vCreateEntry[current.EntryOffset].ChildDirOffset == s_nInvalidOffset)
				{
					m_vCreateEntry[current.EntryOffset].ChildDirOffset = current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount;
				}
				current.ChildOffset.push_back(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount);
			}
		}
#else
		DIR* pDir = opendir(m_vCreateEntry[current.EntryOffset].Path.c_str());
		if (pDir != nullptr)
		{
			map<string, string> mDir;
			map<string, string> mFile;
			dirent* pDirent = nullptr;
			while ((pDirent = readdir(pDir)) != nullptr)
			{
				if (matchInIgnoreList(m_vCreateEntry[current.EntryOffset].Path.substr(m_sDirName.size()) + "/" + pDirent->d_name))
				{
					continue;
				}
				string sNameUpper = pDirent->d_name;
				transform(sNameUpper.begin(), sNameUpper.end(), sNameUpper.begin(), ::toupper);
				if (pDirent->d_type == DT_REG)
				{
					mFile.insert(make_pair(sNameUpper, pDirent->d_name));
				}
				else if (pDirent->d_type == DT_DIR && strcmp(pDirent->d_name, ".") != 0 && strcmp(pDirent->d_name, "..") != 0)
				{
					mDir.insert(make_pair(sNameUpper, pDirent->d_name));
				}
			}
			closedir(pDir);
			for (map<string, string>::const_iterator it = mFile.begin(); it != mFile.end(); ++it)
			{
				m_vCreateEntry[current.EntryOffset].ChildCount++;
				if (!insertFileEntry(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount, it->second, current.EntryOffset))
				{
					bResult = false;
				}
			}
			for (map<string, string>::const_iterator it = mDir.begin(); it != mDir.end(); ++it)
			{
				m_vCreateEntry[current.EntryOffset].ChildCount++;
				insertDirEntry(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount, it->second, current.EntryOffset);
				if (m_vCreateEntry[current.EntryOffset].ChildDirOffset == s_nInvalidOffset)
				{
					m_vCreateEntry[current.EntryOffset].ChildDirOffset = current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount;
				}
				current.ChildOffset.push_back(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount);
			}
		}
#endif
		current.ChildIndex = 0;
	}
	else if (current.ChildIndex != current.ChildOffset.size())
	{
		pushCreateDequeElement(current.ChildOffset[current.ChildIndex++]);
	}
	else
	{
		m_dCreateDeque.pop_back();
	}
	return bResult;
}

bool CDarc::matchInIgnoreList(const UString& a_sPath) const
{
	bool bMatch = false;
	for (vector<URegex>::const_iterator it = m_vIgnoreList.begin(); it != m_vIgnoreList.end(); ++it)
	{
		if (regex_search(a_sPath, *it))
		{
			bMatch = true;
			break;
		}
	}
	return bMatch;
}

void CDarc::redirectSiblingDirOffset()
{
	m_nEntryCount = static_cast<n32>(m_vCreateEntry.size());
	m_vCreateEntry[0].Entry.Entry.Dir.SiblingDirOffset = m_nEntryCount;
	for (vector<SEntry>::iterator it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		SEntry& currentEntry = *it;
		if (currentEntry.IsDir && currentEntry.Entry.Entry.Dir.SiblingDirOffset == s_nInvalidOffset)
		{
			currentEntry.Entry.Entry.Dir.SiblingDirOffset = m_vCreateEntry[currentEntry.Entry.Entry.Dir.ParentDirOffset].Entry.Entry.Dir.SiblingDirOffset;
		}
	}
}

void CDarc::createEntryName()
{
	for (vector<SEntry>::iterator it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		SEntry& currentEntry = *it;
		currentEntry.Entry.NameOffset = static_cast<n32>(m_vEntryName.size() * 2);
		m_vEntryName.resize(m_vEntryName.size() + currentEntry.EntryName.size() + 1);
		if (currentEntry.EntryName.size() != 0)
		{
			memcpy(reinterpret_cast<u8*>(&*m_vEntryName.begin()) + currentEntry.Entry.NameOffset, &*currentEntry.EntryName.begin(), currentEntry.EntryName.size() * 2);
		}
		if (currentEntry.IsDir)
		{
			currentEntry.Entry.NameOffset |= 0x01000000;
		}
	}
	m_DarcHeader.EntrySize = static_cast<u32>(m_vCreateEntry.size() * sizeof(SCommonEntry) + m_vEntryName.size() * 2);
	m_DarcHeader.FileSize = static_cast<u32>(Align(m_DarcHeader.EntryOffset + m_DarcHeader.EntrySize, 4));
	if (m_bUseHeaderSize)
	{
		m_DarcHeader.DataOffset = m_DarcHeader.FileSize;
	}
}

void CDarc::calculateFileOffset()
{
	map<UString, n32> mFileOffset;
	for (n32 i = 0; i < static_cast<n32>(m_vCreateEntry.size()); i++)
	{
		SEntry& currentEntry = m_vCreateEntry[i];
		if (!currentEntry.IsDir)
		{
			UString sNameUpper = currentEntry.Path;
			transform(sNameUpper.begin(), sNameUpper.end(), sNameUpper.begin(), ::toupper);
			mFileOffset.insert(make_pair(sNameUpper, i));
		}
	}
	for (map<UString, n32>::iterator it = mFileOffset.begin(); it != mFileOffset.end(); ++it)
	{
		SEntry& currentEntry = m_vCreateEntry[it->second];
		n32 nEntryAlignment = getAlignment(currentEntry.Path.substr(m_sDirName.size()));
		currentEntry.Entry.Entry.File.FileOffset = static_cast<n32>(Align(m_DarcHeader.FileSize, nEntryAlignment));
		m_DarcHeader.FileSize = currentEntry.Entry.Entry.File.FileOffset + currentEntry.Entry.Entry.File.FileSize;
		if (m_DarcHeader.DataOffset == 0)
		{
			m_DarcHeader.DataOffset = currentEntry.Entry.Entry.File.FileOffset;
		}
	}
	if (m_DarcHeader.DataOffset == 0)
	{
		m_DarcHeader.DataOffset = m_DarcHeader.FileSize;
	}
}

n32 CDarc::getAlignment(const UString& a_sPath) const
{
	for (map<n32, vector<URegex>>::const_reverse_iterator it = m_mUniqueAlignment.rbegin(); it != m_mUniqueAlignment.rend(); ++it)
	{
		n32 nAlignment = it->first;
		const vector<URegex>& vRegex = it->second;
		for (vector<URegex>::const_iterator itRegex = vRegex.begin(); itRegex != vRegex.end(); ++itRegex)
		{
			if (regex_search(a_sPath, *itRegex))
			{
				return nAlignment;
			}
		}
	}
	return m_nSharedAlignment;
}

void CDarc::writeHeader()
{
	Fseek(m_fpDarc, 0, SEEK_SET);
	fwrite(&m_DarcHeader, sizeof(m_DarcHeader), 1, m_fpDarc);
}

void CDarc::writeEntry()
{
	Fseek(m_fpDarc, m_DarcHeader.EntryOffset, SEEK_SET);
	for (vector<SEntry>::const_iterator it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		const SEntry& currentEntry = *it;
		fwrite(&currentEntry.Entry, sizeof(currentEntry.Entry), 1, m_fpDarc);
	}
	fwrite(&*m_vEntryName.begin(), 2, m_vEntryName.size(), m_fpDarc);
}

bool CDarc::writeData()
{
	bool bResult = true;
	for (vector<SEntry>::const_iterator it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		const SEntry& currentEntry = *it;
		if (!currentEntry.IsDir)
		{
			if (!writeFromFile(currentEntry.Path, currentEntry.Entry.Entry.File.FileOffset, currentEntry.Entry.Entry.File.FileSize))
			{
				bResult = false;
			}
		}
	}
	return bResult;
}

bool CDarc::writeFromFile(const UString& a_sPath, n32 a_nOffset, n32 a_nSize)
{
	FILE* fp = UFopen(a_sPath, USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	if (m_bVerbose)
	{
		UPrintf(USTR("load: %") PRIUS USTR("\n"), a_sPath.c_str());
	}
	Fseek(m_fpDarc, a_nOffset, SEEK_SET);
	CopyFile(m_fpDarc, fp, 0, a_nSize);
	fclose(fp);
	return true;
}
