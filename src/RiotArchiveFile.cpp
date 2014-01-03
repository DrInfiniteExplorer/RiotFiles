#include "RiotFiles\RiotArchiveFile.h"
#include "RiotFiles\MMFile.h"

#include "zlib\zlib.h"
#include <algorithm>
#include <iostream>
#include <fstream>

#include <ShlObj.h>

#define STRINGIZE_UGH(X) #X
#define STRINGIZE(X) STRINGIZE_UGH(X)

#define RAFenforce(cond, msg) if(!(cond)) { throw RiotArchiveFileException(std::string(__FILE__ " " STRINGIZE(__LINE__) ": ") + (msg)); }

bool compare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (unsigned int i = 0; i < a.size(); i++) {
        if (tolower(a[i]) != tolower(b[i])) {
            return false;
        }
    }
    return true;
}


RiotArchiveFile::RiotArchiveFile(){
}

RiotArchiveFile::RiotArchiveFile(const std::string& path)
{
    load(path);
}


RiotArchiveFile::~RiotArchiveFile()
{
}

void RiotArchiveFile::createEmptyFile(const std::string& path) {
    auto totalSize = sizeof(RAF::Header_t) + sizeof(RAF::TableOfContents_t) +
        sizeof(RAF::FileListHeader_t) + sizeof(RAF::FileListEntry_t) + sizeof(StringTable::HEADER) + sizeof(StringTable::ENTRY);

    std::unique_ptr<MMFile> newDir;
    newDir.reset(new MMFile(path, MMOpenMode::readWrite, totalSize));

    RAF::Header_t* header;
    newDir->get(header, 0);

    header->mMagic = RAF::MagicNumber;
    header->mVersion = RAF::Version;

    RAF::TableOfContents_t* TOC;
    newDir->get(TOC, sizeof(RAF::Header_t));
    TOC->mMgrIndex = 0; //? :P
    TOC->mFileListOffset = sizeof(RAF::Header_t) + sizeof(RAF::TableOfContents_t);
    TOC->mStringTableOffset = sizeof(RAF::Header_t) + sizeof(RAF::TableOfContents_t) + sizeof(RAF::FileListHeader_t) + sizeof(RAF::FileListEntry_t);
    RAF::FileListHeader_t* fileHeader;
    newDir->get(fileHeader, TOC->mFileListOffset);
    fileHeader->mCount = 0;

    StringTable::HEADER* stringHeader;
    newDir->get(stringHeader, TOC->mStringTableOffset);
    stringHeader->m_Count = 0;
    stringHeader->m_Size = 0;

    std::unique_ptr<MMFile> arcDir;
    arcDir.reset(new MMFile(path + ".dat", MMOpenMode::readWrite, 1));
}

bool RiotArchiveFile::couldBeRAF(const std::string& path) {

    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (!GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &fileData)) {
        return false;
    }
    if (__int64(fileData.nFileSizeLow) + __int64(fileData.nFileSizeHigh) < RAF::MinDirectorySize) {
        return false;
    }

    std::unique_ptr<MMFile> directoryFile;
    directoryFile.reset(new MMFile(path, MMOpenMode::read, 0));
    RAF::Header_t* header;
    directoryFile->get(header, 0);

    if (header->mMagic != RAF::MagicNumber) {
        return false;
    }
    if (header->mVersion != RAF::Version) {
        return false;
    }

    return true;
}


