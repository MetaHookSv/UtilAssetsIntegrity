#pragma once

#include "TextureAssetValidation.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <climits>
#include <cmath>
#include <utility>
#include <vector>

//Validates a version 30 BSP as the GoldSrc engine reads it (Mod_LoadBrushModel, CalcSurfaceExtents, Mod_SetParent, the
//hull traces, RecursiveLightPoint, R_BuildLightMap and CM_DecompressPVS), before any of it is parsed. Records are read
//at their fixed little-endian offsets, so neither an SDK's nor an engine's bspfile.h and MAX_MAP_* capacities are used.
class CBSPValidation
{
public:
    using Reason = UtilAssetsIntegrityCheckReason;
    using Layout = UtilAssetsIntegrityBSPLayout;

    CBSPValidation(const void* pData, size_t size, Layout layout, UtilAssetsIntegrityCheckResult_BSP* pResult) :
        m_pData(static_cast<const std::uint8_t*>(pData)),
        m_Size(size),
        m_Layout(layout),
        m_pResult(pResult)
    {
        if (m_pResult)
            m_pResult->ReasonStr[0] = 0;
    }

    Reason Check()
    {
        if (!m_pData || m_Size < HeaderBytes)
            return Fail("truncated header", Reason::SizeTooSmall);

        if (m_Size > INT_MAX)
            return Fail("file exceeds signed lump offsets", Reason::SizeTooLarge);

        if (m_Layout != Layout::GoldSrc && m_Layout != Layout::BlueShift)
            return Fail("unknown lump layout", Reason::InvalidFormat);

        if (ReadInt32(0) != Version)
            return Fail("expected BSP version 30", Reason::VersionMismatch);

        auto code = Lumps();
        if (code != Reason::OK)
            return code;

        if (m_pResult && Count(LeafLump, LeafBytes) > m_pResult->MaxLeafs)
            return Fail("more leaves than the caller's PVS capacity", Reason::SizeTooLarge);

        if (!Geometry() ||
            !Textures() ||
            !TexInfos() ||
            !Faces() ||
            !MarkSurfaces() ||
            !Models() ||
            !Nodes() ||
            !Leaves() ||
            !ClipNodes() ||
            !Trees())
        {
            return m_Reason;
        }

        return Reason::OK;
    }

private:
    static constexpr std::int32_t Version     = 30;
    static constexpr size_t       LumpCount   = 15;
    static constexpr size_t       HeaderBytes = sizeof(std::int32_t) + LumpCount * 2 * sizeof(std::int32_t);

    //Lumps by meaning; for Blue Shift the entities and planes come from each other's table entry.
    static constexpr size_t EntityLump      = 0;
    static constexpr size_t PlaneLump       = 1;
    static constexpr size_t TextureLump     = 2;
    static constexpr size_t VertexLump      = 3;
    static constexpr size_t VisibilityLump  = 4;
    static constexpr size_t NodeLump        = 5;
    static constexpr size_t TexInfoLump     = 6;
    static constexpr size_t FaceLump        = 7;
    static constexpr size_t LightingLump    = 8;
    static constexpr size_t ClipNodeLump    = 9;
    static constexpr size_t LeafLump        = 10;
    static constexpr size_t MarkSurfaceLump = 11;
    static constexpr size_t EdgeLump        = 12;
    static constexpr size_t SurfEdgeLump    = 13;
    static constexpr size_t ModelLump       = 14;

    static constexpr size_t PlaneBytes       = 20;
    static constexpr size_t VertexBytes      = 12;
    static constexpr size_t NodeBytes        = 24;
    static constexpr size_t TexInfoBytes     = 40;
    static constexpr size_t FaceBytes        = 20;
    static constexpr size_t ClipNodeBytes    = 8;
    static constexpr size_t LeafBytes        = 28;
    static constexpr size_t MarkSurfaceBytes = 2;
    static constexpr size_t EdgeBytes        = 4;
    static constexpr size_t SurfEdgeBytes    = 4;
    static constexpr size_t ModelBytes       = 64;
    static constexpr size_t MiptexBytes      = 40;

