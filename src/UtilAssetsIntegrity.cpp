#include <IUtilAssetsIntegrity.h>
#include "StudioModelValidation.h"

#include <memory>
#include <limits>
#if !defined(UTILASSETSINTEGRITY_NO_FREEIMAGE)
#    include <FreeImage.h>
#endif

class CUtilAssetsIntegrity : public IUtilAssetsIntegrity
{
public:
    UtilAssetsIntegrityCheckReason CheckStudioModel(
        const void*                                 buf,
        size_t                                      bufSize,
        UtilAssetsIntegrityCheckResult_StudioModel* checkResult) noexcept override
    {
        return CStudioModelValidation(buf, bufSize, checkResult).Check();
    }

    UtilAssetsIntegrityCheckReason Check8bitBMP(
        const void*                         buf,
        size_t                              bufSize,
        UtilAssetsIntegrityCheckResult_BMP* checkResult) noexcept override
    {
#if defined(UTILASSETSINTEGRITY_NO_FREEIMAGE)
        if (checkResult)
            std::snprintf(checkResult->ReasonStr, sizeof(checkResult->ReasonStr), "BMP validation requires FreeImage support.");

        return UtilAssetsIntegrityCheckReason::Unknown;
#else
        using Reason = UtilAssetsIntegrityCheckReason;

        if (!buf || bufSize > (std::numeric_limits<DWORD>::max)())
        {
            if (checkResult)
                std::snprintf(checkResult->ReasonStr, sizeof(checkResult->ReasonStr), "Invalid BMP input size.");

            return Reason::InvalidFormat;
        }

        auto* pMemory = FreeImage_OpenMemory(static_cast<BYTE*>(const_cast<void*>(buf)), static_cast<DWORD>(bufSize));

        std::unique_ptr<FIMEMORY, decltype(&FreeImage_CloseMemory)> fim(pMemory, FreeImage_CloseMemory);
        if (!fim)
        {
            if (checkResult)
                std::snprintf(checkResult->ReasonStr, sizeof(checkResult->ReasonStr), "Failed to open BMP memory.");

            return Reason::Unknown;
        }

        auto* pBitmap = FreeImage_LoadFromMemory(FIF_BMP, fim.get());

        std::unique_ptr<FIBITMAP, decltype(&FreeImage_Unload)> fib(pBitmap, FreeImage_Unload);
        if (!fib)
        {
            if (checkResult)
                std::snprintf(checkResult->ReasonStr, sizeof(checkResult->ReasonStr), "Failed to decode BMP.");

            return Reason::BogusHeader;
        }

        if (FreeImage_GetColorType(fib.get()) != FIC_PALETTE)
        {
            if (checkResult)
                std::snprintf(checkResult->ReasonStr, sizeof(checkResult->ReasonStr), "BMP is not indexed color.");

            return Reason::InvalidFormat;
        }

        const auto width  = FreeImage_GetWidth(fib.get());
        const auto height = FreeImage_GetHeight(fib.get());
        if (checkResult &&
            (width > checkResult->MaxWidth ||
             height > checkResult->MaxHeight ||
             std::uint64_t(width) * height > checkResult->MaxSize))
        {
            std::snprintf(checkResult->ReasonStr, sizeof(checkResult->ReasonStr), "BMP dimensions or pixel count exceed limits.");
            return Reason::SizeTooLarge;
        }

        return Reason::OK;
#endif
    }

    UtilAssetsIntegrityCheckReason CheckStudioSequenceGroup(
        const void*                                 model,
        size_t                                      modelBytes,
        int                                         groupIndex,
        const void*                                 group,
        size_t                                      groupBytes,
        UtilAssetsIntegrityCheckResult_StudioModel* result) noexcept override
    {
        return CStudioModelValidation(group, groupBytes, result).CheckGroup(model, modelBytes, groupIndex);
    }
};

EXPOSE_SINGLE_INTERFACE(CUtilAssetsIntegrity, IUtilAssetsIntegrity, UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION);
