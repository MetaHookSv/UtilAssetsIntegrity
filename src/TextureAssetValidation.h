#pragma once

#include <IUtilAssetsIntegrity.h>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace TextureAssetValidation
{
using Reason = UtilAssetsIntegrityCheckReason;
using Byte   = std::uint8_t;

constexpr int      MaxTextureSide = 16384;
constexpr unsigned PaletteColors  = 256;
constexpr size_t   MipLevels      = 4;

//GoldSrc little-endian disk records. Copy into aligned locals before inspecting fields.
struct MiptexHeader
{
    char          name[16];
    std::uint32_t width, height, offsets[MipLevels];
};
struct WADHeader
{
    char         signature[4];
    std::int32_t count, directory;
};
struct WADEntry
{
    std::int32_t position, diskSize, size;
    Byte         type, compression, padding[2];
    char         name[16];
};
struct SpriteHeader
{
    char         signature[4];
    std::int32_t version, orientation, textureFormat;
    float        radius;
    std::int32_t width, height, frames;
    float        beamLength;
    std::int32_t syncType;
};
struct SpriteFrame
{
    std::int32_t x, y, width, height;
};
static_assert(sizeof(MiptexHeader) == 40 && sizeof(WADHeader) == 12 && sizeof(WADEntry) == 32);
static_assert(sizeof(SpriteHeader) == 40 && sizeof(SpriteFrame) == 16);

template <class T>
T Read(const Byte* bytes)
{
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

inline Reason Fail(UtilAssetsIntegrityCheckResult* result, const char* text, Reason reason = Reason::OutOfBound)
{
    if (result)
        std::snprintf(result->ReasonStr, sizeof(result->ReasonStr), "%s", text);
    return reason;
}

inline Reason Miptex(const void* buffer, size_t size, bool allowExternal, UtilAssetsIntegrityCheckResult* result)
{
    if (result)
        result->ReasonStr[0] = 0;
    constexpr size_t alignment = 16;
    const auto*      p         = static_cast<const Byte*>(buffer);
    if (!p || size < sizeof(MiptexHeader))
        return Fail(result, "Truncated miptex header");
    const auto header = Read<MiptexHeader>(p);
    if (!std::memchr(header.name, 0, sizeof(header.name)))
        return Fail(result, "Unterminated miptex name");
    const auto width = header.width, height = header.height;
    if (!width || !height || width > MaxTextureSide || height > MaxTextureSide)
        return Fail(result, "Invalid miptex dimensions", Reason::InvalidFormat);
    if (allowExternal && !header.offsets[0])
        return Reason::OK;
    if (width % alignment || height % alignment)
        return Fail(result, "Miptex dimensions are not 16 aligned", Reason::InvalidFormat);

    //Mod_LoadTextures locates the base pixels and palette using the packed layout.
    size_t cursor = sizeof(header), pixels = width * height;
    for (size_t level = 0; level < MipLevels; ++level)
    {
        if (header.offsets[level] != cursor || pixels > size - cursor)
            return Fail(result, "Miptex level outside packed pixel data");
        cursor += pixels;
        pixels /= 4;
    }
    if (size - cursor < sizeof(std::uint16_t))
        return Fail(result, "Missing miptex color count");
    const auto   colors       = Read<std::uint16_t>(p + cursor);
    const size_t paletteBytes = (colors < PaletteColors ? PaletteColors : colors) * 3;
    if (paletteBytes > size - cursor - sizeof(colors))
        return Fail(result, "Miptex palette outside lump");
    return Reason::OK;
}

inline Reason WADDirectory(const void* buffer, size_t size, UtilAssetsIntegrityCheckResult* result)
{
    if (result)
        result->ReasonStr[0] = 0;
    const auto* p = static_cast<const Byte*>(buffer);
    if (!p || size < sizeof(WADHeader))
        return Fail(result, "Truncated WAD header");
    const auto header = Read<WADHeader>(p);
    if (std::memcmp(header.signature, "WAD2", 4) && std::memcmp(header.signature, "WAD3", 4))
        return Fail(result, "Unknown WAD signature", Reason::InvalidFormat);
    const int count = header.count, offset = header.directory;
    if (count < 0 || offset < 0 || size_t(offset) > size || size_t(count) > (size - offset) / sizeof(WADEntry))
        return Fail(result, "WAD directory outside file");
    for (int i = 0; i < count; ++i)
    {
        const auto lump = Read<WADEntry>(p + offset + size_t(i) * sizeof(WADEntry));
        const int  pos = lump.position, bytes = lump.diskSize;
        if (pos < 0 || bytes < 0 || size_t(pos) > size || size_t(bytes) > size - pos)
            return Fail(result, "WAD lump outside file");
    }
    return Reason::OK;
}

inline Reason Sprite(const void* buffer, size_t size, UtilAssetsIntegrityCheckResult* result)
{
    if (result)
        result->ReasonStr[0] = 0;
    constexpr size_t frameHeader = sizeof(SpriteFrame);
    const auto*      p           = static_cast<const Byte*>(buffer);
    if (!p || size < sizeof(SpriteHeader) + sizeof(std::uint16_t))
        return Fail(result, "Truncated sprite header");
    const auto header = Read<SpriteHeader>(p);
    if (std::memcmp(header.signature, "IDSP", 4) || header.version != 2)
        return Fail(result, "Expected version 2 sprite", Reason::InvalidFormat);
    if (header.orientation < 0 || header.orientation > 4 || header.textureFormat < 0 || header.textureFormat > 3 || header.syncType < 0 || header.syncType > 1)
        return Fail(result, "Invalid sprite render or sync type", Reason::InvalidFormat);
    if (header.width <= 0 || header.height <= 0 || !std::isfinite(header.beamLength) || !std::isfinite(header.radius))
        return Fail(result, "Invalid sprite bounds", Reason::InvalidFormat);
    const auto frames = header.frames;
    const auto colors = Read<uint16_t>(p + sizeof(header));
    if (colors != PaletteColors)
        return Fail(result, "Expected 256 sprite palette colors", Reason::InvalidFormat);
    size_t cursor = sizeof(header) + sizeof(colors) + colors * 3;
    if (cursor > size || size > INT_MAX || frames <= 0 || size_t(frames) > (size - cursor) / (4 + frameHeader + 1))
        return Fail(result, "Sprite palette or frames outside file");
    for (int i = 0; i < frames; ++i)
    {
        if (size - cursor < 4)
            return Fail(result, "Missing sprite frame type");
        const auto type = Read<int32_t>(p + cursor);
        cursor += 4;
        int count = 1;
        if (type == 1)
        {
            if (size - cursor < 4)
                return Fail(result, "Missing sprite group count");
            count = Read<int32_t>(p + cursor);
            cursor += 4;
            if (count <= 0 || size_t(count) > (size - cursor) / (sizeof(float) + frameHeader + 1) ||
                std::int64_t(i) * 100 + count - 1 > INT_MAX)
                return Fail(result, "Invalid sprite group count");
            for (int j = 0; j < count; ++j)
            {
                const float interval = Read<float>(p + cursor);
                if (!std::isfinite(interval) || interval <= 0)
                    return Fail(result, "Invalid sprite group interval", Reason::InvalidFormat);
                cursor += sizeof(float);
            }
        }
        else if (type != 0)
            return Fail(result, "Invalid sprite frame type", Reason::InvalidFormat);
        for (int j = 0; j < count; ++j)
        {
            if (size - cursor < frameHeader)
                return Fail(result, "Truncated sprite frame header");
            const auto frame = Read<SpriteFrame>(p + cursor);
            const int  width = frame.width, height = frame.height;
            cursor += frameHeader;
            if (width <= 0 || height <= 0 || width > MaxTextureSide || height > MaxTextureSide || frame.x > INT_MAX - width || frame.y < INT_MIN + height ||
                size_t(width) > (size - cursor) / size_t(height))
                return Fail(result, "Invalid sprite frame dimensions or origin");
            cursor += size_t(width) * height;
        }
    }
    return Reason::OK;
}
} // namespace TextureAssetValidation