    static constexpr size_t NodeChildrenOffset   = 4;
    static constexpr size_t NodeFirstFaceOffset  = 20;
    static constexpr size_t NodeFaceCountOffset  = 22;
    static constexpr size_t ClipChildrenOffset   = 4;
    static constexpr size_t TexInfoMiptexOffset  = 32;
    static constexpr size_t TexInfoFlagsOffset   = 36;
    static constexpr size_t FaceFirstEdgeOffset  = 4;
    static constexpr size_t FaceEdgeCountOffset  = 8;
    static constexpr size_t FaceTexInfoOffset    = 10;
    static constexpr size_t FaceStylesOffset     = 12;
    static constexpr size_t FaceLightOffset      = 16;
    static constexpr size_t LeafVisOffset        = 4;
    static constexpr size_t LeafFirstMarkOffset  = 20;
    static constexpr size_t LeafMarkCountOffset  = 22;
    static constexpr size_t ModelMaxsOffset      = 12;
    static constexpr size_t ModelHeadNodeOffset  = 36;
    static constexpr size_t ModelVisLeafsOffset  = 52;
    static constexpr size_t ModelFirstFaceOffset = 56;
    static constexpr size_t ModelFaceCountOffset = 60;

    static constexpr int Axes         = 3;
    static constexpr int TextureAxes  = 2;
    static constexpr int TextureVecs  = 4;
    static constexpr int Hulls        = 4;
    static constexpr int LightStyles  = 4;
    static constexpr int NoLightStyle = 255;
    static constexpr int LightSample  = 3;
    static constexpr int LuxelSize    = 16;
    static constexpr int TexSpecial   = 1;
    static constexpr int AbsentOffset = -1;
    //A face whose extents exceed this either errors in CalcSurfaceExtents or is TEX_SPECIAL, which has no lightmap.
    static constexpr int MaxSurfaceExtent = 512;
    //CalcSurfaceExtents' starting extremes, kept as its floats.
    static constexpr float ExtentsInitialMin = 999999.0f;
    static constexpr float ExtentsInitialMax = -99999.0f;
    //Texture coordinates beyond this make floor(coordinate / 16) overflow int.
    static constexpr double MaxTextureCoordinate = 268435456.0;
    //Mod_SetParent, the hull traces and the light point recurse once per level; stock maps stay far below this.
    static constexpr int MaxTreeDepth = 1024;

    Reason Fail(const char* text, Reason reason = Reason::OutOfBound)
    {
        if (m_pResult)
            std::snprintf(m_pResult->ReasonStr, sizeof(m_pResult->ReasonStr), "BSP: %s.", text);

        m_Reason = reason;
        return reason;
    }

    bool Bad(const char* text, Reason reason = Reason::OutOfBound)
    {
        Fail(text, reason);
        return false;
    }

    static bool Range(size_t offset, size_t count, size_t stride, size_t limit)
    {
        return offset <= limit && stride && count <= (limit - offset) / stride;
    }

    template <class T>
    T Read(size_t offset) const
    {
        T value;
        std::memcpy(&value, m_pData + offset, sizeof(value));
        return value;
    }

    std::int32_t  ReadInt32(size_t offset) const { return Read<std::int32_t>(offset); }
    std::int16_t  ReadInt16(size_t offset) const { return Read<std::int16_t>(offset); }
    std::uint16_t ReadUInt16(size_t offset) const { return Read<std::uint16_t>(offset); }
    float         ReadFloat(size_t offset) const { return Read<float>(offset); }

    size_t Count(size_t lump, size_t recordBytes) const
    {
        return m_Lengths[lump] / recordBytes;
    }

    //File offset of record index of a lump, which Lumps() has sized.
    size_t Record(size_t lump, size_t index, size_t recordBytes) const
    {
        return m_Offsets[lump] + index * recordBytes;
    }

    static bool Index(std::int64_t value, size_t count)
    {
        return value >= 0 && static_cast<std::uint64_t>(value) < count;
    }

    //[first, first + count) within total, both read as the engine reads them.
    static bool Span(std::int64_t first, std::int64_t count, size_t total)
    {
        return first >= 0 && count >= 0 && Range(static_cast<size_t>(first), static_cast<size_t>(count), 1, total);
    }

    bool Finite(size_t offset, int floats) const
    {
        for (int i = 0; i < floats; ++i)
        {
            if (!std::isfinite(ReadFloat(offset + i * sizeof(float))))
                return false;
        }

        return true;
    }

