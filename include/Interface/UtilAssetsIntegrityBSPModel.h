#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

//A complete 64-byte disk model is required. Some maps append a marker with an opaque first word,
//origin.z = 9999.0f (little endian), and every other field zero. Its first word is not a bound.
inline bool UtilAssetsIntegrity_IsBSPModelMarker(const void* record, std::size_t index, std::size_t count)
{
    if (!index || index + 1 != count)
        return false;

    constexpr std::size_t  originZ = 8 * sizeof(float);
    constexpr std::size_t  heads   = originZ + sizeof(float);
    constexpr std::uint8_t zeros[64]{};
    constexpr std::uint8_t tag[] = {0x00, 0x3c, 0x1c, 0x46};
    const auto*            bytes = static_cast<const std::uint8_t*>(record);
    return std::memcmp(bytes + originZ, tag, sizeof(tag)) == 0 &&
        std::memcmp(bytes + sizeof(float), zeros, originZ - sizeof(float)) == 0 &&
        std::memcmp(bytes + heads, zeros, sizeof(zeros) - heads) == 0;
}