void RiotArchiveFile::load(const std::string& archivePath)
{
    directoryFile.reset(new MMFile(archivePath, MMOpenMode::read, 0));

    directoryFile->get(header, 0);

    RAFenforce(header->mMagic == RAF::MagicNumber, "Bad magic number found in archive: " + archivePath);
    RAFenforce(header->mVersion == RAF::Version, "Bad version number found in archive: " + archivePath);


    directoryFile->get(TOC, sizeof(RAF::Header_t));
    directoryFile->get(fileListHeader, TOC->mFileListOffset);
    directoryFile->get(stringListHeader, TOC->mStringTableOffset);

    RAFenforce(fileListHeader->mCount == stringListHeader->m_Count, "Number of files and number of strings not matching, cant handle this crap!");
    directoryFile->get(fileListEntries, TOC->mFileListOffset + sizeof(RAF::FileListHeader_t));
    directoryFile->get(stringListEntries, TOC->mStringTableOffset + sizeof(StringTable::HEADER));


    if (fileListHeader->mCount) {
        auto arcPath = archivePath + ".dat";
        WIN32_FILE_ATTRIBUTE_DATA arcData;
        RAFenforce(GetFileAttributesEx(arcPath.c_str(), GetFileExInfoStandard, &arcData), "Could not obtain size of .dat file!");
    }

    path = archivePath;
}

void RiotArchiveFile::dispose() {
    directoryFile.reset();
    archiveFile.reset();

}

void RiotArchiveFile::closeArchiveFile() const {
    archiveFile.reset(nullptr);
}

std::string RiotArchiveFile::getFileName(size_t fileIdx) const {
    RAFenforce(fileIdx < fileListHeader->mCount, "Bad fileIdx supplied to getFileName");
    auto entry = fileListEntries + fileIdx;
    return getString(entry->mFileNameStringTableIndex);
}

std::string RiotArchiveFile::getString(size_t stringIdx) const {
    RAFenforce(stringIdx < stringListHeader->m_Count, "Bad stringIdx supplied to getString");
    auto entry = stringListEntries + stringIdx;
    char* basePtr = (char*)stringListHeader;
    return std::string(basePtr + entry->m_Offset);
}

const std::string getZLibError(int error) {
    switch (error) {
    case Z_OK: return "Z_OK: No error";
    case Z_STREAM_END: return "Z_STREAM_END: The stream ended";
    case Z_NEED_DICT: return "Z_NEED_DICT: Needs a dictionary";
    case Z_ERRNO: return "Z_ERRNO: No idea";
    case Z_STREAM_ERROR: return "Z_STREAM_ERROR: No idea";
    case Z_DATA_ERROR: return "Z_DATA_ERROR: No idea";
    case Z_MEM_ERROR: return "Z_MEM_ERROR: No idea";
    case Z_BUF_ERROR: return "Z_BUF_ERROR: Check you're z_stream!";
    case Z_VERSION_ERROR: return "Z_VERSION_ERROR: Mismatching version!";
    default: return "Unrecognized ZLib-error";
    }
    return "No no";
}

bool RiotArchiveFile::hasFile(const std::string& _path) const {
    auto path = sanitize(_path);
    for (size_t i = 0; i < fileListHeader->mCount; i++) {
        auto name = getFileName(i);
        if (compare(path, name)) {
            return true;
        }
    }
    return false;
}

size_t RiotArchiveFile::getFileIndex(const std::string& _path) const {
    // Can probably be accellerated by grabbing hash of path, looking
    // in the fileheader until a match is found and then
    // linearly search until found
    auto path = sanitize(_path);
    for (size_t i = 0; i < fileListHeader->mCount; i++) {
        auto name = getFileName(i);
        if (compare(path, name)) {
            return i;
        }
    }
    throw RiotArchiveFileException("Could not find file in archive: " + path);
}

size_t RiotArchiveFile::getFileSize(size_t fileIdx) const {
    return fileListEntries[fileIdx].mSize;
}

void RiotArchiveFile::openArchive() const {
    if (archiveFile) {
        return;
    }
    auto arcPath = path + ".dat";
    WIN32_FILE_ATTRIBUTE_DATA arcData;
    RAFenforce(GetFileAttributesEx(arcPath.c_str(), GetFileExInfoStandard, &arcData), "Could not obtain size of .dat file!" + arcPath);
    archiveFile.reset(new MMFile(arcPath, MMOpenMode::read, 0));
}

