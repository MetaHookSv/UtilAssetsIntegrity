#pragma once

//Independent disk fixtures. Each mutation starts with a valid asset.
using AssetBytes = std::vector<std::uint8_t>;
template <typename T>
void SetAssetField(AssetBytes& bytes, size_t offset, T value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void CheckTextureAssets(IUtilAssetsIntegrity& api)
{
    using Reason = UtilAssetsIntegrityCheckReason;
    auto check   = [&](auto method, const AssetBytes& bytes, bool valid, const char* label) {
        UtilAssetsIntegrityCheckResult result;
        const auto                     reason = (api.*method)(bytes.data(), bytes.size(), &result);
        Check((reason == Reason::OK) == valid && (valid || result.ReasonStr[0]), label);
    };
    constexpr size_t palette = 42, frameType = palette + 768;
    AssetBytes       sprite(frameType + 4 + 16 + 1);
    std::memcpy(sprite.data(), "IDSP", 4);
    SetAssetField(sprite, 4, 2);
    for (const size_t offset : {20, 24, 28})
        SetAssetField(sprite, offset, 1);
    SetAssetField(sprite, 40, std::uint16_t{256});
    SetAssetField(sprite, frameType + 12, 1);
    SetAssetField(sprite, frameType + 16, 1);
    const auto spr    = &IUtilAssetsIntegrity::CheckSprite;
    const auto wad    = &IUtilAssetsIntegrity::CheckWADDirectory;
    const auto mip    = &IUtilAssetsIntegrity::CheckMiptex;
    auto       mutate = [&](auto method, AssetBytes bytes, size_t offset, auto value, const char* label) {
        SetAssetField(bytes, offset, value);
        check(method, bytes, false, label);
    };
    check(spr, sprite, true, "single sprite");
    for (const std::uint16_t colors : {1, 2, 255})
    {
        auto compact = sprite;
        compact.erase(compact.begin() + palette + colors * 3, compact.begin() + frameType);
        SetAssetField(compact, 40, colors);
        compact.back() = static_cast<std::uint8_t>(colors - 1);
        check(spr, compact, true, "single sprite with compact palette");
        compact.pop_back();
        check(spr, compact, false, "truncated compact sprite");
    }
    mutate(spr, sprite, 0, 0, "sprite signature");
    mutate(spr, sprite, 4, 3, "sprite version");
    mutate(spr, sprite, 8, 5, "sprite orientation");
    mutate(spr, sprite, 12, 4, "sprite texture format");
    mutate(spr, sprite, 16, std::numeric_limits<float>::quiet_NaN(), "sprite radius");
    mutate(spr, sprite, 20, 0, "sprite width");
    mutate(spr, sprite, 24, -1, "sprite height");
    mutate(spr, sprite, 28, INT_MAX, "sprite frame count");
    mutate(spr, sprite, 32, std::numeric_limits<float>::infinity(), "sprite beam length");
    mutate(spr, sprite, 36, 2, "sprite sync type");
    mutate(spr, sprite, 40, std::uint16_t{0}, "empty sprite palette");
    mutate(spr, sprite, 40, std::uint16_t{257}, "oversize sprite palette");
    mutate(spr, sprite, frameType, 2, "sprite frame type");
    mutate(spr, sprite, frameType + 4, INT_MAX, "sprite right edge overflow");
    mutate(spr, sprite, frameType + 8, INT_MIN, "sprite bottom edge overflow");
    mutate(spr, sprite, frameType + 12, 0, "sprite frame width");
    mutate(spr, sprite, frameType + 16, INT_MAX, "sprite frame height");
    sprite.insert(sprite.begin() + frameType + 4, 8, 0);
    SetAssetField(sprite, frameType, 1);
    SetAssetField(sprite, frameType + 4, 1);
    SetAssetField(sprite, frameType + 8, 0.5f);
    check(spr, sprite, true, "group sprite");
    mutate(spr, sprite, frameType + 4, 0, "empty sprite group");
    mutate(spr, sprite, frameType + 4, INT_MAX, "oversize sprite group");
    mutate(spr, sprite, frameType + 8, 0.0f, "zero group interval");
    mutate(spr, sprite, frameType + 8, std::numeric_limits<float>::quiet_NaN(), "nonfinite group interval");
    for (size_t length = 0; length < sprite.size(); ++length)
        Check((api.*spr)(sprite.data(), length, nullptr) != Reason::OK, "truncated group sprite");

    AssetBytes directory(12 + 32);
    std::memcpy(directory.data(), "WAD2", 4);
    SetAssetField(directory, 4, 1);
    SetAssetField(directory, 8, 12);
    check(wad, directory, true, "WAD2 directory");
    directory[3] = '3';
    check(wad, directory, true, "WAD3 directory with non-texture entry");
    mutate(wad, directory, 0, 0, "WAD signature");
    mutate(wad, directory, 4, -1, "negative WAD count");
    mutate(wad, directory, 4, INT_MAX, "WAD directory size");
    mutate(wad, directory, 8, -1, "negative WAD directory offset");
    mutate(wad, directory, 8, 13, "truncated WAD directory");
    mutate(wad, directory, 12, -1, "negative WAD filepos");
    mutate(wad, directory, 12, INT_MAX, "WAD filepos outside file");
    mutate(wad, directory, 16, -1, "negative WAD disksize");
    mutate(wad, directory, 16, 45, "WAD disksize outside file");
    directory.resize(11);
    check(wad, directory, false, "truncated WAD header");

    BSPFixture  bsp;
    const auto& lump = bsp.lumps[BSPFixture::Textures];
    AssetBytes  texture(lump.begin() + BSPFixture::MiptexOffset, lump.end());
    check(mip, texture, true, "WAD packed miptex");
    for (size_t level = 0; level < 4; ++level)
    {
        mutate(mip, texture, 24 + level * 4, 0, "WAD mip offset");
        if (level)
        {
            BSPFixture broken;
            broken.Set(BSPFixture::Textures, 0, 0, BSPFixture::MiptexOffset + 24 + level * 4);
            auto bytes = broken.Build();
            Check(api.CheckBSP(bytes.data(), bytes.size(), UtilAssetsIntegrityBSPLayout::GoldSrc, nullptr) != Reason::OK, "BSP mip offset");
        }
    }
    mutate(mip, texture, 16, 17, "WAD miptex alignment");
    mutate(mip, texture, 20, 0, "WAD miptex height");
    texture.pop_back();
    check(mip, texture, false, "truncated WAD palette");
}

struct TextureScanCounts
{
    size_t sprites{}, wads{}, textures{}, rejected{};
};

void ScanTextureAsset(IUtilAssetsIntegrity& api, const std::filesystem::path& path, bool isSprite, TextureScanCounts& counts)
{
    std::ifstream                  input(path, std::ios::binary);
    const AssetBytes               bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    UtilAssetsIntegrityCheckResult result;
    auto                           reject = [&](const std::string& entry) {
        ++counts.rejected;
        std::cerr << path.string() << entry << ": " << result.ReasonStr << '\n';
    };
    if (isSprite)
    {
        ++counts.sprites;
        if (api.CheckSprite(bytes.data(), bytes.size(), &result) != UtilAssetsIntegrityCheckReason::OK)
            reject("");
        return;
    }
    ++counts.wads;
    if (api.CheckWADDirectory(bytes.data(), bytes.size(), &result) != UtilAssetsIntegrityCheckReason::OK)
    {
        reject("");
        return;
    }
    auto readInt = [&](size_t offset) {
        std::int32_t value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    };
    const int  count = readInt(4), directory = readInt(8);
    const auto miptexType = bytes[3] == '3' ? 67 : 68;
    for (int i = 0; i < count; ++i)
    {
        const size_t entry = directory + size_t(i) * 32;
        if (bytes[entry + 12] != miptexType)
            continue;
        ++counts.textures;
        if (bytes[entry + 13])
            std::snprintf(result.ReasonStr, sizeof(result.ReasonStr), "Compressed miptex");
        else if (api.CheckMiptex(bytes.data() + readInt(entry), readInt(entry + 4), &result) == UtilAssetsIntegrityCheckReason::OK)
            continue;
        reject(" lump #" + std::to_string(i));
    }
}
