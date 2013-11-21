#include "RiotSkin.h"

#include "RiotArchiveFile.h"

#include <iostream>
#include <set>

#define MemRead(X, Y)   (*(Y*)(X)); (X)+=sizeof(Y);

void RiotSkin::load(void* data, size_t length) {
    dispose();
    char* ptr = (char*)data;
    header = MemRead(ptr, SKN::Header_t);

    SKNenforce(header.mMagic == 0x112233, "Magic header wrong when trying to load SKN file");
    SKNenforce(0 < header.mVersion && header.mVersion < 3, "Unsupported version of SKN file(Supports version 1 & 2)");

    toc = MemRead(ptr, SKN::TableOfContents_t);

    SKN::MaterialHeader_t material;
    for (unsigned int idx = 0; idx < toc.mMaterialCount; idx++) {
        material = MemRead(ptr, SKN::MaterialHeader_t);
        materialHeaders.push_back(material);
    }
    meshHeader = MemRead(ptr, SKN::MeshHeader_t);

    indices.reserve(meshHeader.mIndexCount);
    unsigned short *indexPtr = (unsigned short*)ptr;
    indices.insert(indices.end(), indexPtr, indexPtr + meshHeader.mIndexCount);
    ptr += meshHeader.mIndexCount * sizeof(unsigned short);

    SKN::Vertex_t* vertPtr = (SKN::Vertex_t*)ptr;
    vertices.reserve(meshHeader.mVertexCount);
    vertices.insert(vertices.end(), vertPtr, vertPtr + meshHeader.mVertexCount);
    ptr += meshHeader.mVertexCount * sizeof(SKN::Vertex_t);
    
    if (header.mVersion == 2) {
        endData = MemRead(ptr, SKN::EndData_t);
    }

    if (true) {
        std::cout << "SKN magic: " << header.mMagic << std::endl;
        std::cout << "SKN version: " << header.mVersion << std::endl;        
        std::cout << "SKN objects: " << toc.mObjectCount << std::endl;
        std::cout << "SKN materials: " << toc.mMaterialCount << std::endl;
        for (auto material : materialHeaders) {
            std::cout << "\t" << material.mMaterialName << " " << material.mVertexCount << " " << material.mVertexCount << std::endl;
        }

        std::cout << "SKN vertices: " << meshHeader.mVertexCount << std::endl;
        std::cout << "SKN indices: " << meshHeader.mIndexCount << std::endl;

        int max = 0;
        std::set<int> referenced;
        for (auto vert : vertices) {
            for (int i = 0; i < 4; i++) {
                max = max < vert.mBoneIndices[i] ? vert.mBoneIndices[i] : max;
                referenced.insert(vert.mBoneIndices[i]);
            }
        }

        std::cout << "SKN max referenced boneid: " << max << std::endl;
        std::cout << "SKN unique referenced boneid: " << referenced.size() << std::endl;
        for (auto idx : referenced) {
            std::cout << "\t" << idx << std::endl;
        }
        std::cout << std::endl;
    }
}


void RiotSkin::dispose() {
    indices.clear();
    vertices.clear();
}

RiotSkin* loadRiotSkin(const RiotArchiveFile* archive, const std::string& path) {
    auto fileId = archive->getFileIndex(path);
    auto content = archive->getFileContents(fileId);

    return new RiotSkin(content.data(), content.size());
}