    Reason Lumps()
    {
        for (size_t i = 0; i < LumpCount; ++i)
        {
            const auto entry  = sizeof(std::int32_t) + i * 2 * sizeof(std::int32_t);
            const auto offset = ReadInt32(entry);
            const auto length = ReadInt32(entry + sizeof(std::int32_t));

            if (offset < 0 || length < 0 || !Range(static_cast<size_t>(offset), static_cast<size_t>(length), 1, m_Size))
                return Fail("lump outside file");

            m_Offsets[i] = static_cast<size_t>(offset);
            m_Lengths[i] = static_cast<size_t>(length);
        }

        if (m_Layout == Layout::BlueShift)
        {
            std::swap(m_Offsets[EntityLump], m_Offsets[PlaneLump]);
            std::swap(m_Lengths[EntityLump], m_Lengths[PlaneLump]);
        }

        //Mod_LoadBmodel's "funny lump size".
        const size_t records[][2] = {
            {PlaneLump, PlaneBytes},
            {VertexLump, VertexBytes},
            {NodeLump, NodeBytes},
            {TexInfoLump, TexInfoBytes},
            {FaceLump, FaceBytes},
            {ClipNodeLump, ClipNodeBytes},
            {LeafLump, LeafBytes},
            {MarkSurfaceLump, MarkSurfaceBytes},
            {EdgeLump, EdgeBytes},
            {SurfEdgeLump, SurfEdgeBytes},
            {ModelLump, ModelBytes},
        };

        for (const auto& record : records)
        {
            if (m_Lengths[record[0]] % record[1])
                return Fail("lump size is not a whole number of records", Reason::InvalidFormat);
        }

        //Model 0 is the world; the engine reads its head node and leaf 0 unconditionally.
        if (!Count(ModelLump, ModelBytes) || !Count(NodeLump, NodeBytes) || !Count(LeafLump, LeafBytes))
            return Fail("missing model, node or leaf", Reason::InvalidFormat);

        return Reason::OK;
    }

    bool Geometry()
    {
        for (size_t i = 0; i < Count(PlaneLump, PlaneBytes); ++i)
        {
            if (!Finite(Record(PlaneLump, i, PlaneBytes), Axes + 1))
                return Bad("non-finite plane", Reason::InvalidFormat);
        }

        const auto vertices = Count(VertexLump, VertexBytes);
        for (size_t i = 0; i < vertices; ++i)
        {
            if (!Finite(Record(VertexLump, i, VertexBytes), Axes))
                return Bad("non-finite vertex", Reason::InvalidFormat);
        }

        const auto edges = Count(EdgeLump, EdgeBytes);
        for (size_t i = 0; i < edges; ++i)
        {
            const auto offset = Record(EdgeLump, i, EdgeBytes);
            if (ReadUInt16(offset) >= vertices || ReadUInt16(offset + sizeof(std::uint16_t)) >= vertices)
                return Bad("edge vertex out of range");
        }

        for (size_t i = 0; i < Count(SurfEdgeLump, SurfEdgeBytes); ++i)
        {
            const std::int64_t edge = ReadInt32(Record(SurfEdgeLump, i, SurfEdgeBytes));
            if (!Index(edge < 0 ? -edge : edge, edges))
                return Bad("surface edge out of range");
        }

        return true;
    }

    //Mod_LoadTextures copies an embedded miptex's palette from after its four mip levels and its color count, and
    //GL_LoadTexture reads a whole 256-color palette there. Names are used as strings, embedded or not.
    bool Textures()
    {
        const auto length = m_Lengths[TextureLump];
        if (!length)
            return true;

        const auto base = m_Offsets[TextureLump];
        if (length < sizeof(std::int32_t))
            return Bad("truncated texture lump");

        const auto count = ReadInt32(base);
        if (count < 0 || !Range(sizeof(std::int32_t), static_cast<size_t>(count), sizeof(std::int32_t), length))
            return Bad("texture offset table outside lump");

        m_Miptexes = static_cast<size_t>(count);

        for (size_t i = 0; i < m_Miptexes; ++i)
        {
            const auto offset = ReadInt32(base + sizeof(std::int32_t) + i * sizeof(std::int32_t));
            if (offset == AbsentOffset)
                continue;

            if (offset < 0 || !Range(static_cast<size_t>(offset), 1, MiptexBytes, length))
                return Bad("miptex outside lump");

            const auto miptex = base + static_cast<size_t>(offset);
            m_Reason          = TextureAssetValidation::Miptex(m_pData + miptex, length - offset, true, m_pResult);
            if (m_Reason != Reason::OK)
                return false;
        }

        return true;
    }

