#pragma once

#include <IUtilAssetsIntegrity.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <climits>
#include <cmath>

//Only file-format types are needed from the SDK; no MetaHook runtime or engine globals.
typedef unsigned char byte;
typedef float         vec3_t[3];
typedef std::int32_t  int32;
#include <studio.h>

static_assert(sizeof(studiohdr_t) == 244 && sizeof(studioseqhdr_t) == 76, "Studio v10 file headers");
static_assert(sizeof(mstudioseqgroup_t) == 104 && sizeof(mstudioseqdesc_t) == 176, "Studio v10 sequence records");

class CStudioModelValidation
{
public:
    using Reason = UtilAssetsIntegrityCheckReason;

    CStudioModelValidation(const void* pData, size_t size, UtilAssetsIntegrityCheckResult_StudioModel* pResult) :
        m_pData(static_cast<const byte*>(pData)), m_Size(size), m_Limit(size), m_pResult(pResult)
    {
        if (m_pResult) m_pResult->ReasonStr[0] = 0;
    }

    Reason Check()
    {
        if (!m_pData || m_Size < sizeof(std::uint32_t)) return Fail("truncated model signature", Reason::SizeTooSmall);
        if (m_Size > INT_MAX) return Fail("model exceeds signed file offsets", Reason::SizeTooLarge);
        if (!std::memcmp(m_pData, "IDSQ", 4)) return CheckGroupHeader();
        if (std::memcmp(m_pData, "IDST", 4)) return Fail("expected IDST or IDSQ", Reason::BogusHeader);
        if (m_Size < sizeof(studiohdr_t)) return Fail("truncated studio header", Reason::SizeTooSmall);
        m_Header = Read<studiohdr_t>(0);
        if (m_Header.version != 10) return Fail("expected studio version 10", Reason::VersionMismatch);
        if (m_Header.length < sizeof(studiohdr_t) || static_cast<size_t>(m_Header.length) != m_Size) return Fail("invalid file length");
        if (!Name(m_Header.name)) return Fail("unterminated model name");
        if (m_Header.texturedataindex < 0) return Fail("negative texture data offset");
        if (m_Header.textureindex)
        {
            if (m_Header.texturedataindex < sizeof(studiohdr_t) || static_cast<size_t>(m_Header.texturedataindex) > m_Size) return Fail("invalid kept structure length");
            m_Limit = static_cast<size_t>(m_Header.texturedataindex);
        }
        if (!Textures() || !Bones() || !Bodies() || !Sequences() || !HitboxesAndAttachments() || !Transitions()) return Reason::OutOfBound;
        return Reason::OK;
    }

    //The engine may supply its cached, texture-patched main header: only validated structural tables are read here.
    Reason CheckGroup(const void* pModel, size_t modelBytes, int group)
    {
        auto code = CheckGroupHeader();
        if (code != Reason::OK) return code;
        if (!pModel || modelBytes < sizeof(studiohdr_t) || modelBytes > INT_MAX) return Fail("missing main model for sequence group");
        CStudioModelValidation model(pModel, modelBytes, m_pResult);
        model.m_Header = model.Read<studiohdr_t>(0);
        const auto& h  = model.m_Header;
        if (std::memcmp(pModel, "IDST", 4) || h.version != 10 || h.numbones < 1 || h.numbones > MAXSTUDIOBONES ||
            group <= 0 || group >= h.numseqgroups || h.numseqgroups > SequenceGroups ||
            !model.Table(h.seqindex, h.numseq, sizeof(mstudioseqdesc_t), SequencesLimit)) return Fail("invalid main model sequence context");
        for (int i = 0; i < h.numseq; ++i)
        {
            const auto seq = model.Record<mstudioseqdesc_t>(h.seqindex, i);
            if (seq.seqgroup == group && !Animation(seq, h.numbones)) return Reason::OutOfBound;
        }
        return Reason::OK;
    }

private:
    static constexpr int TextureLimit     = MAXSTUDIOSKINS;
    static constexpr int SequenceGroups   = 16;
    static constexpr int SequencesLimit   = 2048;
    static constexpr int SubmodelsLimit   = 128;
    static constexpr int HitboxesLimit    = 128;
    static constexpr int AttachmentsLimit = 4;
    static constexpr int PaletteBytes     = 768;
    static constexpr int Channels         = 6;
    //Counter-Strike player sequences use a 3x3 blend grid.
    static constexpr int MaxBlends = 9;
    static constexpr int MaxFrames = 65536;
    //Bound repeated/overlapping animation and triangle streams too, not just their file extents.
    static constexpr size_t WorkLimit = 16 * 1024 * 1024;

