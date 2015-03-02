#include "darc.h"

const u32 CDarc::s_uSignature = CONVERT_ENDIAN('darc');
const n32 CDarc::s_nInvalidOffset = -1;
const int CDarc::s_nEntryAlignment4 = 4;
const int CDarc::s_nEntryAlignment128 = 128;

CDarc::CDarc()
	: m_pFileName(nullptr)
	, m_bVerbose(false)
	, m_fpDarc(nullptr)
	, m_nEntryCount(0)
{
	memset(&m_DarcHeader, 0, sizeof(m_DarcHeader));
}

CDarc::~CDarc()
{
}

void CDarc::SetFileName(const char* a_pFileName)
{
	m_pFileName = a_pFileName;
}

void CDarc::SetDirName(const char* a_pRomFsDirName)
{
	m_sDirName = FSAToUnicode(a_pRomFsDirName);
}

void CDarc::SetVerbose(bool a_bVerbose)
{
	m_bVerbose = a_bVerbose;
}

bool CDarc::ExtractFile()
{
	bool bResult = true;
	m_fpDarc = FFopen(m_pFileName, "rb");
	if (m_fpDarc == nullptr)
	{
		return false;
	}
	fread(&m_DarcHeader, sizeof(m_DarcHeader), 1, m_fpDarc);
	FFseek(m_fpDarc, m_DarcHeader.EntryOffset, SEEK_SET);
	SCommonEntry rootEntry;
	fread(&rootEntry, sizeof(rootEntry), 1, m_fpDarc);
	m_nEntryCount = rootEntry.Entry.Dir.SiblingDirOffset;
	n32 nNameOffset = m_DarcHeader.EntryOffset + m_nEntryCount * sizeof(SCommonEntry);
	n32 nNameSize = (m_DarcHeader.EntryOffset + m_DarcHeader.EntrySize - nNameOffset) / sizeof(char16_t);
	m_vEntryName.resize(nNameSize);
	FFseek(m_fpDarc, nNameOffset, SEEK_SET);
	fread(&*m_vEntryName.begin(), sizeof(char16_t), nNameSize, m_fpDarc);
	pushExtractStackElement(0, m_nEntryCount, STR("/"));
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
	buildBlackList();
	buildAlignList();
	insertDirEntry(0, STR(""), 0);
	insertDirEntry(1, STR("."), 0);
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
	m_fpDarc = FFopen(m_pFileName, "wb");
	if (m_fpDarc == nullptr)
	{
		return false;
	}
	FSeek(m_fpDarc, m_DarcHeader.FileSize);
	writeHeader();
	writeEntry();
	if (!writeData())
	{
		bResult = false;
	}
	fclose(m_fpDarc);
	return bResult;
}

bool CDarc::IsDarcFile(const char* a_pFileName)
{
	FILE* fp = FFopen(a_pFileName, "rb");
	if (fp == nullptr)
	{
		return false;
	}
	SDarcHeader darcHeader;
	fread(&darcHeader, sizeof(darcHeader), 1, fp);
	fclose(fp);
	return darcHeader.Signature == s_uSignature;
}