    bool TexInfos()
    {
        for (size_t i = 0; i < Count(TexInfoLump, TexInfoBytes); ++i)
        {
            const auto offset = Record(TexInfoLump, i, TexInfoBytes);
            if (!Finite(offset, TextureAxes * TextureVecs))
                return Bad("non-finite texture vectors", Reason::InvalidFormat);

            //Without a texture lump every face gets the missing texture.
            if (m_Lengths[TextureLump] && !Index(ReadInt32(offset + TexInfoMiptexOffset), m_Miptexes))
                return Bad("texinfo miptex out of range");
        }

        return true;
    }

    //CalcSurfaceExtents, which keeps texturemins and extents as shorts.
    bool FaceExtents(size_t face, size_t texInfo, std::int16_t (&extents)[TextureAxes])
    {
        float mins[TextureAxes] = {ExtentsInitialMin, ExtentsInitialMin};
        float maxs[TextureAxes] = {ExtentsInitialMax, ExtentsInitialMax};

        const auto firstEdge = ReadInt32(face + FaceFirstEdgeOffset);
        const auto edgeCount = ReadInt16(face + FaceEdgeCountOffset);

        for (int i = 0; i < edgeCount; ++i)
        {
            const auto edge   = ReadInt32(Record(SurfEdgeLump, static_cast<size_t>(firstEdge) + i, SurfEdgeBytes));
            const auto record = Record(EdgeLump, static_cast<size_t>(edge >= 0 ? edge : -edge), EdgeBytes);
            const auto vertex = ReadUInt16(record + (edge >= 0 ? 0 : sizeof(std::uint16_t)));
            const auto point  = Record(VertexLump, vertex, VertexBytes);

            for (int axis = 0; axis < TextureAxes; ++axis)
            {
                const auto vec   = texInfo + axis * TextureVecs * sizeof(float);
                const auto value = static_cast<double>(ReadFloat(point)) * ReadFloat(vec) +
                    static_cast<double>(ReadFloat(point + sizeof(float))) * ReadFloat(vec + sizeof(float)) +
                    static_cast<double>(ReadFloat(point + 2 * sizeof(float))) * ReadFloat(vec + 2 * sizeof(float)) +
                    static_cast<double>(ReadFloat(vec + 3 * sizeof(float)));

                if (!(std::fabs(value) <= MaxTextureCoordinate))
                    return Bad("texture coordinate out of range", Reason::InvalidFormat);

                const auto coordinate = static_cast<float>(value);
                if (coordinate < mins[axis])
                    mins[axis] = coordinate;
                if (coordinate > maxs[axis])
                    maxs[axis] = coordinate;
            }
        }

        for (int axis = 0; axis < TextureAxes; ++axis)
        {
            const auto luxelMin = static_cast<int>(std::floor(mins[axis] / LuxelSize));
            const auto luxelMax = static_cast<int>(std::ceil(maxs[axis] / LuxelSize));
            extents[axis]       = static_cast<std::int16_t>((luxelMax - luxelMin) * LuxelSize);
        }

        return true;
    }

