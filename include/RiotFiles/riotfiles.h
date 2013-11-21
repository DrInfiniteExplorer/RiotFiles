#pragma once

#include "RiotFiles\RiotArchiveFile.h"
#include "RiotFiles\RiotSkin.h"

// These are inline. Why? Because they allocate data which needs to be deleted.
// Because allocation and runtime.
//
// Either way, with these being inline it should be allocated by whatever calls these functions
// Meaning we push the troubles of runtime mixing further down the line! :)

inline RiotSkin* loadRiotSkin(const RiotArchiveFile* archive, const std::string& path) {
    auto fileId = archive->getFileIndex(path);
    auto content = archive->getFileContents(fileId);

    return new RiotSkin(content.data(), content.size());
}
inline RiotSkeleton* loadRiotSkeleton(const RiotArchiveFile* archive, const std::string& path) {
    auto fileId = archive->getFileIndex(path);
    auto content = archive->getFileContents(fileId);

    return new RiotSkeleton(content.data(), content.size());
}
inline RiotAnimation* loadRiotAnimation(const RiotArchiveFile* archive, const std::string& path) {
    auto fileId = archive->getFileIndex(path);
    auto content = archive->getFileContents(fileId);
    return new RiotAnimation(content.data(), content.size());
}

