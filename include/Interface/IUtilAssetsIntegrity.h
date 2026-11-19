#pragma once

#include <interface.h>
#include <stdint.h>
#include <stddef.h>

enum class UtilAssetsIntegrityCheckReason
{
    OK = 0,
    Unknown,
    InvalidFormat,
    SizeTooLarge,
    SizeTooSmall,
    BogusHeader,
    VersionMismatch,
    OutOfBound,
};

class UtilAssetsIntegrityCheckResult
{
public:
    UtilAssetsIntegrityCheckResult()
    {
        ReasonStr[0] = 0;
    }

public:
    char ReasonStr[256];
};

class UtilAssetsIntegrityCheckResult_StudioModel : public UtilAssetsIntegrityCheckResult
{
public:
};

class UtilAssetsIntegrityCheckResult_BMP : public UtilAssetsIntegrityCheckResult
{
public:
    UtilAssetsIntegrityCheckResult_BMP() : UtilAssetsIntegrityCheckResult()
    {
        MaxWidth  = 0;
        MaxHeight = 0;
        MaxSize   = 0;
    }

public:
    size_t MaxWidth;
    size_t MaxHeight;
    size_t MaxSize;
};

//Order of a BSP file's lump table. Blue Shift's brush files swap the entities and planes entries; the file does not say
//which it uses, so the caller passes the layout it loads the file with.
enum class UtilAssetsIntegrityBSPLayout
{
    GoldSrc = 0,
    BlueShift,
};

class UtilAssetsIntegrityCheckResult_BSP : public UtilAssetsIntegrityCheckResult
{
public:
    UtilAssetsIntegrityCheckResult_BSP() : UtilAssetsIntegrityCheckResult()
    {
        MaxLeafs = 0;
    }

public:
    //Leaf records the caller's PVS buffers hold (an engine's MAX_MAP_LEAFS). As with the BMP limits, zero is enforced
    //and a null result pointer skips the limit.
    size_t MaxLeafs;
};

class IUtilAssetsIntegrity : public IBaseInterface
{
public:
    virtual UtilAssetsIntegrityCheckReason CheckStudioModel(const void* buf, size_t bufSize, UtilAssetsIntegrityCheckResult_StudioModel* checkResult) = 0;
    virtual UtilAssetsIntegrityCheckReason Check8bitBMP(const void* buf, size_t bufSize, UtilAssetsIntegrityCheckResult_BMP* checkResult)             = 0;

    //Checks every sequence belonging to groupIndex using the main model's structural header (raw or engine-cached).
    //Unlike standalone IDSQ header validation, this checks the bones, blends, all six RLE channels and interpolation reads.
    //The interpolation read at the last frame of a run may end up to UTIL_ASSETS_INTEGRITY_SEQUENCE_GROUP_TAIL_BYTES past
    //the group's bytes, as stock group files do; callers that animate the group keep that many zeroed bytes after it.
    //Appended after the existing methods so their vtable slots and the result layouts stay unchanged.
    virtual UtilAssetsIntegrityCheckReason CheckStudioSequenceGroup(
        const void*                                 model,
        size_t                                      modelBytes,
        int                                         groupIndex,
        const void*                                 group,
        size_t                                      groupBytes,
        UtilAssetsIntegrityCheckResult_StudioModel* result) = 0;

    //Checks a version 30 BSP before any of it is parsed: every lump range and record size, every cross-reference as the
    //GoldSrc engine interprets it, the light samples and visibility rows it decodes, and that its node and clipnode
    //graphs are acyclic and shallow enough to recurse. Appended after the existing methods, as above.
    virtual UtilAssetsIntegrityCheckReason CheckBSP(
        const void*                         buf,
        size_t                              bufSize,
        UtilAssetsIntegrityBSPLayout        layout,
        UtilAssetsIntegrityCheckResult_BSP* result) = 0;
};

//The interface is published only through EXPOSE_SINGLE_INTERFACE; callers resolve
//UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION through CreateInterface.

#define UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION "UtilAssetsIntegrityAPI_001"

//GoldSrc's animators read the first sample after a run's last span at its last frame, with no weight: up to two
//mstudioanimvalue_t past that span. Stock group files (such as scientist01.mdl) end before it.
#define UTIL_ASSETS_INTEGRITY_SEQUENCE_GROUP_TAIL_BYTES 4