    Reason Fail(const char* text, Reason reason = Reason::OutOfBound)
    {
        if (m_pResult) std::snprintf(m_pResult->ReasonStr, sizeof(m_pResult->ReasonStr), "StudioModel: %s.", text);
        return reason;
    }
    bool Bad(const char* text)
    {
        Fail(text);
        return false;
    }
    bool Work() { return m_Work++ < WorkLimit || Bad("validation work limit exceeded"); }
    bool Range(size_t offset, size_t count, size_t stride, size_t limit) const
    {
        return offset <= limit && stride && count <= (limit - offset) / stride;
    }
    bool Table(int offset, int count, size_t stride, int maximum = INT_MAX)
    {
        if (offset < 0 || count < 0 || count > maximum || (count && offset < sizeof(studiohdr_t)) ||
            !Range(static_cast<size_t>(offset), static_cast<size_t>(count), stride, m_Limit)) return Bad("invalid table extent or count");
        return true;
    }
    template <class T> T Read(size_t offset) const
    {
        T value;
        std::memcpy(&value, m_pData + offset, sizeof(value));
        return value;
    }
    template <class T> T            Record(int offset, int index) const { return Read<T>(static_cast<size_t>(offset) + static_cast<size_t>(index) * sizeof(T)); }
    template <size_t N> static bool Name(const char (&name)[N]) { return std::memchr(name, 0, N) != nullptr; }
    static bool                     Index(int value, int count) { return value >= 0 && value < count; }

    Reason CheckGroupHeader()
    {
        if (!m_pData || m_Size < sizeof(studioseqhdr_t)) return Fail("truncated sequence group header", Reason::SizeTooSmall);
        if (m_Size > INT_MAX) return Fail("sequence group too large", Reason::SizeTooLarge);
        if (std::memcmp(m_pData, "IDSQ", 4)) return Fail("expected IDSQ", Reason::BogusHeader);
        const auto h = Read<studioseqhdr_t>(0);
        if (h.version != 10) return Fail("expected sequence group version 10", Reason::VersionMismatch);
        if (h.length < sizeof(h) || static_cast<size_t>(h.length) != m_Size || !Name(h.name)) return Fail("invalid sequence group header");
        return Reason::OK;
    }

    bool Textures()
    {
        const auto& h = m_Header;
        if (!Table(h.textureindex, h.numtextures, sizeof(mstudiotexture_t), TextureLimit)) return false;
        for (int i = 0; i < h.numtextures; ++i)
        {
            const auto t = Record<mstudiotexture_t>(h.textureindex, i);
            if (!Name(t.name) || t.width <= 0 || t.height <= 0 || t.index < h.texturedataindex) return Bad("invalid texture dimensions, name or offset");
            const auto pixels = std::uint64_t(t.width) * std::uint64_t(t.height);
            if (t.index < 0 || static_cast<size_t>(t.index) > m_Size || pixels + PaletteBytes > m_Size - static_cast<size_t>(t.index)) return Bad("texture pixels or palette outside file");
        }
        if (h.numskinref < 0 || h.numskinfamilies < 0 || h.skinindex < 0) return Bad("negative skin table field");
        const auto entries = std::uint64_t(h.numskinref) * std::uint64_t(h.numskinfamilies);
        if (entries > INT_MAX || !Table(h.skinindex, static_cast<int>(entries), sizeof(short))) return Bad("skin table outside kept data");
        for (int i = 0; i < static_cast<int>(entries); ++i)
            if (!Work() || !Index(Record<short>(h.skinindex, i), h.numtextures)) return Bad("skin texture index out of range");
        return true;
    }

    bool Bones()
    {
        const auto& h = m_Header;
        if (!Table(h.boneindex, h.numbones, sizeof(mstudiobone_t), MAXSTUDIOBONES) ||
            !Table(h.bonecontrollerindex, h.numbonecontrollers, sizeof(mstudiobonecontroller_t), MAXSTUDIOCONTROLLERS)) return false;
        for (int i = 0; i < h.numbonecontrollers; ++i)
        {
            const auto c = Record<mstudiobonecontroller_t>(h.bonecontrollerindex, i);
            if ((c.bone != -1 && !Index(c.bone, h.numbones)) || c.index < 0 || c.index > 4) return Bad("controller bone or input index out of range");
        }
        for (int i = 0; i < h.numbones; ++i)
        {
            const auto b = Record<mstudiobone_t>(h.boneindex, i);
            if (!Name(b.name) || (b.parent != -1 && !Index(b.parent, h.numbones))) return Bad("invalid bone name or parent");
            for (const auto controller : b.bonecontroller)
                if (controller != -1 && !Index(controller, h.numbonecontrollers)) return Bad("bone controller slot out of range");
            int parent = i;
            for (int depth = 0; parent != -1; ++depth)
            {
                if (depth >= h.numbones || !Index(parent, h.numbones)) return Bad("cyclic or invalid bone hierarchy");
                parent = Record<mstudiobone_t>(h.boneindex, parent).parent;
            }
        }
        return true;
    }