    bool Faces()
    {
        const auto planes    = Count(PlaneLump, PlaneBytes);
        const auto texInfos  = Count(TexInfoLump, TexInfoBytes);
        const auto surfEdges = Count(SurfEdgeLump, SurfEdgeBytes);
        const auto lighting  = m_Lengths[LightingLump];

        for (size_t i = 0; i < Count(FaceLump, FaceBytes); ++i)
        {
            const auto face  = Record(FaceLump, i, FaceBytes);
            const auto edges = ReadInt16(face + FaceEdgeCountOffset);

            if (!Index(ReadInt16(face), planes) ||
                !Index(ReadInt16(face + FaceTexInfoOffset), texInfos))
            {
                return Bad("face plane or texinfo out of range");
            }

            if (edges < 1 || !Span(ReadInt32(face + FaceFirstEdgeOffset), edges, surfEdges))
                return Bad("face edges out of range");

            const auto   texInfo = Record(TexInfoLump, static_cast<size_t>(ReadInt16(face + FaceTexInfoOffset)), TexInfoBytes);
            std::int16_t extents[TextureAxes];
            if (!FaceExtents(face, texInfo, extents))
                return false;

            const bool bSpecial = (ReadInt32(texInfo + TexInfoFlagsOffset) & TexSpecial) != 0;
            const bool bLarge   = extents[0] > MaxSurfaceExtent || extents[1] > MaxSurfaceExtent;
            if (!bSpecial && bLarge)
                return Bad("surface extents exceed 512", Reason::InvalidFormat);

            const auto lightOffset = ReadInt32(face + FaceLightOffset);
            if (lightOffset == AbsentOffset)
                continue;

            if (lightOffset < 0)
                return Bad("negative light offset");

            //Without lighting the engine's samples are the null lightdata plus the offset: only 0 is null, which reads as
            //unlit (the unlit cz_worldmap gives every face 0).
            if (!lighting)
            {
                if (lightOffset)
                    return Bad("light offset without lighting");

                continue;
            }

            //Large special faces are drawn tiled and never sampled. Any other face may be, by RecursiveLightPoint
            //and R_BuildLightMap, even if it is special: a missing texture clears its flags.
            if (bLarge)
                continue;

            const int luxelsS = (extents[0] >> 4) + 1;
            const int luxelsT = (extents[1] >> 4) + 1;
            if (luxelsS <= 0 || luxelsT <= 0)
                return Bad("negative surface extents", Reason::InvalidFormat);

            int styles = 0;
            while (styles < LightStyles && Read<std::uint8_t>(face + FaceStylesOffset + styles) != NoLightStyle)
                ++styles;

            const auto samples = static_cast<size_t>(luxelsS) * static_cast<size_t>(luxelsT) * static_cast<size_t>(styles);
            if (!Range(static_cast<size_t>(lightOffset), samples, LightSample, lighting))
                return Bad("light samples outside lighting");
        }

        return true;
    }

    //The values are unsigned shorts in the file.
    bool MarkSurfaces()
    {
        const auto faces = Count(FaceLump, FaceBytes);
        for (size_t i = 0; i < Count(MarkSurfaceLump, MarkSurfaceBytes); ++i)
        {
            if (ReadUInt16(Record(MarkSurfaceLump, i, MarkSurfaceBytes)) >= faces)
                return Bad("mark surface out of range");
        }

        return true;
    }

    bool Models()
    {
        const auto nodes     = Count(NodeLump, NodeBytes);
        const auto clipNodes = Count(ClipNodeLump, ClipNodeBytes);
        const auto leaves    = Count(LeafLump, LeafBytes);
        const auto faces     = Count(FaceLump, FaceBytes);

        for (size_t i = 0; i < Count(ModelLump, ModelBytes); ++i)
        {
            const auto model = Record(ModelLump, i, ModelBytes);
            //Mins, maxs and origin.
            if (!Finite(model, Axes * 3))
                return Bad("non-finite model bounds", Reason::InvalidFormat);

            for (int axis = 0; axis < Axes; ++axis)
            {
                if (ReadFloat(model + axis * sizeof(float)) > ReadFloat(model + ModelMaxsOffset + axis * sizeof(float)))
                    return Bad("inverted model bounds", Reason::InvalidFormat);
            }

            if (!Index(ReadInt32(model + ModelHeadNodeOffset), nodes))
                return Bad("model head node out of range");

            //A negative clipping head is the contents of an empty hull. Compilers also give a model without clipping
            //brushes the next model's first clipnode, which is the clipnode count after the last one with clipnodes:
            //stock maps (c2a2a, de_inferno_cz) have such models, which callers must not trace.
            for (int hull = 1; hull < Hulls; ++hull)
            {
                const std::int64_t head = ReadInt32(model + ModelHeadNodeOffset + hull * sizeof(std::int32_t));
                if (head >= 0 && static_cast<std::uint64_t>(head) > clipNodes)
                    return Bad("model clipping head out of range");
            }

            //A model's visible leaves exclude leaf 0.
            if (!Index(ReadInt32(model + ModelVisLeafsOffset), leaves))
                return Bad("model visible leaves out of range");

            if (!Span(ReadInt32(model + ModelFirstFaceOffset), ReadInt32(model + ModelFaceCountOffset), faces))
                return Bad("model faces out of range");
        }

        return true;
    }