std::vector<char> RiotArchiveFile::getFileContents(size_t fileIdx) const {
    RAFenforce(fileIdx < fileListHeader->mCount, "Bad fileIdx supplied to getFileName");
    auto entry = fileListEntries + fileIdx;

    openArchive();

    byte* srcPtr;
    archiveFile->get(srcPtr, entry->mOffset);

    std::vector<char> outBuff;

    z_stream stream;
    ZeroMemory(&stream, sizeof(stream));
    inflateInit(&stream);

    stream.avail_in = entry->mSize;
    stream.next_in = srcPtr;

    byte tmp[4000];

    bool doneAny = false;
    do {
        stream.avail_out = sizeof(tmp);
        stream.next_out = tmp;
        auto err = inflate(&stream, Z_NO_FLUSH);
        if (err && err != Z_STREAM_END) {
            if (!doneAny) {
                outBuff.insert(outBuff.end(), srcPtr, srcPtr + entry->mSize);
                inflateEnd(&stream);
                return outBuff;
            }
        }
        RAFenforce(!err || err == Z_STREAM_END, "Error in deflate: " + getZLibError(err));
        auto written = sizeof(tmp)-stream.avail_out;
        outBuff.insert(outBuff.end(), tmp, tmp + written);
        doneAny = true;
    } while (!stream.avail_out);
    inflateEnd(&stream);

    return outBuff;
}

void makePath(std::string path, bool hasFilePart = false) {
    std::replace(path.begin(), path.end(), '/', '\\');
    if (hasFilePart) {
        path = path.substr(0, path.find_last_of('\\'));
    }
    auto shError = SHCreateDirectoryEx(NULL, path.c_str(), NULL);
    RAFenforce(shError == ERROR_SUCCESS || shError == ERROR_ALREADY_EXISTS, "Could not create output directory while extracting file: " + path);
}

void RiotArchiveFile::extractFile(size_t fileIdx, const std::string& outPath) const {
    auto content = getFileContents(fileIdx);
    makePath(outPath, true);
    std::ofstream outStream(outPath, std::ios::binary);
    RAFenforce(outStream.is_open(), "Failed to open file " + outPath);
    outStream.write(content.data(), content.size());
}


void RiotArchiveFile::unpackArchive(const std::string& outPath) const {
    auto totalFiles = this->getFileCount();
    for (size_t fileIdx = 0; fileIdx < totalFiles; fileIdx++) {
        auto fileName = this->getFileName(fileIdx);
        this->extractFile(fileIdx, outPath + "\\" + fileName);
    }
}


std::string RiotArchiveFile::sanitize(const std::string& _path) {
    auto path = _path;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path[0] == '/') return path;
    return "/" + path;
}

unsigned int RiotArchiveFile::hashString(std::string str) {
    unsigned int hash = 0;
    unsigned int tmp;
    for (auto ch : str) {
        hash = (hash << 4) + tolower(ch);
        tmp = hash & 0xf0000000;
        if (tmp) {
            hash = hash ^ (tmp >> 24);
            hash = hash ^tmp;
        }
    }
    return hash;
}


void RiotArchiveFile::removeFile(const std::string& _archivePath) {
    auto archivePath = sanitize(_archivePath);
    for (const auto& it : addList) {
        if (compare(it.first, archivePath)) {
            addList.erase(it.first);
            break;
        }
    }
    for (const auto& item : removeList) {
        if (compare(item, archivePath)) {
            return;
        }
    }

    if (!hasFile(archivePath)) {
        return;
    }
    removeList.insert(archivePath);
}

void RiotArchiveFile::addFile(const std::string& _archivePath, const std::string& filePath) {
    auto archivePath = sanitize(_archivePath);
    removeFile(archivePath);
    addList[archivePath] = AddInfo{ filePath, archivePath };
}