    bool Mesh(const mstudiomodel_t& model, const mstudiomesh_t& mesh)
    {
        if (mesh.triindex < sizeof(studiohdr_t) || mesh.skinref < 0 ||
            (m_Header.numtextures && !Index(mesh.skinref, m_Header.numskinref))) return Bad("invalid mesh texture reference or commands offset");
        size_t offset = static_cast<size_t>(mesh.triindex);
        for (;;)
        {
            if (!Work() || !Range(offset, 1, sizeof(short), m_Limit)) return Bad("unterminated triangle commands");
            int count = Read<short>(offset);
            offset += sizeof(short);
            if (!count) return true;
            if (count < 0) count = -count;
            constexpr size_t PointBytes = sizeof(short) * 4;
            if (count < 3 || !Range(offset, static_cast<size_t>(count), PointBytes, m_Limit)) return Bad("truncated triangle points");
            for (int i = 0; i < count; ++i, offset += PointBytes)
                if (!Work() || !Index(Read<short>(offset), model.numverts) || !Index(Read<short>(offset + sizeof(short)), model.numnorms)) return Bad("triangle vertex or normal out of range");
        }
    }

    bool Bodies()
    {
        const auto& h = m_Header;
        if (!Table(h.bodypartindex, h.numbodyparts, sizeof(mstudiobodyparts_t), MAXSTUDIOBODYPARTS)) return false;
        for (int i = 0; i < h.numbodyparts; ++i)
        {
            const auto body = Record<mstudiobodyparts_t>(h.bodypartindex, i);
            if (!Name(body.name) || body.base <= 0 || body.nummodels <= 0) return Bad("bodypart has invalid name or divisor");
            if (!Table(body.modelindex, body.nummodels, sizeof(mstudiomodel_t), SubmodelsLimit)) return false;
            for (int j = 0; j < body.nummodels; ++j)
            {
                const auto model = Record<mstudiomodel_t>(body.modelindex, j);
                if (!Name(model.name) || !Table(model.vertindex, model.numverts, sizeof(vec3_t)) ||
                    !Table(model.normindex, model.numnorms, sizeof(vec3_t)) ||
                    !Table(model.vertinfoindex, model.numverts, sizeof(byte)) ||
                    !Table(model.norminfoindex, model.numnorms, sizeof(byte)) ||
                    !Table(model.meshindex, model.nummesh, sizeof(mstudiomesh_t), MAXSTUDIOMESHES)) return Bad("invalid submodel tables");
                for (int v = 0; v < model.numverts; ++v)
                    if (!Work() || !Index(Record<byte>(model.vertinfoindex, v), h.numbones)) return Bad("vertex bone out of range");
                for (int n = 0; n < model.numnorms; ++n)
                    if (!Work() || !Index(Record<byte>(model.norminfoindex, n), h.numbones)) return Bad("normal bone out of range");
                for (int k = 0; k < model.nummesh; ++k)
                    if (!Mesh(model, Record<mstudiomesh_t>(model.meshindex, k))) return false;
            }
        }
        return true;
    }

