#pragma once

#include <string>
#include <vector>

class RiotArchiveFile;

namespace SKN {
#pragma pack(push)
#pragma pack(1)
    struct Header_t {
        unsigned long mMagic;
        unsigned short mVersion;
    };
    struct TableOfContents_t {
        unsigned short mObjectCount;
        unsigned long mMaterialCount;
    };

    struct MaterialHeader_t {
        char mMaterialName[64];
        unsigned long mStartVertex;
        unsigned long mVertexCount;
        unsigned long mStartIndex;
        unsigned long mIndexCount;
    };
    struct MeshHeader_t {
        unsigned long mIndexCount;
        unsigned long mVertexCount;
    };

    struct Vertex_t {
        float mXYZ[3];
        unsigned char mBoneIndices[4];
        float mBoneWeights[4];
        float mNormalXYZ[3];
        float mUV[2];
    };
    struct EndData_t {
        int i1;
        int i2;
        int i3;
    };
#pragma pack(pop)
}

namespace SKL {
#pragma pack(push)
#pragma pack(1)
    struct Header_t {
        char mMagic[8];
        unsigned int mVersion;
    };
    struct Bone_t {
        char mName[32];
        int mParentId;
        float mScale;
        float mMatrix[12];
    };
#pragma pack(pop)
}

namespace ANM {
#pragma pack(push)
#pragma pack(1)
    struct Header_t {
        char mMagic[8];
        unsigned int mVersion;
    };
    struct Bone_t {
        char mName[32];
        unsigned int unknown;
    };
    struct BoneFrame_t {
        float mOrientation[4];
        float mPosition[3];
    };
#pragma pack(pop)
}


class RiotSkinException : public std::runtime_error{
public:
    RiotSkinException(const std::string& msg) : runtime_error(msg) {}
};
class RiotSkeletonException : public std::runtime_error{
public:
    RiotSkeletonException(const std::string& msg) : runtime_error(msg) {}
};
class RiotAnimationException : public std::runtime_error{
public:
    RiotAnimationException(const std::string& msg) : runtime_error(msg) {}
};

#define SKNenforce(cond, msg) if(!(cond)) { throw RiotSkinException((msg)); }
#define SKLenforce(cond, msg) if(!(cond)) { throw RiotSkeletonException((msg)); }
#define ANMenforce(cond, msg) if(!(cond)) { throw RiotAnimationException((msg)); }

class RiotSkin
{
public:
    SKN::Header_t header;
    SKN::TableOfContents_t toc;
    std::vector<SKN::MaterialHeader_t> materialHeaders;
    SKN::MeshHeader_t meshHeader;
    std::vector<unsigned short> indices;
    std::vector<SKN::Vertex_t> vertices;
    SKN::EndData_t endData;

    RiotSkin(const void* data, size_t length) { load(data, length); }
    ~RiotSkin() { dispose(); }
    void load(const void* data, size_t length);
    void dispose();
};

class RiotSkeleton
{
public:

    SKL::Header_t header;
    unsigned int designerId; // Used in version 1,2
    std::vector<SKL::Bone_t> bones;
    std::vector<int> boneIds; //Used in version 2

    RiotSkeleton(const void* data, size_t length) { load(data, length); }
    ~RiotSkeleton() { dispose(); }
    void load(const void* data, size_t length);
    void dispose();
};

class RiotAnimation {
public:
    ANM::Header_t header;
    unsigned int designerId; // Ver 0,1,2,3
    unsigned int boneCount;  // Ver 0,1,2,3
    unsigned int frameCount; // Ver 0,1,2,3
    unsigned int fps;        // Ver 0,1,2,3

    struct Bone {
        ANM::Bone_t bone;
        std::vector<ANM::BoneFrame_t> frames;
    };

    std::vector<Bone> bones;

    RiotAnimation(const void* data, size_t length) { load(data, length); }
    ~RiotAnimation() { dispose(); }

    void load(const void* data, size_t length);
    void dispose();
};