unsigned int compress(const std::string& filePath, FILE* out) {
    auto inFile = new MMFile(filePath, MMOpenMode::read, 0);
    char* data = (char*)inFile->getPtr();
    z_stream stream;
    ZeroMemory(&stream, sizeof(stream));
    deflateInit(&stream, 9);
    
    stream.avail_in = (unsigned int)inFile->getSize();
    stream.next_in = (Bytef*)data;
    char buff[4000];
    unsigned int written = 0;
    do {
        stream.avail_out = sizeof(buff);
        stream.next_out = (Bytef*)buff;
        auto err = deflate(&stream, Z_FINISH);
        if (err && err != Z_STREAM_END) {
            //throw new ZlibException(err);
            throw new RiotArchiveFileException("Error in deflate: " + getZLibError(err));
        }
        int toWrite = sizeof(buff) - stream.avail_out;
        fwrite(buff, 1, toWrite, out);
        written += toWrite;
    } while (stream.avail_out == 0);

    deflateEnd(&stream);
    delete inFile;
    return written;
}



// Load old file
// for each in old file:
//   If not removed:
//      Copy file and make new offsets, add to list of things
// 
// For each new file:
//   Compress
//   Add offsets to list of things
//
// Sort list of things
// Also weirdness checks?
//
// Make new directory




