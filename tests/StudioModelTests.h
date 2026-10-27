#pragma once

#include <climits>
#include <limits>

//A complete, constant-animation model. Offsets are laid out independently of the validator.
struct StudioFixture
{
    studiohdr_t               header = MakeStudioHeader();
    std::vector<std::uint8_t> bytes  = std::vector<std::uint8_t>(sizeof(studiohdr_t));

    template <class T> int Append(const T& value)
    {
        const auto offset = static_cast<int>(bytes.size());
        bytes.resize(bytes.size() + sizeof(value));
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
        return offset;
    }

    template <class T> T Read(int offset) const
    {
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    template <class T> void Write(int offset, const T& value)
    {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    void Finish()
    {
        header.length = static_cast<int>(bytes.size());
        Write(0, header);
    }

    template <class T> int AppendStructure(const T& value)
    {
        const int offset = header.texturedataindex;
        bytes.insert(bytes.begin() + offset, sizeof(T), 0);
        Write(offset, value);
        header.texturedataindex += sizeof(T);
        auto texture = Read<mstudiotexture_t>(header.textureindex);
        texture.index += sizeof(T);
        Write(header.textureindex, texture);
        Finish();
        return offset;
    }

    StudioFixture()
    {
        mstudiobone_t bone{};
        bone.parent = -1;
        for (auto& controller : bone.bonecontroller) controller = -1;
        header.numbones      = 1;
        header.boneindex     = Append(bone);
        header.numseqgroups  = 1;
        header.seqgroupindex = Append(mstudioseqgroup_t{});
        mstudioseqdesc_t sequence{};
        sequence.numframes = 2;
        sequence.numblends = 1;
        header.numseq      = 1;
        header.seqindex    = Append(sequence);
        sequence.animindex = Append(mstudioanim_t{});
        Write(header.seqindex, sequence);

        mstudiobodyparts_t body{};
        body.base            = 1;
        body.nummodels       = 1;
        header.numbodyparts  = 1;
        header.bodypartindex = Append(body);
        mstudiomodel_t model{};
        body.modelindex = Append(model);
        Write(header.bodypartindex, body);
        model.numverts      = 3;
        model.numnorms      = 1;
        model.vertindex     = Append(std::array<float, 9>{});
        model.normindex     = Append(std::array<float, 3>{});
        model.vertinfoindex = Append(std::array<std::uint8_t, 3>{});
        model.norminfoindex = Append(std::uint8_t{});
        model.nummesh       = 1;
        mstudiomesh_t mesh{};
        mesh.numtris    = 1;
        mesh.numnorms   = 1;
        model.meshindex = Append(mesh);
        mesh.triindex   = Append(std::array<std::int16_t, 14>{3, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0});
        Write(model.meshindex, mesh);
        Write(body.modelindex, model);

        mstudiotexture_t texture{};
        texture.width = texture.height = 4;
        header.numtextures             = 1;
        header.textureindex            = Append(texture);
        header.numskinref = header.numskinfamilies = 1;
        header.skinindex                           = Append(std::int16_t{});
        header.texturedataindex = texture.index = Append(std::array<std::uint8_t, 16 + 768>{});
        Write(header.textureindex, texture);
        Finish();
    }
};

void CheckStudioRegressions(IUtilAssetsIntegrity& api)
{
    using Reason = UtilAssetsIntegrityCheckReason;
    auto valid   = StudioFixture{};
    Expect(Reason::OK, api.CheckStudioModel(valid.bytes.data(), valid.bytes.size(), nullptr), "complete model");
    {
        auto file     = valid;
        auto seq      = file.Read<mstudioseqdesc_t>(file.header.seqindex);
        seq.numblends = 9;
        seq.animindex = file.AppendStructure(std::array<mstudioanim_t, 9>{});
        file.Write(file.header.seqindex, seq);
        file.Finish();
        Expect(Reason::OK, api.CheckStudioModel(file.bytes.data(), file.bytes.size(), nullptr), "Counter-Strike nine-way blends");
    }
    {
        //Counter-Strike's v_knife.mdl names its source path in all 64 bytes, without a terminator.
        auto file = valid;
        std::memset(file.header.name, 'a', sizeof(file.header.name));
        file.Finish();
        Expect(Reason::OK, api.CheckStudioModel(file.bytes.data(), file.bytes.size(), nullptr), "header name filling its field");
    }
    for (const bool bTextureOnly : {false, true})
    {
        auto                                             file = valid;
        std::array<mstudiotexture_t, MAXSTUDIOSKINS + 1> textures{};
        for (auto& texture : textures)
        {
            texture.width = texture.height = 4;
            texture.index                  = file.header.texturedataindex + sizeof(textures);
        }
        file.header.textureindex = file.AppendStructure(textures);
        if (bTextureOnly)
        {
            file.header.numbones = file.header.boneindex = 0;
            file.header.numseq = file.header.seqindex = 0;
            file.header.numseqgroups = file.header.seqgroupindex = 0;
            file.header.numbodyparts = file.header.bodypartindex = 0;
        }
        constexpr int s_RemapCacheCapacity = 32;
        for (const int count : {s_RemapCacheCapacity, s_RemapCacheCapacity + 1, s_RemapCacheCapacity + 2, MAXSTUDIOSKINS - 1, MAXSTUDIOSKINS, MAXSTUDIOSKINS + 1})
        {
            file.header.numtextures = count;
            file.Write(file.header.skinindex, static_cast<std::int16_t>(count - 1));
            file.Finish();
            const auto expected = count <= MAXSTUDIOSKINS ? Reason::OK : Reason::OutOfBound;
            Expect(expected, api.CheckStudioModel(file.bytes.data(), file.bytes.size(), nullptr), "MDL and T.mdl texture limits are independent of remap cache capacity");
            file.Write(file.header.skinindex, static_cast<std::int16_t>(count));
            Check(api.CheckStudioModel(file.bytes.data(), file.bytes.size(), nullptr) != Reason::OK, "skin reference at texture count rejected");
            file.Write(file.header.skinindex, std::int16_t{-1});
            Check(api.CheckStudioModel(file.bytes.data(), file.bytes.size(), nullptr) != Reason::OK, "negative skin reference rejected");
        }
    }
    const auto reject = [&](auto patch, const char* name) {
        auto file = valid;
        patch(file);
        file.Finish();
        UtilAssetsIntegrityCheckResult_StudioModel result;
        Check(api.CheckStudioModel(file.bytes.data(), file.bytes.size(), &result) != Reason::OK, name);
        Check(result.ReasonStr[0] != 0, "invalid model has diagnostic");
    };
    reject([](auto& f) { f.header.numhitboxes = 1; f.header.hitboxindex = static_cast<int>(f.bytes.size()); }, "hitbox table at EOF");
    reject([](auto& f) { f.header.numbonecontrollers = 2; f.header.bonecontrollerindex = f.header.texturedataindex - sizeof(mstudiobonecontroller_t); }, "controller table uses controller count");
    reject([](auto& f) { f.header.numskinref = INT_MAX; f.header.numskinfamilies = INT_MAX; }, "skin count multiplication overflow");
    reject([](auto& f) { f.header.numskinref = -1; f.header.numskinfamilies = 0; }, "negative count with empty family table");
    reject([](auto& f) { auto t = f.template Read<mstudiotexture_t>(f.header.textureindex); t.width = t.height = INT_MAX; f.Write(f.header.textureindex, t); }, "texture multiplication overflow");
    reject([](auto& f) { f.bytes.pop_back(); }, "palette truncated by one byte");
    reject([](auto& f) { auto b = f.template Read<mstudiobodyparts_t>(f.header.bodypartindex); b.base = 0; f.Write(f.header.bodypartindex, b); }, "zero bodypart base");
    reject([](auto& f) { auto b = f.template Read<mstudiobodyparts_t>(f.header.bodypartindex); b.nummodels = 0; f.Write(f.header.bodypartindex, b); }, "zero bodypart models");
    reject([](auto& f) { auto b = f.template Read<mstudiobone_t>(f.header.boneindex); b.parent = 0; f.Write(f.header.boneindex, b); }, "cyclic bone hierarchy");
    reject([](auto& f) { auto b = f.template Read<mstudiobone_t>(f.header.boneindex); b.bonecontroller[0] = 0; f.Write(f.header.boneindex, b); }, "bone controller slot without controller");
    reject([](auto& f) { auto s = f.template Read<mstudioseqdesc_t>(f.header.seqindex); s.seqgroup = 16; f.Write(f.header.seqindex, s); }, "sequence group beyond cache");
    reject([](auto& f) { auto s = f.template Read<mstudioseqdesc_t>(f.header.seqindex); s.motionbone = f.header.numbones; f.Write(f.header.seqindex, s); }, "motion bone at count");
    reject([](auto& f) { f.header.numattachments = 1; f.header.attachmentindex = static_cast<int>(f.bytes.size()); }, "attachment table at EOF");
    reject([](auto& f) { f.header.numtransitions = INT_MAX; }, "transition count overflow");
    reject([](auto& f) {
        auto b    = f.template Read<mstudiobodyparts_t>(f.header.bodypartindex);
        auto m    = f.template Read<mstudiomodel_t>(b.modelindex);
        auto mesh = f.template Read<mstudiomesh_t>(m.meshindex);
        f.Write(mesh.triindex + sizeof(std::int16_t), static_cast<std::int16_t>(m.numverts));
    },
           "triangle vertex at count");
    reject([](auto& f) {
        auto s      = f.template Read<mstudioseqdesc_t>(f.header.seqindex);
        auto a      = f.template Read<mstudioanim_t>(s.animindex);
        a.offset[0] = static_cast<unsigned short>(f.header.texturedataindex - s.animindex);
        f.Write(s.animindex, a);
    },
           "position animation channel outside kept data");

    //The legacy checker read the next count without ensuring a short remained.
    reject([](auto& f) {
        auto b        = f.template Read<mstudiobodyparts_t>(f.header.bodypartindex);
        auto m        = f.template Read<mstudiomodel_t>(b.modelindex);
        auto mesh     = f.template Read<mstudiomesh_t>(m.meshindex);
        mesh.triindex = f.header.texturedataindex - sizeof(std::int16_t);
        f.Write(mesh.triindex, std::int16_t{1});
        f.Write(m.meshindex, mesh);
    },
           "unterminated triangle commands");

    studioseqhdr_t group{};
    std::memcpy(&group.id, "IDSQ", 4);
    group.version = 10;
    group.length  = sizeof(group);
    Expect(Reason::OK, api.CheckStudioModel(&group, sizeof(group), nullptr), "actual 76-byte IDSQ header");
    std::memset(group.name, 'a', sizeof(group.name));
    Expect(Reason::OK, api.CheckStudioModel(&group, sizeof(group), nullptr), "IDSQ name filling its field");
    ++group.length;
    Check(api.CheckStudioModel(&group, sizeof(group), nullptr) != Reason::OK, "IDSQ declared length beyond input");
    Check(api.CheckStudioModel(nullptr, sizeof(studiohdr_t), nullptr) != Reason::OK, "null model with nonzero length");

    //An unterminated event string ends exactly at a guard page. A bounded scan must reject it without reading byte N.
    auto eventFile               = valid;
    eventFile.header.numtextures = eventFile.header.textureindex = eventFile.header.texturedataindex = 0;
    eventFile.header.numskinref = eventFile.header.numskinfamilies = eventFile.header.skinindex = 0;
    auto           sequence                                                                     = eventFile.Read<mstudioseqdesc_t>(eventFile.header.seqindex);
    mstudioevent_t event{};
    std::memset(event.options, 'a', sizeof(event.options));
    sequence.eventindex = eventFile.Append(event);
    sequence.numevents  = 1;
    eventFile.Write(eventFile.header.seqindex, sequence);
    eventFile.Finish();
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    auto* pages = static_cast<byte*>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages) throw std::runtime_error("guard page allocation failed");
    DWORD      oldProtect{};
    const bool protectedPage = VirtualProtect(pages + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &oldProtect) != 0;
    Check(protectedPage && eventFile.bytes.size() < info.dwPageSize, "guard page setup");
    if (protectedPage && eventFile.bytes.size() < info.dwPageSize)
    {
        auto* data = pages + info.dwPageSize - eventFile.bytes.size();
        std::memcpy(data, eventFile.bytes.data(), eventFile.bytes.size());
        Check(api.CheckStudioModel(data, eventFile.bytes.size(), nullptr) != Reason::OK, "event string at guard page");
    }
    VirtualFree(pages, 0, MEM_RELEASE);
}

void CheckSequenceGroups(HMODULE library, IUtilAssetsIntegrity& api)
{
    using Reason = UtilAssetsIntegrityCheckReason;
    //The contextual check is an interface method; no separate exported symbol exists.
    Check(GetProcAddress(library, "UtilAssetsIntegrity_CheckStudioSequenceGroup") == nullptr, "no standalone sequence group export");
    const auto checkGroup = [&api](const void* model, size_t modelBytes, int groupIndex, const void* group, size_t groupBytes, UtilAssetsIntegrityCheckResult_StudioModel* result) {
        return api.CheckStudioSequenceGroup(model, modelBytes, groupIndex, group, groupBytes, result);
    };
    auto model                 = StudioFixture{};
    model.header.numseqgroups  = 2;
    model.header.seqgroupindex = model.AppendStructure(std::array<mstudioseqgroup_t, 2>{});
    auto seq                   = model.Read<mstudioseqdesc_t>(model.header.seqindex);
    seq.seqgroup               = 1;
    seq.animindex              = sizeof(studioseqhdr_t);
    model.Write(model.header.seqindex, seq);
    model.Finish();
    Expect(Reason::OK, api.CheckStudioModel(model.bytes.data(), model.bytes.size(), nullptr), "main model with external group");
    studioseqhdr_t header{};
    std::memcpy(&header.id, "IDSQ", 4);
    header.version = 10;
    header.length  = sizeof(header) + sizeof(mstudioanim_t);
    std::vector<std::uint8_t> group(header.length);
    std::memcpy(group.data(), &header, sizeof(header));
    Expect(Reason::OK, checkGroup(model.bytes.data(), model.header.texturedataindex, 1, group.data(), group.size(), nullptr), "constant external animation with cached main header");
    seq.numblends = 2;
    model.Write(model.header.seqindex, seq);
    Check(checkGroup(model.bytes.data(), model.header.texturedataindex, 1, group.data(), group.size(), nullptr) != Reason::OK, "all external animation blends checked");
    seq.numblends = 1;
    model.Write(model.header.seqindex, seq);

    const std::array<std::int16_t, 3> run{static_cast<std::int16_t>((3 << 8) | 2), 10, 20};
    const size_t                      runOffset = group.size();
    group.resize(group.size() + sizeof(run));
    std::memcpy(group.data() + runOffset, run.data(), sizeof(run));
    header.length = static_cast<int>(group.size());
    std::memcpy(group.data(), &header, sizeof(header));
    mstudioanim_t anim{};
    anim.offset[0] = sizeof(anim);
    std::memcpy(group.data() + seq.animindex, &anim, sizeof(anim));
    Expect(Reason::OK, checkGroup(model.bytes.data(), model.header.texturedataindex, 1, group.data(), group.size(), nullptr), "valid position RLE channel");
    group[runOffset] = group[runOffset + 1] = 0;
    Check(checkGroup(model.bytes.data(), model.header.texturedataindex, 1, group.data(), group.size(), nullptr) != Reason::OK, "zero-length RLE run");
    group[runOffset]     = 3;
    group[runOffset + 1] = 2;
    Check(checkGroup(model.bytes.data(), model.header.texturedataindex, 1, group.data(), group.size(), nullptr) != Reason::OK, "RLE valid exceeds total");

    //Stock group files end right after their last span, whose last frame reads the next sample with no weight: that
    //read lies in the tail callers keep after the group.
    const std::array<std::int16_t, 3> finalRun{static_cast<std::int16_t>((2 << 8) | 2), 10, 20};
    std::memcpy(group.data() + runOffset, finalRun.data(), sizeof(finalRun));
    Expect(Reason::OK, checkGroup(model.bytes.data(), model.header.texturedataindex, 1, group.data(), group.size(), nullptr), "last frame lookahead in the caller's tail");

    //A main model keeps no tail: the same span at the end of its kept structure is rejected, and accepted when it
    //covers more frames than the sequence, which reads nothing after it.
    auto       mainModel = StudioFixture{};
    const auto mainSeq   = mainModel.Read<mstudioseqdesc_t>(mainModel.header.seqindex);
    const auto mainRun   = mainModel.AppendStructure(finalRun);
    auto       mainAnim  = mainModel.Read<mstudioanim_t>(mainSeq.animindex);
    mainAnim.offset[0]   = static_cast<unsigned short>(mainRun - mainSeq.animindex);
    mainModel.Write(mainSeq.animindex, mainAnim);
    Check(api.CheckStudioModel(mainModel.bytes.data(), mainModel.bytes.size(), nullptr) != Reason::OK, "main model lookahead past its kept structure");
    mainModel.Write(mainRun, static_cast<std::int16_t>((3 << 8) | 2));
    Expect(Reason::OK, api.CheckStudioModel(mainModel.bytes.data(), mainModel.bytes.size(), nullptr), "main model span longer than its sequence");
}
