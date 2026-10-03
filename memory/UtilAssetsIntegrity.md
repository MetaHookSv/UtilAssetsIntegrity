---
title: UtilAssetsIntegrity
type: note
permalink: utilassetsintegrity/util-assets-integrity
---

# UtilAssetsIntegrity

## Overview and responsibilities

Standalone Windows x86 DLL migrated from `MetaHookSv/PluginLibs/UtilAssetsIntegrity`.
It provides basic integrity and bounds validation for GoldSrc StudioModel assets and
indexed-color BMP images through `IUtilAssetsIntegrity`. This migration preserves
the source implementation and ABI. It does not expand the original validation guarantees.

## Files and architecture

- `src/UtilAssetsIntegrity.cpp`: `CUtilAssetsIntegrity`, validation and singleton registration.
- `src/dllmain.cpp`: no-op DLL entry point.
- `include/Interface/IUtilAssetsIntegrity.h`: public methods, reason enum and result objects.
- MetaHook SDK `include/HLSDK/common/interface.cpp`: exports `CreateInterface` and owns
  the registration chain. It is compiled into this DLL, not linked from the launcher.
- `CMakeLists.txt`, `cmake/`: C++20 MSVC x86 build and pinned dependencies.
- `tests/SmokeTests.cpp`: actual DLL factory and validation integration tests.

`EXPOSE_SINGLE_INTERFACE` registers a DLL-owned `CUtilAssetsIntegrity` singleton under
`UtilAssetsIntegrityAPI_001`. Callers load the DLL, resolve `CreateInterface` and request
that version. The interface pointer remains valid only while the DLL is loaded.

## Public contract and data flow

`CheckStudioModel(buf, bufSize, result)` recognizes `IDST` and `IDSQ` and requires
version 10. IDST checks select a system-memory boundary from `texturedataindex` or
`length`, then inspect texture tables/data, skin references, body parts/submodels/meshes,
triangle commands, bones, sequences/events/animation, hitboxes and bone controllers.
IDSQ retains the original version check. The entry point requires a buffer at least
as large as `studiohdr_t`, including for IDSQ.

`Check8bitBMP(buf, bufSize, result)` opens a FreeImage memory stream, decodes BMP data,
requires `FIC_PALETTE`, then applies width, height and pixel-count limits when a result
object is provided. ScopeExit releases both the decoded bitmap and stream. The method
name does not add a separate bit-depth check beyond the existing color classification.

Results use `UtilAssetsIntegrityCheckReason`: OK, Unknown, InvalidFormat, SizeTooLarge,
SizeTooSmall, BogusHeader, VersionMismatch and OutOfBound. Result objects provide
`ReasonStr[256]`; the BMP result adds `size_t` limits initialized to zero. Zero limits
are enforced; a null result pointer skips BMP limits and diagnostics.

## Dependencies and runtime layout

MetaHook supplies SDK headers and interface.cpp; ScopeExit supplies headers; FreeImage
is a shared library. VC-LTL is applied once before configuring FreeImage. Fixed versions,
external overrides and verification commands are recorded in `docs/build.md`.

The library and PDB install under `svencoop/metahook/dlls`, with FreeImage under its
`FreeImage` subdirectory. Debug imports `FreeImaged.dll`, Release imports `FreeImage.dll`.
Hosts must prepare the dependency search paths or preload the decoder before loading
the module. Public headers install under `include/Interface`; consumers also need the
MetaHook SDK's `interface.h`.

## Callers

The original `SCModelDownloader` plugin dynamically loads `UtilAssetsIntegrity.dll`,
gets the versioned interface, and validates models and BMP downloads before storing
them. Original `studiocheck` tooling also consumes model validation. These consumers
remain outside this standalone repository.

## Existing validation boundaries

The original implementation contains checks that warrant separate analysis before
changing its behavior: hitbox-table end validation, bone-controller table count and
index bounds, texture size multiplication, terminated strings, and the approximate
animation-data bounds check. Several checks operate on pointer ranges or signed counts
without comprehensive overflow handling. The DLL is a compatibility-preserving asset
filter; the migration's smoke tests cover the contract and runtime integration rather
than proving complete malformed-input safety.

## Verification

Both configuration scripts run the public-interface CTest against build-tree DLLs,
install the module and runtime, then repeat the test against the install tree. Before
release, verify x86 headers, the factory export, source-copy equivalence and the 7z
archive. Record actual execution results separately from the procedures above.