void RiotArchiveFile::apply() {
    if (addList.empty() && removeList.empty()) {
        return;
    }

    openArchive();

    //int fileCountDiff = (int)addList.size() - (int)removeList.size();
    //unsigned int finalFileCount = fileListHeader->mCount + fileCountDiff;

    std::set<unsigned int> toRemoveId;
    for(const auto& removePath : removeList) {
        auto fileIndex = getFileIndex(removePath);
        toRemoveId.insert((unsigned int)fileIndex);
    }


    FILE* archiveOut = nullptr;
    fopen_s(&archiveOut, (path + ".tmp.dat").c_str(), "wb");
    RAFenforce(archiveOut, "Could not create file:" + (path + ".tmp.dat"));

    std::vector<NewFileEntry> newArchiveFiles;
    for (unsigned int fileIdx = 0; fileIdx < fileListHeader->mCount; fileIdx++) {
        if (toRemoveId.find(fileIdx) != toRemoveId.end()) {
            continue;
        }
        auto onlyEarlier = [&](unsigned int a[2]) {
            return a[0] < fileListEntries[fileIdx].mOffset;
        };

        RAF::FileListEntry_t entry = fileListEntries[fileIdx];

        if (entry.mSize) {
            auto size = entry.mSize;
            auto src = (char*)archiveFile->getPtr() + entry.mOffset;
            auto offset = ftell(archiveOut);
            fwrite(src, 1, size, archiveOut);
            auto archivePath = getFileName(fileIdx);

            auto entry = NewFileEntry(archivePath);
            entry.offset = offset;
            entry.size = size;
            newArchiveFiles.push_back(entry);
        }
    }

    for (auto& toAdd : addList) {
        auto offset = ftell(archiveOut);
        auto sourcePath = toAdd.second.sourcePath;
        auto size = compress(sourcePath, archiveOut);

        auto entry = NewFileEntry(toAdd.second.archivePath);
        entry.offset = offset;
        entry.size = size;
        newArchiveFiles.push_back(entry);
    }

    auto sortByHash = [](const NewFileEntry& a, const NewFileEntry& b) {
        if (a.hash < b.hash) { return true; }
        else if (a.hash == b.hash) { return a.archivePath < b.archivePath; }
        return false;
    };
    std::sort(newArchiveFiles.begin(), newArchiveFiles.end(), sortByHash);


    // Now make directory file!

    auto exists = [](const std::string& path) {
        auto attribs = GetFileAttributes(path.c_str());
        return attribs != INVALID_FILE_ATTRIBUTES;
    };
    auto remove = [](const std::string& path) {
        DeleteFileA(path.c_str());
    };
    auto rename = [&](const std::string& from, const std::string& to) {
        RAFenforce(exists(from), "Cant rename file, does not exist: " + from);
        RAFenforce(MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING), "Could not rename file " + from + " to " + to);
    };

    FILE* outFile = nullptr;
    fopen_s(&outFile, (path + ".tmp").c_str(), "wb");
    fwrite(header, sizeof(*header), 1, outFile);

    // Later fseek to sizeof(RAF::Header_t) and write real TOC
    fwrite(TOC, sizeof(*TOC), 1, outFile);

    auto disc = std::string("RAF File created by RAF Packer for Total Commander");
    fwrite(disc.c_str(), 1, disc.size(), outFile);

    auto fileHeaderOffset = ftell(outFile);

    RAF::FileListHeader_t flHeader;
    flHeader.mCount = (unsigned long) newArchiveFiles.size();
    fwrite(&flHeader, sizeof(flHeader), 1, outFile);

    for (unsigned int fileIdx = 0; fileIdx < newArchiveFiles.size(); fileIdx++) {
        const auto& file = newArchiveFiles[fileIdx];
        RAF::FileListEntry_t entry;
        entry.mHash = file.hash;
        entry.mOffset = file.offset;
        entry.mSize = file.size;

        // TODO: Dont use this, create a list of the archivenames, sort it, map index.
        entry.mFileNameStringTableIndex = fileIdx;
        fwrite(&entry, sizeof(entry), 1, outFile);
    }

    auto stringListOffset = ftell(outFile);
    StringTable::HEADER slHeader;
    slHeader.m_Size = 0;
    slHeader.m_Count = (unsigned int) newArchiveFiles.size();
    fwrite(&slHeader, sizeof(slHeader), 1, outFile);

    auto totalSize = sizeof(slHeader) + sizeof(StringTable::ENTRY) * newArchiveFiles.size();
    for (const auto& file : newArchiveFiles) {
        StringTable::ENTRY entry;
        entry.m_Offset = (unsigned int) totalSize;
        entry.m_Size = (unsigned int) file.archivePath.length() + 1;
        fwrite(&entry, sizeof(entry), 1, outFile);
        totalSize += entry.m_Size;
    }
    for (const auto& file : newArchiveFiles) {
        fwrite(file.archivePath.c_str(), 1, file.archivePath.size(), outFile);
        fwrite("\0", 1, 1, outFile); // heh string is \0\0 lololol
    }

    slHeader.m_Size = (unsigned int) totalSize;
    fseek(outFile, stringListOffset, SEEK_SET);
    fwrite(&slHeader, sizeof(slHeader), 1, outFile);

    fseek(outFile, sizeof(RAF::Header_t), SEEK_SET);
    RAF::TableOfContents_t newToc;
    newToc.mMgrIndex = 0;
    newToc.mFileListOffset = fileHeaderOffset;
    newToc.mStringTableOffset = stringListOffset;
    fwrite(&newToc, sizeof(newToc), 1, outFile);

    fclose(archiveOut);
    fclose(outFile);

    // Because path is reset in dispose
    auto origPath = path;
    dispose();

    //rename(origPath, origPath + ".old");
    //rename(origPath + ".dat", origPath + ".old.dat");

    rename(origPath +".tmp", origPath);
    rename(origPath +".tmp.dat", origPath + ".dat");

    addList.clear();
    removeList.clear();

    load(origPath);
}













RiotArchiveFileCollection::RiotArchiveFileCollection(bool buildIndex) : buildIndex(buildIndex) {
}


void RiotArchiveFileCollection::dispose() {
    for (auto archive : archives) {
        archive->dispose();
        delete archive;
    }
    archives.clear();
    archivesNamed.clear();
}

void RiotArchiveFileCollection::closeArchiveFile() const {
    std::cout << "Unmapping " << archives.size() << " archives" << std::endl;
    for (auto archive : archives) {
        archive->closeArchiveFile();
    }
}

