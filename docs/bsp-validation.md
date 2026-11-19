# BSP validation

`CheckBSP(buf, bufSize, layout, result)` checks a version 30 brush file before any of
it is parsed. It is appended to `UtilAssetsIntegrityAPI_001` after
`CheckStudioSequenceGroup()`, so the existing vtable slots and result layouts stay
unchanged. The input is borrowed synchronously and the DLL keeps none of it.

The guarantee is about the GoldSrc engine's loader and the code that reads what it
loads: for a file that passes, `Mod_LoadBrushModel`, `CalcSurfaceExtents`,
`Mod_SetParent`, `Mod_MakeHull0`, `Mod_SetupSubmodel`, the hull traces,
`RecursiveLightPoint`, `R_BuildLightMap` and `CM_DecompressPVS` stay inside the file's
records and terminate. Records are read at their fixed little-endian offsets, so the
validator includes neither an SDK's nor an engine's `bspfile.h`, and an engine's
enlarged `MAX_MAP_*` capacities do not change it.

## Inputs

- `layout`: `UtilAssetsIntegrityBSPLayout::GoldSrc`, or `BlueShift` for Blue Shift's
  brush files, which swap the entities and planes entries of the lump table. The file
  does not record which it uses; engines pick it by game, so the caller passes it. Any
  other value is `InvalidFormat`.
- `result->MaxLeafs`: the leaf records the caller's PVS buffers hold (an engine's
  `MAX_MAP_LEAFS`). More leaves is `SizeTooLarge`. As with the BMP limits, the limit
  starts at zero and is enforced; a null result pointer skips it and all diagnostics.

## Checks

Fields are read as the engine reads them: `dface_t` and `dclipnode_t` shorts and the
node children are signed; node face ranges, leaf mark surface ranges and mark surface
values are unsigned. Every range is checked without overflow.

- Header: at least 124 bytes, at most `INT_MAX`, version 30. Every lump lies inside
  the file, and the fixed-size lumps hold whole records. Models, nodes and leaves are
  not empty (the engine reads model 0, its head node and leaf 0 unconditionally).
- Planes, vertices, texture vectors and model bounds are finite; model mins do not
  exceed maxs.
- Edges reference vertices; surface edges reference edges, and `INT_MIN` is refused.
- Texture lump: the offset table and every present miptex (offset `-1` is absent) lie
  inside the lump; names are terminated; dimensions are 1..16384. An embedded miptex
  (`offsets[0] != 0`) is 16 aligned and holds its four mip levels, its color count and
  a palette of at least 256 colors, the size `GL_LoadTexture` reads. WAD textures are
  read by name, outside this check.
- Texture infos reference a miptex when the file has a texture lump.
- Faces reference a plane, a texture info and at least one surface edge in range.
  Extents are computed as `CalcSurfaceExtents` computes them, keeping its float
  extremes and its shorts. A face without `TEX_SPECIAL` may not exceed 512 texels.
  Unless the light offset is `-1`, it is not negative, and its samples (one 3-byte
  luxel per 16 texels plus one, per style before the first 255) lie inside the lighting
  lump. Only special faces over 512 are exempt: a missing texture clears a special
  face's flags, after which its samples are read. Without a lighting lump the engine's
  samples are its null lighting data plus the offset, so only offset 0 is accepted
  (the unlit `cz_worldmap.bsp` gives every face 0).
- Mark surfaces reference faces.
- Models reference their head node; a clipping hull's head is a clipnode, a negative
  contents, or the clipnode count; visible leaves are below the leaf count; faces are in
  range. Compilers give a model without clipping brushes the next model's first
  clipnode, which after the last model with clipnodes is the count (stock `c2a2a.bsp`,
  `de_inferno_cz.bsp` and `dod_jagd.bsp` have such models): callers must not trace a
  hull whose head is the count.
- Nodes reference a plane, children nodes or leaves (`-1 - child`), and faces in range.
- Leaves have negative contents, which `Mod_SetParent` and `Mod_MakeHull0` use to tell
  them from nodes, and mark surfaces in range. A visibility offset other than `-1`
  decodes a whole row as `CM_DecompressPVS` does: at least once, with every byte inside
  the visibility lump, until it reaches `(visleafs + 7) / 8` bytes for model 0's visible
  leaves, the leaf count `Mod_SetupSubmodel` gives the world. Compilers write longer
  rows (hlvis pads them to 64 leaves; the stock Opposing Force and many Counter-Strike
  maps hold a bit for every leaf), and the engine does not clip the run of zeros that
  crosses its row end, so a run may end anywhere up to the longer of those two rows.
  Within `MaxLeafs`, that stays inside the caller's `MAX_MAP_LEAFS / 8` buffers; callers
  that decode into rows sized by the visible leaves must clip the run. Without a
  visibility lump, as for lighting, only offset 0 is accepted, which the engine reads
  as seeing every leaf.
- Clipnodes reference a plane and children clipnodes; negative children are contents.
- Graphs: the node and clipnode graphs are acyclic and no deeper than 1024 levels,
  bounding the engine's recursion. Clipnode subtrees may be shared, as compilers merge
  identical clipnodes. The drawing tree below node 0, which `Mod_SetParent` recurses,
  reaches every node once.

Failures fill `ReasonStr` with a `BSP:` diagnostic: `SizeTooSmall` and
`VersionMismatch` for the header, `SizeTooLarge` for the size and leaf limits,
`InvalidFormat` for record sizes, non-finite values, extents and graph shapes, and
`OutOfBound` for references and ranges.

## Left to the caller

- WAD textures, which are loaded by name from outside the file.
- Terminating the entities: Condition Zero: Deleted Scenes ends them with a space, so
  the lump is not required to hold a zero; callers copy it with a terminator.
- Engine policy that depends on how it draws a face, such as `GL_SubdivideSurface`'s
  64-edge limit for warped faces, chosen by texture name.
- The meaning of entity key values.

## Tests

`tests/BSPTests.h` builds a complete one-face world lump by lump and checks it through
the real DLL, then breaks each rule above one field at a time, including both
layouts. The smoke runner's optional directory argument also scans every `.bsp` below
it, using the Blue Shift layout below a `bshift` directory.

## Stock map scan

Scanning 496 brush files of an installed Half-Life (valve, bshift, gearbox, cstrike,
czero, czeror, dod and several mods) accepted all but two `!Src.bsp` files in `czero`
and `trinity`, whose surface extents exceed 512; the engine's `CalcSurfaceExtents`
refuses them too. The relaxed rules above (clipping heads at the count, runs past the
engine row, offset 0 without lighting or visibility, unterminated entities) each come
from stock files this scan first rejected.
