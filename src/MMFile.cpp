#include "RiotFiles\MMFile.h"

#define MMFenforce(cond, msg) if(!(cond)) { throw MMFileException((msg)); }

MMFile::MMFile(const std::string &path, MMOpenMode mode, size_t sizeToMap)
{
    auto desiredAccess = mode == read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE;
    auto shareMode = mode == read ? FILE_SHARE_READ : FILE_SHARE_READ | FILE_SHARE_WRITE;
    auto createDisposition = mode == read ? OPEN_EXISTING : OPEN_ALWAYS;
    fileHandle = CreateFile(path.c_str(), desiredAccess, shareMode, NULL, createDisposition, 0, NULL);
    MMFenforce(fileHandle != INVALID_HANDLE_VALUE, "Could not open desired file: " + path);
    if (sizeToMap == 0) {
        unsigned long low, high;
        low = GetFileSize(fileHandle, &high);
#ifdef _WIN64 // Do not support files larger than 4.2 gb because we are retarded :)
        // Hmm actually it might be a bad idea to use 4.2 gb of memory space in 32-bit mode anyway, so consider this
        // a way to enforce proper.. things and crap. In a passive-aggressive silent-(error)-treatment way.
        sizeToMap = low + (size_t(high) << 32);
#endif
    }
    fileSize = sizeToMap;
    MMFenforce(fileSize, "File is of 0 size, cant map that!");

    auto protectMode = mode == read ? PAGE_READONLY : PAGE_READWRITE;
    mapHandle = CreateFileMapping(fileHandle, NULL, protectMode, 0, 0, NULL);
    MMFenforce(mapHandle != NULL, "Could not create file mapping");
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