void CDarc::pushExtractStackElement(n32 a_nEntryOffset, n32 a_nParentSiblingDirOffset, const String& a_sPrefix)
{
	if (a_nEntryOffset != m_nEntryCount)
	{
		m_sExtractStack.push(SExtractStackElement());
		SExtractStackElement& current = m_sExtractStack.top();
		current.EntryOffset = a_nEntryOffset;
		FFseek(m_fpDarc, m_DarcHeader.EntryOffset + current.EntryOffset * sizeof(SCommonEntry), SEEK_SET);
		fread(&current.Entry, sizeof(current.Entry), 1, m_fpDarc);
		current.IsDir = (current.Entry.NameOffset & 0xFF000000) != 0;
		current.Entry.NameOffset &= 0xFFFFFF;
		current.EntryName = FSU16ToUnicode(reinterpret_cast<char16_t*>(reinterpret_cast<u8*>(&*m_vEntryName.begin()) + current.Entry.NameOffset));
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
		String sPrefix = current.Prefix;
		String sDirName = m_sDirName + sPrefix;
		if (current.EntryName != STR(""))
		{
			sPrefix += current.EntryName + STR("/");
			sDirName += current.EntryName;
		}
		else
		{
			sDirName.erase(sDirName.end() - 1);
		}
		if (m_bVerbose)
		{
			FPrintf(STR("save: %s\n"), sDirName.c_str());
		}
		if (!FMakeDir(sDirName.c_str()))
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
		String sPath = m_sDirName + current.Prefix + current.EntryName;
		FILE* fp = FFopenUnicode(sPath.c_str(), STR("wb"));
		if (fp == nullptr)
		{
			bResult = false;
		}
		else
		{
			if (m_bVerbose)
			{
				FPrintf(STR("save: %s\n"), sPath.c_str());
			}
			FCopyFile(fp, m_fpDarc, current.Entry.Entry.File.FileOffset, current.Entry.Entry.File.FileSize);
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
	memset(&m_DarcHeader, 0, sizeof(m_DarcHeader));
	m_DarcHeader.Signature = s_uSignature;
	m_DarcHeader.ByteOrder = 0xFEFF;
	m_DarcHeader.HeaderSize = sizeof(m_DarcHeader);
	m_DarcHeader.Version = 0x01000000;
	m_DarcHeader.EntryOffset = m_DarcHeader.HeaderSize;
}

void CDarc::buildBlackList()
{
	m_vBlackList.clear();
	String sIgnorePath = FGetModuleDir() + STR("/ignore_darc.txt");
	FILE* fp = FFopenUnicode(sIgnorePath.c_str(), STR("rb"));
	if (fp != nullptr)
	{
		FFseek(fp, 0, SEEK_END);
		u32 nSize = static_cast<u32>(FFtell(fp));
		FFseek(fp, 0, SEEK_SET);
		char* pTxt = new char[nSize + 1];
		fread(pTxt, 1, nSize, fp);
		fclose(fp);
		pTxt[nSize] = '\0';
		string sTxt(pTxt);
		delete[] pTxt;
		vector<string> vTxt = FSSplitOf<string>(sTxt, "\r\n");
		for (auto it = vTxt.begin(); it != vTxt.end(); ++it)
		{
			sTxt = FSTrim(*it);
			if (!sTxt.empty() && !FSStartsWith<string>(sTxt, "//"))
			{
				try
				{
					Regex black(FSAToUnicode(sTxt), regex_constants::ECMAScript | regex_constants::icase);
					m_vBlackList.push_back(black);
				}
				catch (regex_error& e)
				{
					printf("ERROR: %s\n\n", e.what());
				}
			}
		}
	}
}

void CDarc::buildAlignList()
{
	m_vAlignList.clear();
	String sAlignPath = FGetModuleDir() + STR("/align_darc.txt");
	FILE* fp = FFopenUnicode(sAlignPath.c_str(), STR("rb"));
	if (fp != nullptr)
	{
		FFseek(fp, 0, SEEK_END);
		u32 nSize = static_cast<u32>(FFtell(fp));
		FFseek(fp, 0, SEEK_SET);
		char* pTxt = new char[nSize + 1];
		fread(pTxt, 1, nSize, fp);
		fclose(fp);
		pTxt[nSize] = '\0';
		string sTxt(pTxt);
		delete[] pTxt;
		vector<string> vTxt = FSSplitOf<string>(sTxt, "\r\n");
		for (auto it = vTxt.begin(); it != vTxt.end(); ++it)
		{
			sTxt = FSTrim(*it);
			if (!sTxt.empty() && !FSStartsWith<string>(sTxt, "//"))
			{
				try
				{
					Regex align(FSAToUnicode(sTxt), regex_constants::ECMAScript | regex_constants::icase);
					m_vAlignList.push_back(align);
				}
				catch (regex_error& e)
				{
					printf("ERROR: %s\n\n", e.what());
				}
			}
		}
	}
}

void CDarc::insertDirEntry(n32 a_nEntryOffset, const String& a_sEntryName, n32 a_nParentDirOffset)
{
	m_vCreateEntry.resize(m_vCreateEntry.size() + 1);
	moveEntry(a_nEntryOffset);
	SEntry& currentEntry = m_vCreateEntry[a_nEntryOffset];
	if (a_nEntryOffset < 2)
	{
		currentEntry.Path = m_sDirName;
	}
	else
	{
		currentEntry.Path = m_vCreateEntry[a_nParentDirOffset].Path + STR("/") + a_sEntryName;
	}
	currentEntry.EntryName = FSUnicodeToU16(a_sEntryName);
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

bool CDarc::insertFileEntry(n32 a_nEntryOffset, const String& a_sEntryName, n32 a_nParentDirOffset)
{
	bool bResult = true;
	m_vCreateEntry.resize(m_vCreateEntry.size() + 1);
	moveEntry(a_nEntryOffset);
	SEntry& currentEntry = m_vCreateEntry[a_nEntryOffset];
	currentEntry.Path = m_vCreateEntry[a_nParentDirOffset].Path + STR("/") + a_sEntryName;
	currentEntry.EntryName = FSUnicodeToU16(a_sEntryName);
	currentEntry.IsDir = false;
	currentEntry.ChildCount = -1;
	currentEntry.ChildDirOffset = s_nInvalidOffset;
	currentEntry.Entry.NameOffset = s_nInvalidOffset;
	currentEntry.Entry.Entry.File.FileOffset = s_nInvalidOffset;
	if (!FGetFileSize32(currentEntry.Path.c_str(), currentEntry.Entry.Entry.File.FileSize))
	{
		bResult = false;
		FPrintf(STR("ERROR: %s stat error\n\n"), currentEntry.Path.c_str());
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
	for (auto it = m_dCreateDeque.begin(); it != m_dCreateDeque.end(); ++it)
	{
		SCreateDequeElement& current = *it;
		for (auto it2 = current.ChildOffset.begin(); it2 != current.ChildOffset.end(); ++it2)
		{
			n32& nOffset = *it2;
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
	if (a_nEntryOffset == 0)
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
#if DARCTOOL_COMPILER == COMPILER_MSC
		WIN32_FIND_DATAW ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		wstring sPattern = m_vCreateEntry[current.EntryOffset].Path + L"/*";
		hFind = FindFirstFileW(sPattern.c_str(), &ffd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			map<String, String> mDir;
			map<String, String> mFile;
			do
			{
				if (matchInBlackList(m_vCreateEntry[current.EntryOffset].Path.substr(m_sDirName.size()) + STR("/") + ffd.cFileName))
				{
					continue;
				}
				String nameUpper = ffd.cFileName;
				transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);
				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					mFile.insert(make_pair(nameUpper, ffd.cFileName));
				}
				else if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0)
				{
					mDir.insert(make_pair(nameUpper, ffd.cFileName));
				}
			} while (FindNextFileW(hFind, &ffd) != 0);
			for (auto it = mFile.begin(); it != mFile.end(); ++it)
			{
				m_vCreateEntry[current.EntryOffset].ChildCount++;
				if (!insertFileEntry(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount, it->second, current.EntryOffset))
				{
					bResult = false;
				}
			}
			for (auto it = mDir.begin(); it != mDir.end(); ++it)
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
				if (matchInBlackList(m_vCreateEntry[current.EntryOffset].Path.substr(m_sDirName.size()) + STR("/") + pDirent->d_name))
				{
					continue;
				}
				string nameUpper = pDirent->d_name;
				transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);
				if (pDirent->d_type == DT_REG)
				{
					mFile.insert(make_pair(nameUpper, pDirent->d_name));
				}
				else if (pDirent->d_type == DT_DIR && strcmp(pDirent->d_name, ".") != 0 && strcmp(pDirent->d_name, "..") != 0)
				{
					mDir.insert(make_pair(nameUpper, pDirent->d_name));
				}
			}
			closedir(pDir);
			for (auto it = mFile.begin(); it != mFile.end(); ++it)
			{
				m_vCreateEntry[current.EntryOffset].ChildCount++;
				if (!insertFileEntry(current.EntryOffset + m_vCreateEntry[current.EntryOffset].ChildCount, it->second, current.EntryOffset))
				{
					bResult = false;
				}
			}
			for (auto it = mDir.begin(); it != mDir.end(); ++it)
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

bool CDarc::matchInBlackList(const String& a_sPath)
{
	bool bMatch = false;
	for (auto it = m_vBlackList.begin(); it != m_vBlackList.end(); ++it)
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
	for (auto it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
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
	for (auto it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		SEntry& currentEntry = *it;
		currentEntry.Entry.NameOffset = static_cast<n32>(m_vEntryName.size() * sizeof(char16_t));
		m_vEntryName.resize(m_vEntryName.size() + currentEntry.EntryName.size() + 1);
		if (currentEntry.EntryName.size() != 0)
		{
			memcpy(reinterpret_cast<u8*>(&*m_vEntryName.begin()) + currentEntry.Entry.NameOffset, &*currentEntry.EntryName.begin(), currentEntry.EntryName.size() * sizeof(char16_t));
		}
		if (currentEntry.IsDir)
		{
			currentEntry.Entry.NameOffset |= 0x01000000;
		}
	}
	m_DarcHeader.EntrySize = static_cast<u32>(m_vCreateEntry.size() * sizeof(SCommonEntry) + m_vEntryName.size() * sizeof(char16_t));
	m_DarcHeader.FileSize = static_cast<u32>(FAlign(m_DarcHeader.EntryOffset + m_DarcHeader.EntrySize, 4));
}

void CDarc::calculateFileOffset()
{
	map<String, int> mFileOffset;
	for (int i = 0; i < static_cast<int>(m_vCreateEntry.size()); i++)
	{
		SEntry& currentEntry = m_vCreateEntry[i];
		if (!currentEntry.IsDir)
		{
			String nameUpper = currentEntry.Path;
			transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);
			mFileOffset.insert(make_pair(nameUpper, i));
		}
	}
	for (auto it = mFileOffset.begin(); it != mFileOffset.end(); ++it)
	{
		SEntry& currentEntry = m_vCreateEntry[it->second];
		int nEntryAlignment = s_nEntryAlignment4;
		if (matchInAlignList(currentEntry.Path.substr(m_sDirName.size())))
		{
			nEntryAlignment = s_nEntryAlignment128;
		}
		currentEntry.Entry.Entry.File.FileOffset = static_cast<n32>(FAlign(m_DarcHeader.FileSize, nEntryAlignment));
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

bool CDarc::matchInAlignList(const String& a_sPath)
{
	bool bMatch = false;
	for (auto it = m_vAlignList.begin(); it != m_vAlignList.end(); ++it)
	{
		if (regex_search(a_sPath, *it))
		{
			bMatch = true;
			break;
		}
	}
	return bMatch;
}

void CDarc::writeHeader()
{
	FFseek(m_fpDarc, 0, SEEK_SET);
	fwrite(&m_DarcHeader, sizeof(m_DarcHeader), 1, m_fpDarc);
}

void CDarc::writeEntry()
{
	FFseek(m_fpDarc, m_DarcHeader.EntryOffset, SEEK_SET);
	for (auto it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		SEntry& currentEntry = *it;
		fwrite(&currentEntry.Entry, sizeof(currentEntry.Entry), 1, m_fpDarc);
	}
	fwrite(&*m_vEntryName.begin(), sizeof(char16_t), m_vEntryName.size(), m_fpDarc);
}

bool CDarc::writeData()
{
	bool bResult = true;
	for (auto it = m_vCreateEntry.begin(); it != m_vCreateEntry.end(); ++it)
	{
		SEntry& currentEntry = *it;
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

bool CDarc::writeFromFile(const String& a_sPath, n32 a_nOffset, n32 a_nSize)
{
	FILE* fp = FFopenUnicode(a_sPath.c_str(), STR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	if (m_bVerbose)
	{
		FPrintf(STR("load: %s\n"), a_sPath.c_str());
	}
	FFseek(m_fpDarc, a_nOffset, SEEK_SET);
	FCopyFile(m_fpDarc, fp, 0, a_nSize);
	fclose(fp);
	return true;
}
