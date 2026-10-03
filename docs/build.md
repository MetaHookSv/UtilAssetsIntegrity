# Build and verification

## Direct CMake commands

From the repository root, for Release:

```bat
cmake -G "Visual Studio 17 2022" -A Win32 -S . -B build/x86/Release -DCMAKE_INSTALL_PREFIX=install/x86/Release -DBUILD_TESTING=ON
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure --no-tests=error
cmake --install build/x86/Release --config Release
build\x86\Release\tests\Release\UtilAssetsIntegritySmokeTests.exe install\x86\Release\svencoop\metahook\dlls\UtilAssetsIntegrity.dll install\x86\Release\svencoop\metahook\dlls\FreeImage\FreeImage.dll
```

For Debug, replace the configuration names and use `FreeImaged.dll` for the last argument.
Single-configuration MSVC generators must set `CMAKE_BUILD_TYPE=Debug` or `Release`.
Only Windows MSVC x86 is supported. The project uses C++20, `/MTd` in Debug and `/MT`
in Release with VC-LTL; Release enables interprocedural optimization.

## Dependency inputs

| Input | Required tree | Default |
| --- | --- | --- |
| `METAHOOK_SOURCE_PATH` | MetaHook SDK, including `include/metahook.h`, `include/HLSDK/common/interface.cpp`, `include/HLSDK/engine/studio.h`, and `LICENSE` | `MetaHookSv/MetaHook@4d23b6fecd79dc949aabc2e145480cd1328d4a35` |
| `FREEIMAGE_SOURCE_PATH` | FreeImage clone with `CMakeLists.txt`, `Source/FreeImage.h`, and root license files | `hzqst/FreeImage_clone@c68700b9fe699dbbf99f88a611065f101cba1a41` |
| `SCOPEEXIT_SOURCE_PATH` | ScopeExit with `include/ScopeExit/ScopeExit.h` and `LICENSE` | `SergiusTheBest/ScopeExit@bd345da594a4675d04de663d93d00cb81b6678b2` |
| `VC_LTL_Root` | VC-LTL binary package with its helper, configuration and Win32 libraries | Downloaded VC-LTL 5.3.1 |

CMake cache arguments take precedence over environment variables. A nonempty override
is validated before any dependency download, and is never downloaded into or modified.
Source downloads use FetchContent in the build tree with fixed commit hashes and no
submodules. ScopeExit is header-only. FreeImage builds in a separate binary directory;
its source files and vendor CMake files are unchanged.

The default VC-LTL archive is downloaded under `UTILASSETSINTEGRITY_DEPENDENCY_CACHE_DIR`
(default `thirdparty/cache`), verified against
`7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`,
and extracted to `VC-LTL-5.3.1`. A cache lock coordinates Debug and Release configuration.
An explicit `VC_LTL_Root` is a read-only input validated for required files.
For offline builds, provide all source overrides and an existing VC-LTL package.

## Verification gates

This migration uses Level 1 regression verification, followed by change review and
completion verification. Validation is exercised through the real DLL factory rather
than by including the implementation in a test executable. The smoke test covers
factory versioning, singleton behavior, model format/version/bounds failures, BMP
decoding, indexed-color rejection, limits and optional result pointers. The build
scripts repeat the same tests with the installed DLL paths.

Before delivery, build and test both configurations, check x86 headers and the
`CreateInterface` export, compare the migrated implementation and interface with their
source copies, then package only `svencoop/` and `include/` from the install directory
and run `7z t`. Preserve both top-level directories and check that the archive contains
no other paths. Record actual commands and results; a CI workflow definition alone
does not prove a successful run.

## Provenance

The initial module sources, DLL entry point, public interface and MIT license are
copied unchanged from `hzqst/MetaHookSv`. The original MSBuild project is replaced by
CMake. The standalone runtime layout retains the original `metahook/dlls` and
`metahook/dlls/FreeImage` placement. This library does not use engine gamedata or
require a plugin load-list entry.

## Initial migration verification (2026-10-03)

Verified on Windows with CMake 3.31.12, Visual Studio 2022 Community,
MSVC 19.44.35228.0 and Windows SDK 10.0.26100.0.

| Check | Observed result |
| --- | --- |
| `scripts/build-UtilAssetsIntegrity-x86-Release.bat` | Exit 0; default dependency fetch, build, CTest (1/1), install and installed-DLL smoke test succeeded. |
| `scripts/build-UtilAssetsIntegrity-x86-Debug.bat` | Exit 0; default dependency fetch, build, CTest (1/1), install and installed-DLL smoke test succeeded. |
| Reconfiguration and incremental builds | Both configuration scripts returned 0 after installation/documentation updates. |
| Explicit local inputs, `BUILD_TESTING=OFF` | CMake configuration/generation returned 0 using the original MetaHookSv, FreeImage and ScopeExit trees and the existing VC-LTL package. This check did not build that additional configuration. |
| Invalid `METAHOOK_SOURCE_PATH` | Configure returned 1 with the missing-header diagnostic before fetching dependencies. |
| x64 configuration | Configure returned 1 with the Windows MSVC x86 requirement. |
| `dumpbin /headers`, `/exports`, `/dependents` | Both installed DLLs are x86 and export `CreateInterface`; Release imports `FreeImage.dll`, Debug imports `FreeImaged.dll`. |
| SHA-256 comparison | Both migrated implementation files, the public interface and MIT license match the original files byte for byte. |
| Dependency revisions and working trees | All three fetched source dependencies use the documented pins in both configurations and have clean Git working trees. Original source and external input repositories also remain clean. |
| `actionlint .github/workflows/livebuild.yml .github/workflows/release.yml` | Exit 0; all three GitHub YAML files also parsed successfully with PyYAML. |
| `7z a`, `7z t`, archive extraction | Exit 0; the complete Release install was packaged, its contents inspected, and the public-interface smoke test passed against extracted DLLs. |

The archive is generated at `build/artifacts/UtilAssetsIntegrity-windows-x86.7z`.
The first dependency builds report warnings in bundled TIFF, LibRaw and OpenEXR
sources/archives, plus VC-LTL Debug CRT linker warning LNK4075. These vendor sources
were not modified or their diagnostics suppressed. No compiler warnings were observed
in the migrated module or new smoke-test source.

GitHub-hosted workflows and game integration were not executed locally. Changes are
uncommitted; this verification does not record a commit, push or published release.

## Packaging scope verification (2026-10-03)

After limiting the archive inputs to `svencoop/` and `include/`, local verification showed:

- Both x86 build scripts returned 0; CTest passed 1/1 in each configuration and both
  installed-DLL smoke tests passed.
- `actionlint .github/workflows/livebuild.yml .github/workflows/release.yml` returned 0;
  the composite action and both workflow YAML files parsed successfully with PyYAML.
- From `install/x86/Release`, `7z a -t7z <archive-path> svencoop include` and
  `7z t <archive-path>` returned 0. `7z l -slt -ba <archive-path>` confirmed only the
  two requested directory trees, containing the public header, module DLL/PDB and
  FreeImage DLL (four files total).
- `7z x` returned 0, and the Release public-interface smoke test passed against
  the extracted DLLs. GitHub-hosted workflows were not executed locally.