    //Children are nodes when not negative, otherwise leaf -1 - child.
    bool Nodes()
    {
        const auto planes = Count(PlaneLump, PlaneBytes);
        const auto nodes  = Count(NodeLump, NodeBytes);
        const auto leaves = Count(LeafLump, LeafBytes);
        const auto faces  = Count(FaceLump, FaceBytes);

        for (size_t i = 0; i < nodes; ++i)
        {
            const auto node = Record(NodeLump, i, NodeBytes);
            if (!Index(ReadInt32(node), planes))
                return Bad("node plane out of range");

            for (int side = 0; side < 2; ++side)
            {
                const std::int64_t child = ReadInt16(node + NodeChildrenOffset + side * sizeof(std::int16_t));
                if (child >= 0 ? !Index(child, nodes) : !Index(-1 - child, leaves))
                    return Bad("node child out of range");
            }

            if (!Span(ReadUInt16(node + NodeFirstFaceOffset), ReadUInt16(node + NodeFaceCountOffset), faces))
                return Bad("node faces out of range");
        }

        return true;
    }

    //CM_DecompressPVS decodes at least once and stops at rowBytes, but does not clip the run of zeros that crosses it,
    //which may end anywhere up to runBytes.
    bool VisibilityRow(size_t offset, size_t rowBytes, size_t runBytes)
    {
        const auto* pVisibility = m_pData + m_Offsets[VisibilityLump];
        const auto  length      = m_Lengths[VisibilityLump];
        size_t      written     = 0;

        do
        {
            if (offset >= length)
                return Bad("visibility row outside lump");

            if (pVisibility[offset])
            {
                ++written;
                ++offset;
            }
            else
            {
                if (offset + 1 >= length)
                    return Bad("visibility row outside lump");

                written += pVisibility[offset + 1];
                offset += 2;
            }

            if (written > runBytes)
                return Bad("visibility run past the compiled row");
        } while (written < rowBytes);

        return true;
    }

    bool Leaves()
    {
        const auto marks = Count(MarkSurfaceLump, MarkSurfaceBytes);
        //Mod_SetupSubmodel sets the world's leaf count to model 0's visible leaves, which sizes every row the engine
        //decodes. Compilers write longer rows: hlvis pads them to 64 leaves, and the stock Opposing Force and
        //Counter-Strike maps hold a bit for every leaf, so a run of zeros may end at the longer of the two. Within the
        //caller's leaf capacity that stays inside its MAX_MAP_LEAFS / 8 buffers; callers decoding into rows sized by
        //the visible leaves must clip the run.
        const auto visibleLeaves = static_cast<size_t>(ReadInt32(Record(ModelLump, 0, ModelBytes) + ModelVisLeafsOffset));
        const auto rowBytes      = (visibleLeaves + 7) >> 3;
        const auto paddedBytes   = ((visibleLeaves + 63) & ~size_t(63)) >> 3;
        const auto leafBytes     = (Count(LeafLump, LeafBytes) + 7) >> 3;
        const auto runBytes      = paddedBytes > leafBytes ? paddedBytes : leafBytes;

        for (size_t i = 0; i < Count(LeafLump, LeafBytes); ++i)
        {
            const auto leaf = Record(LeafLump, i, LeafBytes);

            //Mod_SetParent and Mod_MakeHull0 tell leaves from nodes by negative contents.
            if (ReadInt32(leaf) >= 0)
                return Bad("leaf contents not negative", Reason::InvalidFormat);

            if (!Span(ReadUInt16(leaf + LeafFirstMarkOffset), ReadUInt16(leaf + LeafMarkCountOffset), marks))
                return Bad("leaf mark surfaces out of range");

            const auto visibility = ReadInt32(leaf + LeafVisOffset);
            if (visibility == AbsentOffset)
                continue;

            if (visibility < 0)
                return Bad("negative visibility offset");

            //Without visibility data the row is the null visdata plus the offset, and only 0 is null, which sees
            //everything (brush model files such as sphere256.bsp give their leaves 0).
            if (!m_Lengths[VisibilityLump])
            {
                if (visibility)
                    return Bad("visibility offset without visibility");

                continue;
            }

            if (!VisibilityRow(static_cast<size_t>(visibility), rowBytes, runBytes))
                return false;
        }

        return true;
    }

