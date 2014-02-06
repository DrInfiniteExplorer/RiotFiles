#include "RiotFiles\MMFile.h"

#define MMFenforce(cond, msg) if(!(cond)) { throw MMFileException((msg)); }

MMFile::MMFile(const std::string &path, MMOpenMode mode, size_t _sizeToMap)
{
    auto desiredAccess = mode == read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE;
    auto shareMode = mode == read ? FILE_SHARE_READ : FILE_SHARE_READ | FILE_SHARE_WRITE;
    auto createDisposition = mode == read ? OPEN_EXISTING : OPEN_ALWAYS;
    fileHandle = CreateFile(path.c_str(), desiredAccess, shareMode, NULL, createDisposition, 0, NULL);
    MMFenforce(fileHandle != INVALID_HANDLE_VALUE, "Could not open desired file: " + path);
    LARGE_INTEGER size;
    size.QuadPart = _sizeToMap;
    if (size.QuadPart == 0) {
        size.LowPart = GetFileSize(fileHandle, (LPDWORD)&size.HighPart);
    }
    fileSize = (size_t)size.QuadPart;
    MMFenforce(fileSize, std::string("File is of 0 size, cant map that! ") + path);


    auto protectMode = mode == read ? PAGE_READONLY : PAGE_READWRITE;
    mapHandle = CreateFileMapping(fileHandle, NULL, protectMode, size.HighPart, size.LowPart, NULL);
    MMFenforce(mapHandle != NULL, "Could not create file mapping for file " + path);
    auto mapAccess = mode == read ? FILE_MAP_READ : FILE_MAP_WRITE;
    ptr = MapViewOfFileEx(mapHandle, mapAccess, 0, 0, fileSize, NULL);
    MMFenforce(ptr != NULL, "Could not map view of file");
}


MMFile::~MMFile()
{
    dispose();
}

void MMFile::dispose() {
    fileSize = 0;
    if (ptr) {
        FlushViewOfFile(ptr, fileSize);
        UnmapViewOfFile(ptr); ptr = nullptr;
    }
    if (mapHandle != NULL) {
        CloseHandle(mapHandle); mapHandle = INVALID_HANDLE_VALUE;
    }
    if (fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(fileHandle); fileHandle = INVALID_HANDLE_VALUE;
    }
}

