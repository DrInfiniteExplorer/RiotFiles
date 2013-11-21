#include "RiotFiles\RiotArchiveFile.h"
#include "RiotFiles\MMFile.h"

#include "zlib\zlib.h"

#define RAFenforce(cond, msg) if(!(cond)) { throw RiotArchiveFileException((msg)); }


RiotArchiveFile::RiotArchiveFile(const std::string& path)
{
    load(path);
}


RiotArchiveFile::~RiotArchiveFile()
{
}


void RiotArchiveFile::load(const std::string& archivePath)
{
    directoryFile.reset(new MMFile(archivePath, MMOpenMode::read, 0));
    archiveFile.reset(new MMFile(archivePath + ".dat", MMOpenMode::read, 0));

    directoryFile->get(header, 0);

    RAFenforce(header->mMagic == RAF::MagicNumber, "Bad magic number found in archive: " + archivePath);
    RAFenforce(header->mVersion == RAF::Version, "Bad version number found in archive: " + archivePath);


    directoryFile->get(TOC, sizeof(RAF::Header_t));
    directoryFile->get(fileListHeader, TOC->mFileListOffset);
    directoryFile->get(stringListHeader, TOC->mStringTableOffset);

    RAFenforce(fileListHeader->mCount == stringListHeader->m_Count, "Number of files and number of strings not matching, cant handle this crap!");
    directoryFile->get(fileListEntries, TOC->mFileListOffset + sizeof(RAF::FileListHeader_t));
    directoryFile->get(stringListEntries, TOC->mStringTableOffset + sizeof(StringTable::HEADER));

    path = archivePath;
}

void RiotArchiveFile::dispose() {
    directoryFile.reset();;
    archiveFile.reset();;

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

bool RiotArchiveFile::hasFile(const std::string& path) const {
    for (size_t i = 0; i < fileListHeader->mCount; i++) {
        auto name = getFileName(i);
        if (path == name) {
            return true;
        }
    }
    return false;
}

size_t RiotArchiveFile::getFileIndex(const std::string& path) const {
    for (size_t i = 0; i < fileListHeader->mCount; i++) {
        auto name = getFileName(i);
        if (path == name) {
            return i;
        }
    }
    throw RiotArchiveFileException("Could not find file in archive: " + path);
}

std::vector<char> RiotArchiveFile::getFileContents(size_t fileIdx) const {
    RAFenforce(fileIdx < fileListHeader->mCount, "Bad fileIdx supplied to getFileName");
    auto entry = fileListEntries + fileIdx;

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
