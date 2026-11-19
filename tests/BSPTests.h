#pragma once

#include <climits>
#include <limits>

//A complete single-face world, kept lump by lump so each case breaks one field. The layout follows the BSP version 30
//records independently of the validator.
struct BSPFixture
{
    enum Lump
    {
        Entities,
        Planes,
        Textures,
        Vertexes,
        Visibility,
        Nodes,
        TexInfos,
        Faces,
        Lighting,
        ClipNodes,
        Leafs,
        MarkSurfaces,
        Edges,
        SurfEdges,
        Models,
        LumpCount
    };

    struct Plane
    {
        float        normal[3];
        float        dist;
        std::int32_t type;
    };

    struct Miptex
    {
        char          name[16];
        std::uint32_t width, height;
        std::uint32_t offsets[4];
    };

    struct Node
    {
        std::int32_t  plane;
        std::int16_t  children[2];
        std::int16_t  mins[3], maxs[3];
        std::uint16_t firstFace, faceCount;
    };

    struct TexInfo
    {
        float        vecs[2][4];
        std::int32_t miptex, flags;
    };

    struct Face
    {
        std::int16_t plane, side;
        std::int32_t firstEdge;
        std::int16_t edgeCount, texInfo;
        std::uint8_t styles[4];
        std::int32_t lightOffset;
    };

    struct ClipNode
    {
        std::int32_t plane;
        std::int16_t children[2];
    };

    struct Leaf
    {
        std::int32_t  contents, visOffset;
        std::int16_t  mins[3], maxs[3];
        std::uint16_t firstMark, markCount;
        std::uint8_t  ambient[4];
    };

    struct Edge
    {
        std::uint16_t v[2];
    };

    struct Model
    {
        float        mins[3], maxs[3], origin[3];
        std::int32_t headNodes[4];
        std::int32_t visLeaves, firstFace, faceCount;
    };

    static_assert(sizeof(Plane) == 20 && sizeof(Miptex) == 40 && sizeof(Node) == 24 && sizeof(TexInfo) == 40, "BSP records");
    static_assert(sizeof(Face) == 20 && sizeof(ClipNode) == 8 && sizeof(Leaf) == 28 && sizeof(Model) == 64, "BSP records");

    static constexpr int    Version       = 30;
    static constexpr size_t HeaderBytes   = sizeof(std::int32_t) + LumpCount * 2 * sizeof(std::int32_t);
    static constexpr int    Side          = 16;
    static constexpr size_t MipBytes      = Side * Side * 85 / 64;
    static constexpr size_t PaletteColors = 256;
    static constexpr size_t PaletteBytes  = PaletteColors * 3;
    //2x2 luxels of one style: the face spans one 16 texel luxel on each axis.
    static constexpr size_t LightBytes = 2 * 2 * 3;

    //Miptex 0 starts after the count and its one offset.
    static constexpr size_t MiptexOffset = 2 * sizeof(std::int32_t);

    std::vector<std::uint8_t> lumps[LumpCount];

    template <class T> void Append(Lump lump, const T& value)
    {
        auto& bytes = lumps[lump];
        bytes.resize(bytes.size() + sizeof(value));
        std::memcpy(bytes.data() + bytes.size() - sizeof(value), &value, sizeof(value));
    }

    template <class T> T Get(Lump lump, size_t index = 0, size_t base = 0) const
    {
        T value{};
        std::memcpy(&value, lumps[lump].data() + base + index * sizeof(T), sizeof(value));
        return value;
    }

    template <class T> void Set(Lump lump, const T& value, size_t index = 0, size_t base = 0)
    {
        std::memcpy(lumps[lump].data() + base + index * sizeof(T), &value, sizeof(value));
    }

