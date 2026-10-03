#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <metahook.h>
#include <studio.h>
#include <IUtilAssetsIntegrity.h>

namespace
{
int failures = 0;

void Check(bool condition, const char* scenario)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << scenario << '\n';
        ++failures;
    }
}

void Expect(UtilAssetsIntegrityCheckReason expected, UtilAssetsIntegrityCheckReason actual, const char* scenario)
{
    if (expected != actual)
    {
        std::cerr << "FAIL: " << scenario << " (expected " << static_cast<int>(expected)
                  << ", got " << static_cast<int>(actual) << ")\n";
        ++failures;
    }
}

class Module
{
public:
    explicit Module(const std::filesystem::path& path)
        : handle(LoadLibraryExW(std::filesystem::absolute(path).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
    {
        if (!handle)
        {
            throw std::runtime_error("LoadLibraryExW failed with error " + std::to_string(GetLastError()));
        }
    }

    ~Module() { FreeLibrary(handle); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    HMODULE handle;
};

studiohdr_t MakeStudioHeader(const char* magic = "IDST")
{
    constexpr int studioVersion = 10;
    studiohdr_t header{};
    std::memcpy(&header.id, magic, sizeof(header.id));
    header.version = studioVersion;
    header.length = sizeof(header);
    return header;
}

void CheckStudioModels(IUtilAssetsIntegrity& api)
{
    using Reason = UtilAssetsIntegrityCheckReason;
    UtilAssetsIntegrityCheckResult_StudioModel result;
    auto header = MakeStudioHeader();
    Expect(Reason::OK, api.CheckStudioModel(&header, sizeof(header), &result), "IDST header");
    Expect(Reason::OK, api.CheckStudioModel(&header, sizeof(header), nullptr), "IDST without result");

    Expect(Reason::SizeTooSmall, api.CheckStudioModel(nullptr, 0, &result), "empty model");
    Check(result.ReasonStr[0] != '\0', "empty model diagnostic");
    Expect(Reason::SizeTooSmall, api.CheckStudioModel(&header, sizeof(header) - 1, nullptr), "truncated model");

    header = MakeStudioHeader("????");
    Expect(Reason::BogusHeader, api.CheckStudioModel(&header, sizeof(header), &result), "unknown model signature");
    Check(result.ReasonStr[0] != '\0', "unknown model diagnostic");

    header = MakeStudioHeader();
    --header.version;
    Expect(Reason::VersionMismatch, api.CheckStudioModel(&header, sizeof(header), &result), "IDST version");

    header = MakeStudioHeader();
    ++header.length;
    Expect(Reason::OutOfBound, api.CheckStudioModel(&header, sizeof(header), &result), "IDST file length");

    header = MakeStudioHeader();
    header.numtextures = 1;
    header.textureindex = sizeof(header) + 1;
    header.texturedataindex = sizeof(header);
    Expect(Reason::OutOfBound, api.CheckStudioModel(&header, sizeof(header), &result), "IDST texture table offset");

    header = MakeStudioHeader("IDSQ");
    Expect(Reason::OK, api.CheckStudioModel(&header, sizeof(header), &result), "IDSQ header");
    --header.version;
    Expect(Reason::VersionMismatch, api.CheckStudioModel(&header, sizeof(header), &result), "IDSQ version");
}

constexpr DWORD bmpWidth = 2;
constexpr DWORD bmpHeight = 2;
constexpr WORD indexedBits = 8;
constexpr WORD rgbBits = 24;

std::vector<std::uint8_t> MakeBmp(WORD bits)
{
    constexpr WORD bmpSignature = 0x4d42;
    constexpr DWORD paletteEntries = 256;
    constexpr DWORD rowAlignmentBits = 32;
    constexpr DWORD rowAlignmentBytes = 4;
    const DWORD paletteSize = bits == indexedBits ? paletteEntries * sizeof(RGBQUAD) : 0;
    const DWORD rowSize = ((bmpWidth * bits + rowAlignmentBits - 1) / rowAlignmentBits) * rowAlignmentBytes;
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = bmpSignature;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paletteSize;
    fileHeader.bfSize = fileHeader.bfOffBits + rowSize * bmpHeight;
    BITMAPINFOHEADER infoHeader{};
    infoHeader.biSize = sizeof(infoHeader);
    infoHeader.biWidth = bmpWidth;
    infoHeader.biHeight = bmpHeight;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = bits;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = rowSize * bmpHeight;
    infoHeader.biClrUsed = bits == indexedBits ? paletteEntries : 0;

    std::vector<std::uint8_t> image(fileHeader.bfSize);
    std::memcpy(image.data(), &fileHeader, sizeof(fileHeader));
    std::memcpy(image.data() + sizeof(fileHeader), &infoHeader, sizeof(infoHeader));
    if (bits == indexedBits)
    {
        for (DWORD index = 0; index < paletteEntries; ++index)
        {
            // A colored palette keeps FreeImage from classifying this as grayscale.
            const RGBQUAD color{static_cast<BYTE>(index), 0, 255, 0};
            std::memcpy(image.data() + sizeof(fileHeader) + sizeof(infoHeader) + index * sizeof(color), &color, sizeof(color));
        }
    }
    return image;
}

void CheckBmps(IUtilAssetsIntegrity& api)
{
    using Reason = UtilAssetsIntegrityCheckReason;
    const auto image = MakeBmp(indexedBits);
    UtilAssetsIntegrityCheckResult_BMP result;
    result.MaxWidth = bmpWidth;
    result.MaxHeight = bmpHeight;
    result.MaxSize = bmpWidth * bmpHeight;
    Expect(Reason::OK, api.Check8bitBMP(image.data(), image.size(), &result), "indexed BMP at limits");
    Expect(Reason::OK, api.Check8bitBMP(image.data(), image.size(), nullptr), "indexed BMP without limits");

    --result.MaxWidth;
    Expect(Reason::SizeTooLarge, api.Check8bitBMP(image.data(), image.size(), &result), "BMP width limit");
    Check(result.ReasonStr[0] != '\0', "BMP width diagnostic");
    result.MaxWidth = bmpWidth;
    --result.MaxHeight;
    Expect(Reason::SizeTooLarge, api.Check8bitBMP(image.data(), image.size(), &result), "BMP height limit");
    result.MaxHeight = bmpHeight;
    --result.MaxSize;
    Expect(Reason::SizeTooLarge, api.Check8bitBMP(image.data(), image.size(), &result), "BMP pixel count limit");

    UtilAssetsIntegrityCheckResult_BMP defaults;
    Expect(Reason::SizeTooLarge, api.Check8bitBMP(image.data(), image.size(), &defaults), "BMP zero limits");
    const auto rgb = MakeBmp(rgbBits);
    Expect(Reason::InvalidFormat, api.Check8bitBMP(rgb.data(), rgb.size(), nullptr), "RGB BMP");
    Expect(Reason::BogusHeader, api.Check8bitBMP(image.data(), sizeof(BITMAPFILEHEADER), &result), "truncated BMP");
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: UtilAssetsIntegritySmokeTests <UtilAssetsIntegrity.dll> <FreeImage DLL>\n";
        return 2;
    }
    try
    {
        // Preload the exact runtime, including the installed FreeImage subdirectory.
        // The host likewise makes its dependency directories available before loading plugins.
        Module freeimage(argv[2]);
        Module library(argv[1]);
        auto factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(library.handle, CREATEINTERFACE_PROCNAME));
        if (!factory)
            throw std::runtime_error("CreateInterface export is missing");

        int returnCode = IFACE_FAILED;
        auto api = static_cast<IUtilAssetsIntegrity*>(factory(UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION, &returnCode));
        if (!api)
            throw std::runtime_error("UtilAssetsIntegrityAPI_001 is unavailable");
        Check(returnCode == IFACE_OK, "interface success code");
        Check(api == factory(UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION, nullptr), "singleton factory");
        returnCode = IFACE_OK;
        Check(factory("UtilAssetsIntegrityAPI_999", &returnCode) == nullptr, "unknown interface version");
        Check(returnCode == IFACE_FAILED, "unknown interface failure code");
        CheckStudioModels(*api);
        CheckBmps(*api);
        if (failures != 0)
            return 1;
        std::cout << "Public interface, StudioModel and BMP smoke tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