size_t RiotArchiveFileCollection::getFileCount() const {
    size_t counter = 0;
    for (auto archive : archives) {
        counter += archive->getFileCount();
    }
    return counter;
}
size_t RiotArchiveFileCollection::getStringCount() const {
    size_t counter = 0;
    for (auto archive : archives) {
        counter += archive->getStringCount();
    }
    return counter;
};

std::string RiotArchiveFileCollection::getFileName(size_t fileIdx) const {
    for (auto archive : archives) {
        auto fileCount = archive->getFileCount();
        if (fileIdx >= fileCount) {
            fileIdx -= fileCount;
            continue;
        }
        return archive->getFileName(fileIdx);
    }
    throw RiotArchiveFileException("RiotArchiveFileCollection::getFileName bad fileIdx");
}

std::string RiotArchiveFileCollection::getString(size_t stringIdx) const {
    for (auto archive : archives) {
        auto stringCount = archive->getStringCount();;
        if (stringIdx >= stringCount) {
            stringIdx -= stringCount;
            continue;
        }
        return archive->getString(stringIdx);
    }
    throw RiotArchiveFileException("RiotArchiveFileCollection::getString bad stringIdx");
}

bool RiotArchiveFileCollection::hasFile(const std::string& path) const {
    for (auto archive : archives) {
        if (archive->hasFile(path)) {
            return true;
        }
    }
    return false;
}

size_t RiotArchiveFileCollection::getFileIndex(const std::string& path) const {
    size_t count = 0;
    for (auto archive : archives) {
        size_t fileCount = archive->getFileCount();
        if (archive->hasFile(path)) {
            return count + archive->getFileIndex(path);
        }
        else {
            count += fileCount;
        }
    }
    throw RiotArchiveFileException("RiotArchiveFileCollection: Could not find file in archive: " + path);
}

std::vector<char> RiotArchiveFileCollection::getFileContents(size_t fileIdx) const {
    for (auto archive : archives) {
        auto fileCount = archive->getFileCount();
        if (fileIdx >= fileCount) {
            fileIdx -= fileCount;
            continue;
        }
        return archive->getFileContents(fileIdx);;
    }
    throw RiotArchiveFileException("RiotArchiveFileCollection::getFileContents bad fileIdx");
}

void RiotArchiveFileCollection::extractFile(size_t fileIdx, const std::string& outPath) const {
    for (auto archive : archives) {
        auto fileCount = archive->getFileCount();
        if (fileIdx >= fileCount) {
            fileIdx -= fileCount;
            continue;
        }
        archive->extractFile(fileIdx, outPath);
        return;
    }
    throw RiotArchiveFileException("RiotArchiveFileCollection::extractFile bad fileIdx");
}

void RiotArchiveFileCollection::unpackArchive(const std::string& outPath) const {
    auto totalFiles = getFileCount();
    for (ptrdiff_t fileIdx = totalFiles-1; fileIdx >= 0; fileIdx--) {
        auto fileName = getFileName(fileIdx);
        auto outFilePath = outPath + "\\" + fileName;
        try {
            makePath(outFilePath, true);
            auto attribs = GetFileAttributes(outFilePath.c_str());
            if (attribs == INVALID_FILE_ATTRIBUTES) {
                extractFile(fileIdx, outFilePath);
            }
        }
        catch (const std::runtime_error& e) {
            if (std::string(e.what()).find("not map view of file")) {
                closeArchiveFile();
                extractFile(fileIdx, outFilePath);
            }
            else {
                //std::cout << e.what() << std::endl;
                throw e;
            }
        }
    }

}

void RiotArchiveFileCollection::addArchive(const std::string& path) {
    if (archivesNamed.find(path) != archivesNamed.end()) {
        return;
    }
    auto archive = new RiotArchiveFile(path);
    archives.push_back(archive);
    archivesNamed[path] = archive;
}