void RiotSkeleton::load(void* data, size_t length) {
    dispose();
    char* ptr = (char*)data;

    header = MemRead(ptr, SKL::Header_t);

    SKLenforce(std::string(header.mMagic, 8) == "r3d2sklt", "Magic header wrong when trying to load SKN file");
    SKLenforce(0 < header.mVersion && header.mVersion < 3, "Unsupported version of SKL file(Supports version 1 & 2)");

    if (header.mVersion == 1 || header.mVersion == 2) {
        designerId = MemRead(ptr, unsigned int);
        auto boneCount = MemRead(ptr, unsigned int);

        bones.reserve(boneCount);
        for (unsigned int idx = 0; idx < boneCount; idx++) {
            auto bone = MemRead(ptr, SKL::Bone_t);
            bones.push_back(bone);
        }

        if (header.mVersion == 1) {
            for (unsigned int idx = 0; idx < boneCount; idx++) {
                boneIds.push_back(idx);
            }
        } else {
            auto boneIdCount = MemRead(ptr, unsigned int);
            boneIds.reserve(boneIdCount);
            for (unsigned int idx = 0; idx < boneIdCount; idx++) {
                auto boneId = MemRead(ptr, unsigned int);
                boneIds.push_back(boneId);
            }
        } 
    }
    else if (header.mVersion == 0) { 

        auto zero = MemRead(ptr, unsigned short);
        auto boneCount = MemRead(ptr, unsigned short);
        auto boneIdCount = MemRead(ptr, unsigned int);
        bones.reserve(boneCount);
        boneIds.reserve(boneIdCount);
        throw RiotSkeletonException("SKL version 0 not implemented yet");
    }
    else {
        throw RiotSkeletonException("Unsupported version of SKL encountered");    
    }

    if (true) {
        std::cout << "SKL magic: " << std::string(this->header.mMagic, 8) << std::endl;
        std::cout << "SKL version: " << this->header.mVersion << std::endl;
        std::cout << "SKL Designer: " << this->designerId << std::endl;
        std::cout << "SKL Bonecount: " << this->bones.size() << std::endl;
        for (size_t idx = 0; idx < bones.size(); idx++) {
            const auto& bone = bones[idx];
            std::cout << "\t" << idx << " " << bone.mParentId << " " << bone.mName << " " << bone.mScale << std::endl;
            SKLenforce(idx > size_t(bone.mParentId), "Bad parent relation found in SKL file");
        }
        std::cout << "SKL BoneIdCount: " << this->boneIds.size() << std::endl;
        for (size_t idx = 0; idx < boneIds.size(); idx++) {
            const auto& id = boneIds[idx];
            std::cout << "\t" << idx << " " << id << std::endl;
        }
    }
}

void RiotSkeleton::dispose() {
    bones.clear();
    boneIds.clear();
}


void RiotAnimation::load(void* data, size_t length) {
    char* ptr = (char*)data;
    header = MemRead(ptr, ANM::Header_t);

    ANMenforce(std::string(header.mMagic, 8) == "r3d2anmd", "Wrong magic header for riot animation file.");
    ANMenforce(0 <= header.mVersion && header.mVersion < 5, "Only supports versions 0-4 of ANM files.");

    if (0 <= header.mVersion && header.mVersion <= 4) {
        designerId = MemRead(ptr, unsigned int);
        boneCount = MemRead(ptr, unsigned int);
        frameCount = MemRead(ptr, unsigned int);
        fps = MemRead(ptr, unsigned int);

        bones.resize(boneCount);
        for (auto& bone : bones) {
            bone.bone = MemRead(ptr, ANM::Bone_t);
            bone.frames.resize(frameCount);
            for (auto& frame : bone.frames) {
                frame = MemRead(ptr, ANM::BoneFrame_t);
            }
        }
    }
    else {
        throw RiotAnimationException("Unsupported version of ANM file");
    }

    std::cout << "ANM magic: " << std::string(this->header.mMagic, 8) << std::endl;
    std::cout << "ANM version: " << this->header.mVersion << std::endl;
    std::cout << "ANM Designer: " << this->designerId << std::endl;
    std::cout << "ANM Frames: " << this->frameCount << std::endl;
    std::cout << "ANM FPS: " << this->fps << std::endl;
    std::cout << "ANM Bonecount: " << this->bones.size() << std::endl;
    for (size_t idx = 0; idx < bones.size(); idx++) {
        const auto& bone = bones[idx];
        std::cout << "\t" << idx << " " << bone.bone.mName << " " << bone.bone.unknown << std::endl;
    }
}

void RiotAnimation::dispose() {
    bones.clear();
}

RiotSkeleton* loadRiotSkeleton(const RiotArchiveFile* archive, const std::string& path) {
    auto fileId = archive->getFileIndex(path);
    auto content = archive->getFileContents(fileId);

    return new RiotSkeleton(content.data(), content.size());
}

RiotAnimation* loadRiotAnimation(const RiotArchiveFile* archive, const std::string& path) {
    auto fileId = archive->getFileIndex(path);
    auto content = archive->getFileContents(fileId);
    return new RiotAnimation(content.data(), content.size());
}