    BSPFixture()
    {
        const char entities[] = "{\n\"classname\" \"worldspawn\"\n}\n";
        lumps[Entities].assign(entities, entities + sizeof(entities));

        Append(Planes, Plane{{0, 0, 1}, 0, 2});

        Append(Textures, std::int32_t{1});
        Append(Textures, static_cast<std::int32_t>(MiptexOffset));
        Miptex miptex{"fixture", Side, Side, {sizeof(Miptex), 0, 0, 0}};
        Append(Textures, miptex);
        lumps[Textures].resize(lumps[Textures].size() + MipBytes);
        Append(Textures, static_cast<std::uint16_t>(PaletteColors));
        lumps[Textures].resize(lumps[Textures].size() + PaletteBytes);

        const float vertices[][3] = {{0, 0, 0}, {Side, 0, 0}, {0, Side, 0}};
        for (const auto& vertex : vertices)
            Append(Vertexes, vertex);

        Append(Visibility, std::uint8_t{1});

        Append(Nodes, Node{0, {-1, -2}, {0, 0, 0}, {Side, Side, 0}, 0, 1});
        Append(TexInfos, TexInfo{{{1, 0, 0, 0}, {0, 1, 0, 0}}, 0, 0});
        Append(Faces, Face{0, 0, 0, 3, 0, {0, 255, 255, 255}, 0});
        lumps[Lighting].resize(LightBytes);
        Append(ClipNodes, ClipNode{0, {-1, -2}});

        //Leaf 0 is the solid leaf; leaf 1 sees itself.
        Append(Leafs, Leaf{-2, -1, {}, {}, 0, 0, {}});
        Append(Leafs, Leaf{-1, 0, {0, 0, 0}, {Side, Side, 0}, 0, 1, {}});
        Append(MarkSurfaces, std::uint16_t{0});

        //Edge 0 is never used: negative surface edges walk an edge backwards.
        const Edge edges[] = {{{0, 0}}, {{0, 1}}, {{1, 2}}, {{2, 0}}};
        for (const auto& edge : edges)
            Append(Edges, edge);
        for (std::int32_t edge = 1; edge <= 3; ++edge)
            Append(SurfEdges, edge);

        Append(Models, Model{{0, 0, 0}, {Side, Side, 0}, {0, 0, 0}, {0, 0, 0, 0}, 1, 0, 1});
    }

    std::vector<std::uint8_t> Build(UtilAssetsIntegrityBSPLayout layout = UtilAssetsIntegrityBSPLayout::GoldSrc) const
    {
        std::vector<std::uint8_t> file(HeaderBytes);
        const std::int32_t        version = Version;
        std::memcpy(file.data(), &version, sizeof(version));

        for (int lump = 0; lump < LumpCount; ++lump)
        {
            //Blue Shift's table holds the planes in the entities entry and the entities in the planes entry.
            auto slot = lump;
            if (layout == UtilAssetsIntegrityBSPLayout::BlueShift && (lump == Entities || lump == Planes))
                slot = lump == Entities ? Planes : Entities;

            file.resize((file.size() + 3) & ~size_t(3));
            const std::int32_t entry[] = {static_cast<std::int32_t>(file.size()), static_cast<std::int32_t>(lumps[lump].size())};
            std::memcpy(file.data() + sizeof(version) + slot * sizeof(entry), entry, sizeof(entry));
            file.insert(file.end(), lumps[lump].begin(), lumps[lump].end());
        }

        return file;
    }
};