    //Children are clipnodes when not negative, otherwise contents.
    bool ClipNodes()
    {
        const auto planes    = Count(PlaneLump, PlaneBytes);
        const auto clipNodes = Count(ClipNodeLump, ClipNodeBytes);

        for (size_t i = 0; i < clipNodes; ++i)
        {
            const auto node = Record(ClipNodeLump, i, ClipNodeBytes);
            if (!Index(ReadInt32(node), planes))
                return Bad("clipnode plane out of range");

            for (int side = 0; side < 2; ++side)
            {
                const auto child = ReadInt16(node + ClipChildrenOffset + side * sizeof(std::int16_t));
                if (child >= 0 && !Index(child, clipNodes))
                    return Bad("clipnode child out of range");
            }
        }

        return true;
    }

    //Every node of a graph whose non-negative children are nodes must reach a leaf within MaxTreeDepth levels, which
    //also rules out cycles. Shared subtrees are allowed: compilers merge identical clipnodes.
    bool Acyclic(size_t lump, size_t recordBytes, size_t childrenOffset)
    {
        enum : std::uint8_t
        {
            Unvisited,
            Open,
            Closed
        };

        struct Frame
        {
            size_t node;
            int    side;
        };

        const auto                count = Count(lump, recordBytes);
        std::vector<std::uint8_t> state(count, Unvisited);
        std::vector<int>          height(count, 0);
        std::vector<Frame>        stack;

        for (size_t root = 0; root < count; ++root)
        {
            if (state[root] != Unvisited)
                continue;

            state[root] = Open;
            stack.push_back({root, 0});

            while (!stack.empty())
            {
                const auto node = stack.back().node;

                if (stack.back().side < 2)
                {
                    const auto side  = stack.back().side++;
                    const auto child = ReadInt16(Record(lump, node, recordBytes) + childrenOffset + side * sizeof(std::int16_t));
                    if (child < 0)
                        continue;

                    if (state[child] == Open)
                        return Bad("cyclic node graph", Reason::InvalidFormat);

                    if (state[child] == Unvisited)
                    {
                        state[child] = Open;
                        stack.push_back({static_cast<size_t>(child), 0});
                    }

                    continue;
                }

                int deepest = 0;
                for (int side = 0; side < 2; ++side)
                {
                    const auto child = ReadInt16(Record(lump, node, recordBytes) + childrenOffset + side * sizeof(std::int16_t));
                    if (child >= 0 && height[child] > deepest)
                        deepest = height[child];
                }

                height[node] = deepest + 1;
                if (height[node] > MaxTreeDepth)
                    return Bad("node graph too deep", Reason::InvalidFormat);

                state[node] = Closed;
                stack.pop_back();
            }
        }

        return true;
    }

    bool Trees()
    {
        if (!Acyclic(NodeLump, NodeBytes, NodeChildrenOffset) ||
            !Acyclic(ClipNodeLump, ClipNodeBytes, ClipChildrenOffset))
        {
            return false;
        }

        //Mod_SetParent recurses the drawing tree from node 0 into every child: no node may be reached twice.
        const auto          nodes = Count(NodeLump, NodeBytes);
        std::vector<bool>   visited(nodes);
        std::vector<size_t> pending{0};

        while (!pending.empty())
        {
            const auto node = pending.back();
            pending.pop_back();

            if (visited[node])
                return Bad("shared node in the drawing tree", Reason::InvalidFormat);

            visited[node] = true;

            for (int side = 0; side < 2; ++side)
            {
                const auto child = ReadInt16(Record(NodeLump, node, NodeBytes) + NodeChildrenOffset + side * sizeof(std::int16_t));
                if (child >= 0)
                    pending.push_back(static_cast<size_t>(child));
            }
        }

        return true;
    }

    const std::uint8_t*                 m_pData;
    size_t                              m_Size;
    Layout                              m_Layout;
    UtilAssetsIntegrityCheckResult_BSP* m_pResult;
    Reason                              m_Reason{Reason::OK};
    size_t                              m_Offsets[LumpCount]{};
    size_t                              m_Lengths[LumpCount]{};
    size_t                              m_Miptexes{};
};
