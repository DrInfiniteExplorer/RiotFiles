#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>

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

namespace RAF {
    enum {
        MinDirectorySize = sizeof(RAF::Header_t)+sizeof(RAF::TableOfContents_t)+sizeof(RAF::FileListHeader_t)+sizeof(StringTable::HEADER)
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
    mutable std::unique_ptr<MMFile> archiveFile;

    void openArchive() const;

    RAF::Header_t* header;
    RAF::TableOfContents_t* TOC;
    RAF::FileListHeader_t* fileListHeader;
    RAF::FileListEntry_t* fileListEntries;
    StringTable::HEADER* stringListHeader;
    StringTable::ENTRY* stringListEntries;

protected:
    RiotArchiveFile();
public:
    RiotArchiveFile(const std::string& path);
    virtual ~RiotArchiveFile();

    static void createEmptyFile(const std::string& path);
    static bool couldBeRAF(const std::string& path);

    virtual void dispose();

    //Closes the archive file (.dat) if open; opened by reading content of a file in the archive.
    virtual void closeArchiveFile() const;

    virtual size_t getFileCount() const {
        return fileListHeader->mCount;
    }
    virtual size_t getStringCount() const {
        return stringListHeader->m_Count;
    }

    virtual std::string getFileName(size_t fileIdx) const;
    virtual std::string getString(size_t stringIdx) const;

    virtual bool hasFile(const std::string& path) const;
    virtual size_t getFileIndex(const std::string& path) const;
    virtual size_t getFileSize(size_t fileIdx) const;
    virtual std::vector<char> getFileContents(size_t fileIdx) const;

    virtual void extractFile(size_t fileIdx, const std::string& outPath) const;
    virtual void unpackArchive(const std::string& outPath) const;


private:
    void load(const std::string& archivePath);

private:
    std::set<std::string> removeList;
    struct AddInfo {
        std::string sourcePath;
        std::string archivePath;
    };
    std::map<std::string, AddInfo> addList;

    struct NewFileEntry {
        NewFileEntry(std::string p) {
            hash = RiotArchiveFile::hashString(p);
            archivePath = p;
        }
        std::string archivePath;
        unsigned int offset;
        unsigned int size;
        unsigned int hash;
    };

    static std::string sanitize(const std::string& path);
public:
    static unsigned int hashString(std::string str);
    void apply();
    void discard();

    void addFile(const std::string& archivePath, const std::string& filePath);
    void removeFile(const std::string& archivePath);
};


class RiotArchiveFileCollection : public RiotArchiveFile {
    bool buildIndex;
public:
    RiotArchiveFileCollection(bool buildIndex);
    virtual ~RiotArchiveFileCollection() { dispose(); }


    virtual void dispose() override;

    virtual void closeArchiveFile() const override;

    virtual size_t getFileCount() const override;
    virtual size_t getStringCount() const override;
    virtual std::string getFileName(size_t fileIdx) const override;
    virtual std::string getString(size_t stringIdx) const override;

    virtual bool hasFile(const std::string& path) const override;
    virtual size_t getFileIndex(const std::string& path) const override;
    virtual std::vector<char> getFileContents(size_t fileIdx) const override;
    virtual void extractFile(size_t fileIdx, const std::string& outPath) const override;
    virtual void unpackArchive(const std::string& outPath) const override; // Tries to closeArchiveFile if cant map files.

    void addArchive(const std::string& path);
    std::map<std::string, RiotArchiveFile*> archivesNamed;
    std::vector<RiotArchiveFile*> archives;
    
    //void addArchive(RiotArchiveFile* archive); // Not implemented, not sure about how to handle ownership.
};