void CheckBSPs(IUtilAssetsIntegrity& api)
{
    using Reason = UtilAssetsIntegrityCheckReason;
    using Layout = UtilAssetsIntegrityBSPLayout;
    using F      = BSPFixture;

    UtilAssetsIntegrityCheckResult_BSP result;
    result.MaxLeafs = 2;

    const auto check = [&](const std::vector<std::uint8_t>& file, Layout layout = Layout::GoldSrc) {
        return api.CheckBSP(file.data(), file.size(), layout, &result);
    };
    const auto expect = [&](Reason expected, const F& fixture, const char* scenario) {
        Expect(expected, check(fixture.Build()), scenario);
    };

    const F valid;
    expect(Reason::OK, valid, "BSP fixture");
    Check(result.ReasonStr[0] == '\0', "BSP fixture diagnostic is empty");
    const auto file = valid.Build();
    Expect(Reason::OK, api.CheckBSP(file.data(), file.size(), Layout::GoldSrc, nullptr), "BSP without result");

    //Header and lump table.
    Expect(Reason::SizeTooSmall, api.CheckBSP(nullptr, 0, Layout::GoldSrc, &result), "empty BSP");
    Check(result.ReasonStr[0] != '\0', "empty BSP diagnostic");
    Expect(Reason::SizeTooSmall, api.CheckBSP(file.data(), F::HeaderBytes - 1, Layout::GoldSrc, &result), "truncated BSP header");
    Expect(Reason::InvalidFormat, check(file, static_cast<Layout>(2)), "unknown BSP layout");
    {
        auto bytes = file;
        bytes[0]   = 29;
        Expect(Reason::VersionMismatch, check(bytes), "BSP version 29");
    }
    {
        //The models are the last lump: one more byte runs past the file.
        auto               bytes  = file;
        const auto         entry  = sizeof(std::int32_t) + F::Models * 2 * sizeof(std::int32_t);
        const std::int32_t longer = static_cast<std::int32_t>(valid.lumps[F::Models].size() + 1);
        std::memcpy(bytes.data() + entry + sizeof(std::int32_t), &longer, sizeof(longer));
        Expect(Reason::OutOfBound, check(bytes), "BSP lump past file end");
        const std::int32_t negative = -1;
        std::memcpy(bytes.data() + entry, &negative, sizeof(negative));
        Expect(Reason::OutOfBound, check(bytes), "BSP negative lump offset");
    }
    {
        F f;
        f.lumps[F::Planes].push_back(0);
        expect(Reason::InvalidFormat, f, "BSP partial plane record");
    }
    {
        F f;
        f.lumps[F::Models].clear();
        expect(Reason::InvalidFormat, f, "BSP without models");
    }

    //The caller's leaf capacity.
    result.MaxLeafs = 1;
    expect(Reason::SizeTooLarge, valid, "BSP leaf capacity");
    UtilAssetsIntegrityCheckResult_BSP defaults;
    Expect(Reason::SizeTooLarge, api.CheckBSP(file.data(), file.size(), Layout::GoldSrc, &defaults), "BSP zero leaf capacity");
    result.MaxLeafs = 2;

    {
        F f;
        //Condition Zero: Deleted Scenes ends its entities with a space; callers copy them with a terminator.
        f.lumps[F::Entities].back() = ' ';
        expect(Reason::OK, f, "BSP unterminated entities");
    }

    //Planes, vertices, edges and surface edges.
    {
        F    f;
        auto plane      = f.Get<F::Plane>(F::Planes);
        plane.normal[0] = std::numeric_limits<float>::quiet_NaN();
        f.Set(F::Planes, plane);
        expect(Reason::InvalidFormat, f, "BSP non-finite plane");
    }
    {
        F f;
        f.Set(F::Vertexes, std::numeric_limits<float>::infinity(), 4);
        expect(Reason::InvalidFormat, f, "BSP non-finite vertex");
    }
    {
        F f;
        f.Set(F::Edges, F::Edge{{0, 3}}, 1);
        expect(Reason::OutOfBound, f, "BSP edge vertex out of range");
    }
    {
        F f;
        f.Set(F::SurfEdges, std::int32_t{-4}, 1);
        expect(Reason::OutOfBound, f, "BSP backward surface edge out of range");
        f.Set(F::SurfEdges, std::int32_t{INT_MIN}, 1);
        expect(Reason::OutOfBound, f, "BSP surface edge INT_MIN");
    }

    //Texture lump.
    {
        F f;
        f.lumps[F::Textures].resize(2);
        expect(Reason::OutOfBound, f, "BSP truncated texture lump");
    }
    {
        F f;
        f.Set(F::Textures, std::int32_t{-1});
        expect(Reason::OutOfBound, f, "BSP negative miptex count");
        f.Set(F::Textures, std::int32_t{INT_MAX});
        expect(Reason::OutOfBound, f, "BSP miptex table past lump");
    }
    {
        F f;
        f.Set(F::Textures, static_cast<std::int32_t>(f.lumps[F::Textures].size() - sizeof(F::Miptex) + 1), 1);
        expect(Reason::OutOfBound, f, "BSP miptex past lump");
        f.Set(F::Textures, std::int32_t{-2}, 1);
        expect(Reason::OutOfBound, f, "BSP negative miptex offset");
        f.Set(F::Textures, std::int32_t{-1}, 1);
        expect(Reason::OK, f, "BSP absent miptex");
    }
    {
        F    f;
        auto miptex = f.Get<F::Miptex>(F::Textures, 0, F::MiptexOffset);
        std::memset(miptex.name, 'a', sizeof(miptex.name));
        f.Set(F::Textures, miptex, 0, F::MiptexOffset);
        expect(Reason::OutOfBound, f, "BSP unterminated miptex name");
    }
    {
        F    f;
        auto miptex  = f.Get<F::Miptex>(F::Textures, 0, F::MiptexOffset);
        miptex.width = 0;
        f.Set(F::Textures, miptex, 0, F::MiptexOffset);
        expect(Reason::InvalidFormat, f, "BSP zero miptex width");
        miptex.width = 32768;
        f.Set(F::Textures, miptex, 0, F::MiptexOffset);
        expect(Reason::InvalidFormat, f, "BSP oversized miptex");
        miptex.width = 24;
        f.Set(F::Textures, miptex, 0, F::MiptexOffset);
        expect(Reason::InvalidFormat, f, "BSP unaligned embedded miptex");
        //A WAD texture's pixels are not in the file.
        miptex.offsets[0] = 0;
        f.Set(F::Textures, miptex, 0, F::MiptexOffset);
        expect(Reason::OK, f, "BSP unaligned WAD miptex");
    }
    {
        F f;
        f.lumps[F::Textures].pop_back();
        expect(Reason::OutOfBound, f, "BSP miptex palette past lump");
    }
    {
        //The pixels and palette count of a 32x32 miptex no longer fit.
        F    f;
        auto miptex   = f.Get<F::Miptex>(F::Textures, 0, F::MiptexOffset);
        miptex.width  = 2 * F::Side;
        miptex.height = 2 * F::Side;
        f.Set(F::Textures, miptex, 0, F::MiptexOffset);
        expect(Reason::OutOfBound, f, "BSP miptex pixels past lump");
    }
    {
        const auto count = F::MiptexOffset + sizeof(F::Miptex) + F::MipBytes;
        F          f;
        f.Set(F::Textures, std::uint16_t{16}, 0, count);
        expect(Reason::OK, f, "BSP short palette count");
        f.lumps[F::Textures].pop_back();
        expect(Reason::OutOfBound, f, "BSP short palette count still reads 256 colors");
        f = F();
        f.Set(F::Textures, std::uint16_t{257}, 0, count);
        expect(Reason::OutOfBound, f, "BSP palette count past lump");
    }

    //Texture infos.
    {
        F    f;
        auto texInfo   = f.Get<F::TexInfo>(F::TexInfos);
        texInfo.miptex = 1;
        f.Set(F::TexInfos, texInfo);
        expect(Reason::OutOfBound, f, "BSP texinfo miptex out of range");
        texInfo.miptex = -1;
        f.Set(F::TexInfos, texInfo);
        expect(Reason::OutOfBound, f, "BSP negative texinfo miptex");
        //Without a texture lump every face is drawn with the missing texture.
        f.lumps[F::Textures].clear();
        expect(Reason::OK, f, "BSP texinfo without texture lump");
    }
    {
        F    f;
        auto texInfo       = f.Get<F::TexInfo>(F::TexInfos);
        texInfo.vecs[1][3] = std::numeric_limits<float>::infinity();
        f.Set(F::TexInfos, texInfo);
        expect(Reason::InvalidFormat, f, "BSP non-finite texture vector");
        texInfo.vecs[1][3] = 1e30f;
        f.Set(F::TexInfos, texInfo);
        expect(Reason::InvalidFormat, f, "BSP texture coordinate overflow");
    }

    //Faces.
    const auto face = [](F& f, void (*change)(F::Face&)) {
        auto value = f.Get<F::Face>(F::Faces);
        change(value);
        f.Set(F::Faces, value);
    };
    {
        F f;
        face(f, [](F::Face& value) { value.plane = 1; });
        expect(Reason::OutOfBound, f, "BSP face plane out of range");
        face(f, [](F::Face& value) { value.plane = -1; });
        expect(Reason::OutOfBound, f, "BSP negative face plane");
    }
    {
        F f;
        face(f, [](F::Face& value) { value.texInfo = 1; });
        expect(Reason::OutOfBound, f, "BSP face texinfo out of range");
    }
    {
        F f;
        face(f, [](F::Face& value) { value.edgeCount = 0; });
        expect(Reason::OutOfBound, f, "BSP face without edges");
        face(f, [](F::Face& value) { value.edgeCount = -1; });
        expect(Reason::OutOfBound, f, "BSP negative face edge count");
        face(f, [](F::Face& value) { value.edgeCount = 3, value.firstEdge = 1; });
        expect(Reason::OutOfBound, f, "BSP face edges past surface edges");
        face(f, [](F::Face& value) { value.firstEdge = INT_MAX; });
        expect(Reason::OutOfBound, f, "BSP face edge range overflow");
    }
    {
        F f;
        face(f, [](F::Face& value) { value.lightOffset = -2; });
        expect(Reason::OutOfBound, f, "BSP negative light offset");
        face(f, [](F::Face& value) { value.lightOffset = -1; });
        expect(Reason::OK, f, "BSP face without light samples");
    }
    {
        F f;
        f.lumps[F::Lighting].pop_back();
        expect(Reason::OutOfBound, f, "BSP light samples past lighting");
        f.lumps[F::Lighting].clear();
        expect(Reason::OK, f, "BSP unlit file with light offset 0");
        face(f, [](F::Face& value) { value.lightOffset = 3; });
        expect(Reason::OutOfBound, f, "BSP light offset without lighting");
    }
    {
        F f;
        face(f, [](F::Face& value) { value.styles[1] = 1; });
        expect(Reason::OutOfBound, f, "BSP second light style past lighting");
    }
    {
        //Texture coordinates up to 640 make the face 640 texels wide.
        F    f;
        auto texInfo       = f.Get<F::TexInfo>(F::TexInfos);
        texInfo.vecs[0][0] = 40;
        f.Set(F::TexInfos, texInfo);
        expect(Reason::InvalidFormat, f, "BSP surface extents over 512");
        texInfo.flags = 1;
        f.Set(F::TexInfos, texInfo);
        expect(Reason::OK, f, "BSP large special surface has no lightmap");
    }
    {
        //A missing texture clears a special face's flags, so its light samples can be read.
        F    f;
        auto texInfo  = f.Get<F::TexInfo>(F::TexInfos);
        texInfo.flags = 1;
        f.Set(F::TexInfos, texInfo);
        f.lumps[F::Lighting].pop_back();
        expect(Reason::OutOfBound, f, "BSP small special surface light samples");
    }

    //Mark surfaces are unsigned.
    {
        F f;
        f.Set(F::MarkSurfaces, std::uint16_t{1});
        expect(Reason::OutOfBound, f, "BSP mark surface out of range");
        f.Set(F::MarkSurfaces, std::uint16_t{0x8000});
        expect(Reason::OutOfBound, f, "BSP mark surface past signed range");
    }

    //Models.
    const auto model = [](F& f, void (*change)(F::Model&)) {
        auto value = f.Get<F::Model>(F::Models);
        change(value);
        f.Set(F::Models, value);
    };
    {
        F f;
        model(f, [](F::Model& value) { value.headNodes[0] = 1; });
        expect(Reason::OutOfBound, f, "BSP model head node out of range");
        model(f, [](F::Model& value) { value.headNodes[0] = -1; });
        expect(Reason::OutOfBound, f, "BSP negative model head node");
    }
    {
        F f;
        //The clipnode count is the head compilers give a model without clipping brushes.
        model(f, [](F::Model& value) { value.headNodes[1] = 1; });
        expect(Reason::OK, f, "BSP model clipping head at the clipnode count");
        model(f, [](F::Model& value) { value.headNodes[1] = 2; });
        expect(Reason::OutOfBound, f, "BSP model clipping head out of range");
        model(f, [](F::Model& value) { value.headNodes[1] = -1; });
        expect(Reason::OK, f, "BSP model with an empty hull");
    }
    {
        F f;
        model(f, [](F::Model& value) { value.visLeaves = 2; });
        expect(Reason::OutOfBound, f, "BSP model visible leaves out of range");
        model(f, [](F::Model& value) { value.visLeaves = -1; });
        expect(Reason::OutOfBound, f, "BSP negative model visible leaves");
    }
    {
        F f;
        model(f, [](F::Model& value) { value.firstFace = 1; });
        expect(Reason::OutOfBound, f, "BSP model faces out of range");
        model(f, [](F::Model& value) { value.firstFace = 0, value.faceCount = -1; });
        expect(Reason::OutOfBound, f, "BSP negative model face count");
    }
    {
        F f;
        model(f, [](F::Model& value) { value.origin[2] = std::numeric_limits<float>::quiet_NaN(); });
        expect(Reason::InvalidFormat, f, "BSP non-finite model origin");
        model(f, [](F::Model& value) { value.origin[2] = 0, value.mins[0] = F::Side + 1; });
        expect(Reason::InvalidFormat, f, "BSP inverted model bounds");
    }

    //Nodes.
    const auto node = [](F& f, void (*change)(F::Node&), size_t index = 0) {
        auto value = f.Get<F::Node>(F::Nodes, index);
        change(value);
        f.Set(F::Nodes, value, index);
    };
    {
        F f;
        node(f, [](F::Node& value) { value.plane = 1; });
        expect(Reason::OutOfBound, f, "BSP node plane out of range");
    }
    {
        F f;
        node(f, [](F::Node& value) { value.children[0] = 1; });
        expect(Reason::OutOfBound, f, "BSP node child out of range");
        node(f, [](F::Node& value) { value.children[0] = -3; });
        expect(Reason::OutOfBound, f, "BSP node leaf out of range");
    }
    {
        F f;
        node(f, [](F::Node& value) { value.faceCount = 2; });
        expect(Reason::OutOfBound, f, "BSP node faces out of range");
    }

    //Leaves and visibility rows; model 0's one visible leaf makes the rows one byte long.
    const auto leaf = [](F& f, void (*change)(F::Leaf&)) {
        auto value = f.Get<F::Leaf>(F::Leafs, 1);
        change(value);
        f.Set(F::Leafs, value, 1);
    };
    {
        F f;
        leaf(f, [](F::Leaf& value) { value.contents = 0; });
        expect(Reason::InvalidFormat, f, "BSP leaf with node contents");
    }
    {
        F f;
        leaf(f, [](F::Leaf& value) { value.firstMark = 1; });
        expect(Reason::OutOfBound, f, "BSP leaf mark surfaces out of range");
        leaf(f, [](F::Leaf& value) { value.firstMark = 0, value.markCount = 2; });
        expect(Reason::OutOfBound, f, "BSP leaf mark surface count out of range");
    }
    {
        F f;
        leaf(f, [](F::Leaf& value) { value.visOffset = 1; });
        expect(Reason::OutOfBound, f, "BSP visibility offset past lump");
        leaf(f, [](F::Leaf& value) { value.visOffset = -2; });
        expect(Reason::OutOfBound, f, "BSP negative visibility offset");
        leaf(f, [](F::Leaf& value) { value.visOffset = 0; });
        f.lumps[F::Visibility].clear();
        expect(Reason::OK, f, "BSP visibility offset 0 without visibility");
        leaf(f, [](F::Leaf& value) { value.visOffset = 1; });
        expect(Reason::OutOfBound, f, "BSP visibility offset without visibility");
    }
    {
        F f;
        f.lumps[F::Visibility] = {0, 1};
        expect(Reason::OK, f, "BSP visibility run filling its row");
        //Compiled rows are padded to 64 leaves: a run may end 8 bytes in, not 9.
        f.lumps[F::Visibility] = {0, 8};
        expect(Reason::OK, f, "BSP visibility run to the padded row");
        f.lumps[F::Visibility] = {0, 9};
        expect(Reason::OutOfBound, f, "BSP visibility run past the padded row");
        f.lumps[F::Visibility] = {0};
        expect(Reason::OutOfBound, f, "BSP truncated visibility run");
    }
    {
        //A row compiled for all 100 leaves is 13 bytes long.
        F f;
        for (int i = 2; i < 100; ++i)
            f.Append(F::Leafs, F::Leaf{-2, -1, {}, {}, 0, 0, {}});
        result.MaxLeafs        = 100;
        f.lumps[F::Visibility] = {0, 13};
        expect(Reason::OK, f, "BSP visibility run to the row of every leaf");
        f.lumps[F::Visibility] = {0, 14};
        expect(Reason::OutOfBound, f, "BSP visibility run past the row of every leaf");
        result.MaxLeafs = 2;
    }
    {
        //An empty row still decodes once, within the row of every leaf.
        F f;
        model(f, [](F::Model& value) { value.visLeaves = 0; });
        f.lumps[F::Visibility] = {0, 0};
        expect(Reason::OK, f, "BSP empty visibility row");
        f.lumps[F::Visibility] = {0, 2};
        expect(Reason::OutOfBound, f, "BSP visibility run past an empty row");
    }

    //Clipnodes.
    const auto clipNode = [](F& f, void (*change)(F::ClipNode&)) {
        auto value = f.Get<F::ClipNode>(F::ClipNodes);
        change(value);
        f.Set(F::ClipNodes, value);
    };
    {
        F f;
        clipNode(f, [](F::ClipNode& value) { value.plane = 1; });
        expect(Reason::OutOfBound, f, "BSP clipnode plane out of range");
        clipNode(f, [](F::ClipNode& value) { value.plane = 0, value.children[1] = 1; });
        expect(Reason::OutOfBound, f, "BSP clipnode child out of range");
    }

    //Node graphs.
    {
        F f;
        node(f, [](F::Node& value) { value.children[1] = 0; });
        expect(Reason::InvalidFormat, f, "BSP node cycle");
    }
    {
        F f;
        clipNode(f, [](F::ClipNode& value) { value.children[1] = 0; });
        expect(Reason::InvalidFormat, f, "BSP clipnode cycle");
    }
    {
        //Node 0 reaches node 1 twice; clipnodes may share subtrees.
        F f;
        f.Append(F::Nodes, F::Node{0, {-1, -2}, {}, {}, 0, 0});
        node(f, [](F::Node& value) { value.children[0] = 1, value.children[1] = 1; });
        expect(Reason::InvalidFormat, f, "BSP shared drawing node");

        f = F();
        f.Append(F::ClipNodes, F::ClipNode{0, {-1, -2}});
        clipNode(f, [](F::ClipNode& value) { value.children[0] = 1, value.children[1] = 1; });
        expect(Reason::OK, f, "BSP shared clipnode");
    }
    {
        //A chain of nodes, each one level deeper.
        const auto chain = [](int depth) {
            F f;
            for (int i = 1; i < depth; ++i)
                f.Append(F::Nodes, F::Node{0, {-1, -2}, {}, {}, 0, 0});
            for (int i = 0; i + 1 < depth; ++i)
            {
                auto value        = f.Get<F::Node>(F::Nodes, i);
                value.children[0] = static_cast<std::int16_t>(i + 1);
                f.Set(F::Nodes, value, i);
            }
            return f;
        };
        constexpr int maxDepth = 1024;
        expect(Reason::OK, chain(maxDepth), "BSP node chain at the depth limit");
        expect(Reason::InvalidFormat, chain(maxDepth + 1), "BSP node chain past the depth limit");
    }

    //Blue Shift swaps the entities and planes entries.
    {
        Check(valid.lumps[F::Entities].size() % sizeof(F::Plane) != 0, "BSP fixture entities are not whole planes");
        const auto blueShift = valid.Build(Layout::BlueShift);
        Expect(Reason::OK, check(blueShift, Layout::BlueShift), "Blue Shift BSP");
        Expect(Reason::InvalidFormat, check(blueShift, Layout::GoldSrc), "Blue Shift BSP read as GoldSrc");
        Expect(Reason::InvalidFormat, check(file, Layout::BlueShift), "GoldSrc BSP read as Blue Shift");
    }
}
