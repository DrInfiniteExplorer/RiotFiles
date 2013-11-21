#pragma once

#include <Windows.h>

#include <string>

enum MMOpenMode {
    read,
    readWrite
};

class MMFileException : public std::runtime_error{
public:
    MMFileException(const std::string& msg) : runtime_error(msg) {}
};

class MMFile
{
    HANDLE fileHandle;
    HANDLE mapHandle;
    void* ptr;
    size_t fileSize;
public:
    MMFile(const std::string &path, MMOpenMode mode, size_t sizeToMap);
    ~MMFile();
    void dispose();

    void* getPtr() {
        return ptr;
    }
    size_t getSize() {
        return fileSize;
    }

    template <typename A>
    void get(A& out, size_t offset) {
        out = A(((char*)ptr) + offset);
    }
};

