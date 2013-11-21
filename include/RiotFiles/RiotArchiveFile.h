#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

//#include "MMFile.h"

class MMFile;

namespace RAF
{
    enum  {
        MagicNumber = 0x18BE0EF0,
        Version = 0x1,
    };
    // Header that appears at start of the directory file
    struct Header_t
    {
        // Magic value used to identify the file type, must be 0x18BE0EF0
        unsigned long	mMagic;

        // Version of the archive format, must be 1
        unsigned long	mVersion;
    };

    // Table of contents appears directly after header
    struct TableOfContents_t
    {
        // An index that is used by the runtime, do not modify
        unsigned long	mMgrIndex;

        // Offset to the file list from the beginning of the file
        unsigned long	mFileListOffset;

        // Offset to the string table from the beginning of the file
        unsigned long	mStringTableOffset;
    };

    // Header of the file list
    struct FileListHeader_t
    {
        // Number of entries in the list
        unsigned long	mCount;
    };

    // An entry in the file list describes a file that has been archived
    struct FileListEntry_t
    {
        // Hash of the string name
        unsigned long	mHash;

        // Offset to the start of the archived file in the data file
        unsigned long	mOffset;

        // Size of this archived file
        unsigned long	mSize;

        // Index of the name of the archvied file in the string table
        unsigned long	mFileNameStringTableIndex;
    };
}

namespace StringTable
{
    // First structure in the built string table
    struct HEADER
    {
        // size of all data including header
        unsigned int
        m_Size;

        // Number of strings in the table
        unsigned int
            m_Count;
    };

    // Entry in the table of contents
    struct ENTRY
    {
        // Offset from START OF THE STRING TABLE 
        // (i.e. offset from the start of the header structure)
        unsigned int
        m_Offset;

        // Size of string including null
        unsigned int
            m_Size;
    };
}

class RiotArchiveFileException : public std::runtime_error{
public:
    RiotArchiveFileException(const std::string& msg) : runtime_error(msg) {}    
};



class RiotArchiveFile
{
    std::string path;
    std::unique_ptr<MMFile> directoryFile;
    std::unique_ptr<MMFile> archiveFile;

    RAF::Header_t* header;
    RAF::TableOfContents_t* TOC;
    RAF::FileListHeader_t* fileListHeader;
    RAF::FileListEntry_t* fileListEntries;
    StringTable::HEADER* stringListHeader;
    StringTable::ENTRY* stringListEntries;
        

public:
    RiotArchiveFile(const std::string& path);
    ~RiotArchiveFile();

    void dispose();

    size_t getFileCount() const {
        return fileListHeader->mCount;
    }

    std::string getFileName(size_t fileIdx) const;
    std::string getString(size_t stringIdx) const;

    bool hasFile(const std::string& path) const;
    size_t getFileIndex(const std::string& path) const;
    std::vector<char> getFileContents(size_t fileIdx) const;


private:
    void load(const std::string& archivePath);
};

