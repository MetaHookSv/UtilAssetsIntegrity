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

class IUtilAssetsIntegrity : public IBaseInterface
{
public:
    virtual UtilAssetsIntegrityCheckReason CheckStudioModel(const void* buf, size_t bufSize, UtilAssetsIntegrityCheckResult_StudioModel* checkResult) = 0;
    virtual UtilAssetsIntegrityCheckReason Check8bitBMP(const void* buf, size_t bufSize, UtilAssetsIntegrityCheckResult_BMP* checkResult)             = 0;
};

#if defined(_WIN32) && defined(UTILASSETSINTEGRITY_EXPORTS)
#    define UTILASSETSINTEGRITY_API __declspec(dllexport)
#elif defined(_WIN32) && defined(UTILASSETSINTEGRITY_IMPORTS)
#    define UTILASSETSINTEGRITY_API __declspec(dllimport)
#else
#    define UTILASSETSINTEGRITY_API
#endif

//Optional import-library entry points; API_001's vtable and result layouts remain unchanged.
extern "C" UTILASSETSINTEGRITY_API IUtilAssetsIntegrity* UtilAssetsIntegrity() noexcept;

//Checks every sequence belonging to groupIndex using the main model's structural header (raw or engine-cached).
//Unlike standalone IDSQ header validation, this checks the bones, blends, all six RLE channels and interpolation reads.
extern "C" UTILASSETSINTEGRITY_API UtilAssetsIntegrityCheckReason UtilAssetsIntegrity_CheckStudioSequenceGroup(
    const void*                                 model,
    size_t                                      modelBytes,
    int                                         groupIndex,
    const void*                                 group,
    size_t                                      groupBytes,
    UtilAssetsIntegrityCheckResult_StudioModel* result) noexcept;

#define UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION "UtilAssetsIntegrityAPI_001"
