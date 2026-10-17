# Studio validation update

The public `UtilAssetsIntegrityAPI_001` vtable and result layouts are unchanged. The
implementation now uses checked integer ranges and copies records only after their
extents have been checked. It no longer performs out-of-range pointer arithmetic or
scans one byte beyond fixed strings. BMP resource cleanup uses standard RAII.

Fixed: hitbox table extent, controller table's incorrect bone-count multiplier,
controller bone/slot/input indices, texture multiplication and the full palette,
skin multiplication and zero-count bypasses, triangle count termination and vertex/
normal bounds. Additional checks cover bodypart zero divisors, bone parent cycles,
sequence groups, all animation blends (including Counter-Strike's nine-way blends),
all six RLE channels and interpolation lookahead, events, attachments and transitions.
IDST/IDSQ headers use their actual sizes; null buffers and signed-offset overflow are
rejected. Texture tables retain the SDK's `MAXSTUDIOSKINS` limit (100), with full
extent checks and skin references bounded by the actual texture count. The engine's
32-slot remap cache is not a format limit: models with 33, 34, or 99 textures are
not rejected merely for exceeding that cache capacity. Cache accesses need their
own bounds checks in the caller. The earlier 32-texture restriction was incorrect
and has been withdrawn.

The interface is published only through `EXPOSE_SINGLE_INTERFACE`, so callers resolve
`UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION` through `CreateInterface`. Main-model-aware
sequence validation is the `CheckStudioSequenceGroup()` interface method, appended
after the existing API_001 methods so their vtable slots stay unchanged. It takes the
main model structural header and an IDSQ file together. A plain `CheckStudioModel(IDSQ)`
validates only the sequence-file header because the file alone does not describe its
sequences, bones or animation record offsets. It must not be treated as full animation
validation. All input pointers are borrowed synchronously; the DLL retains none.

`tests/StudioModelTests.h` exercises the corrected cases through the real DLL, including
a string ending at a PAGE_NOACCESS guard page and malformed external RLE channels.
Both x86 build scripts run these regressions and the existing BMP tests against build
and installed DLLs. The smoke runner's optional third argument scans an MDL directory.

The corrected sources/header/tests were also migrated to HLND2T-DiligentGraphics,
where a local CMake target builds x86/x64 with the engine SDK and existing FreeImage.
That engine keeps separate Bridge resource/draw validation. Future fixes to the
shared source copies should be synchronized; platform build files remain separate.