    //Check all six channels and blends. Span headers advance monotonically, and every value read by interpolation
    //is bounded, including the following span's first sample at the last frame of a run.
    bool Animation(const mstudioseqdesc_t& seq, int bones)
    {
        if (seq.numframes <= 0 || seq.numframes > MaxFrames || seq.numblends <= 0 || seq.numblends > MaxBlends || seq.animindex < 0) return Bad("invalid animation dimensions");
        const size_t records = static_cast<size_t>(bones) * static_cast<size_t>(seq.numblends);
        if (!Range(static_cast<size_t>(seq.animindex), records, sizeof(mstudioanim_t), m_Limit)) return Bad("animation records outside file");
        for (size_t i = 0; i < records; ++i)
        {
            const size_t base = static_cast<size_t>(seq.animindex) + i * sizeof(mstudioanim_t);
            const auto   anim = Read<mstudioanim_t>(base);
            for (int channel = 0; channel < Channels; ++channel)
            {
                if (!anim.offset[channel]) continue;
                size_t offset  = base + anim.offset[channel];
                int    covered = 0;
                while (covered < seq.numframes)
                {
                    if (!Work() || !Range(offset, 1, sizeof(mstudioanimvalue_t), m_Limit)) return Bad("animation span header outside file");
                    const auto span = Read<mstudioanimvalue_t>(offset);
                    if (!span.num.total || !span.num.valid || span.num.valid > span.num.total) return Bad("invalid animation span counts");
                    if (!Range(offset, size_t(span.num.valid) + 1, sizeof(mstudioanimvalue_t), m_Limit)) return Bad("animation samples outside file");
                    if (covered + span.num.total <= seq.numframes && !Range(offset, size_t(span.num.valid) + 3, sizeof(mstudioanimvalue_t), m_Limit)) return Bad("animation interpolation lookahead outside file");
                    covered += span.num.total;
                    offset += (size_t(span.num.valid) + 1) * sizeof(mstudioanimvalue_t);
                }
            }
        }
        return true;
    }

    bool Sequences()
    {
        const auto& h = m_Header;
        if (!Table(h.seqindex, h.numseq, sizeof(mstudioseqdesc_t), SequencesLimit) ||
            !Table(h.seqgroupindex, h.numseqgroups, sizeof(mstudioseqgroup_t), SequenceGroups)) return false;
        if ((h.numbones || h.numbodyparts) && !h.numseq) return Bad("geometry model has no sequence");
        for (int i = 0; i < h.numseqgroups; ++i)
        {
            const auto group = Record<mstudioseqgroup_t>(h.seqgroupindex, i);
            if (!Name(group.name) || !Name(group.label)) return Bad("unterminated sequence group name");
        }
        for (int i = 0; i < h.numseq; ++i)
        {
            const auto seq = Record<mstudioseqdesc_t>(h.seqindex, i);
            if (!Name(seq.label) || !Index(seq.seqgroup, h.numseqgroups) || !Index(seq.motionbone, h.numbones) ||
                seq.numframes <= 0 || seq.numframes > MaxFrames || seq.numblends <= 0 || seq.numblends > MaxBlends || seq.animindex < 0 || !std::isfinite(seq.fps)) return Bad("invalid sequence fields");
            if (!Table(seq.eventindex, seq.numevents, sizeof(mstudioevent_t), MAXSTUDIOEVENTS) ||
                !Table(seq.pivotindex, seq.numpivots, sizeof(mstudiopivot_t), MAXSTUDIOPIVOTS)) return false;
            for (int j = 0; j < seq.numevents; ++j)
                if (!Name(Record<mstudioevent_t>(seq.eventindex, j).options)) return Bad("unterminated event options");
            if (seq.seqgroup == 0 && !Animation(seq, h.numbones)) return false;
            if (seq.entrynode < 0 || seq.exitnode < 0 || seq.entrynode > h.numtransitions || seq.exitnode > h.numtransitions) return Bad("transition node out of range");
        }
        return true;
    }

    bool HitboxesAndAttachments()
    {
        const auto& h = m_Header;
        if (!Table(h.hitboxindex, h.numhitboxes, sizeof(mstudiobbox_t), HitboxesLimit) ||
            !Table(h.attachmentindex, h.numattachments, sizeof(mstudioattachment_t), AttachmentsLimit)) return false;
        for (int i = 0; i < h.numhitboxes; ++i)
            if (!Index(Record<mstudiobbox_t>(h.hitboxindex, i).bone, h.numbones)) return Bad("hitbox bone out of range");
        for (int i = 0; i < h.numattachments; ++i)
        {
            const auto attachment = Record<mstudioattachment_t>(h.attachmentindex, i);
            if (!Name(attachment.name) || !Index(attachment.bone, h.numbones)) return Bad("invalid attachment name or bone");
        }
        return true;
    }

    bool Transitions()
    {
        const auto& h = m_Header;
        if (h.numtransitions < 0 || h.numtransitions > UCHAR_MAX) return Bad("invalid transition count");
        const int count = h.numtransitions * h.numtransitions;
        if (!Table(h.transitionindex, count, sizeof(byte))) return false;
        for (int i = 0; i < count; ++i)
            if (Record<byte>(h.transitionindex, i) > h.numtransitions) return Bad("transition target out of range");
        return true;
    }

    const byte*                                 m_pData;
    size_t                                      m_Size;
    size_t                                      m_Limit;
    size_t                                      m_Work{};
    studiohdr_t                                 m_Header{};
    UtilAssetsIntegrityCheckResult_StudioModel* m_pResult;
};
