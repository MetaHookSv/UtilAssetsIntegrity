# UtilAssetsIntegrity

A Windows x86 utility DLL for checking GoldSrc StudioModel assets and indexed-color BMP images.
Originally part of [MetaHookSv](https://github.com/hzqst/MetaHookSv/tree/main/PluginLibs/UtilAssetsIntegrity).

The migration preserves the existing validation behavior and the
`CreateInterface("UtilAssetsIntegrityAPI_001", ...)` interface. See
[中文说明](README.zh-CN.md) and [build details](docs/build.md).

## Build and test

Requirements: Windows, Visual Studio 2022 with the C++ desktop workload and Windows SDK,
CMake 3.21 or newer, and Git. The first configure downloads pinned MetaHook SDK,
FreeImage and ScopeExit sources, plus the SHA-256 verified VC-LTL 5.3.1 binary package.
MetaHook's launcher and ScopeExit's examples are not built.

```bat
scripts\build-UtilAssetsIntegrity-x86-Release.bat
scripts\build-UtilAssetsIntegrity-x86-Debug.bat
```

Each script configures, builds, runs CTest, installs, and repeats the public-interface
smoke tests against the installed DLLs. Output is under `build/x86/<Configuration>`
and `install/x86/<Configuration>`. Tests dynamically load the DLL, obtain its factory,
and check StudioModel and BMP behavior without running a game.

Local source trees can override automatic downloads through CMake cache arguments or
environment variables. Explicit source trees are read-only inputs; CMake cache arguments
take precedence. For example:

```bat
scripts\build-UtilAssetsIntegrity-x86-Release.bat "-DMETAHOOK_SOURCE_PATH=D:/MetaHookSv-org/MetaHook" "-DFREEIMAGE_SOURCE_PATH=D:/MetaHookSv/thirdparty/FreeImage_clone" "-DSCOPEEXIT_SOURCE_PATH=D:/MetaHookSv/thirdparty/ScopeExit"
```

`VC_LTL_Root` accepts an existing VC-LTL binary package. Without an override, the
verified package is cached in `thirdparty/cache`. A direct CMake build can set
`BUILD_TESTING=OFF`; the convenience scripts require tests to be enabled.

## Install and use

The installed layout is:

```text
svencoop/metahook/dlls/UtilAssetsIntegrity.dll
svencoop/metahook/dlls/UtilAssetsIntegrity.pdb
svencoop/metahook/dlls/FreeImage/FreeImage.dll   (FreeImaged.dll for Debug)
include/Interface/IUtilAssetsIntegrity.h
licenses/
```

Copy the installed `svencoop` directory over the game's mod directory. The host must
make the FreeImage directory available to the Windows DLL loader before loading
`UtilAssetsIntegrity.dll`, as MetaHook's dependency search setup does.

Consumers include `IUtilAssetsIntegrity.h` plus the MetaHook SDK's `interface.h`, load
the DLL and request `UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION` from its `CreateInterface`
factory. The returned instance is a singleton owned by the DLL. Keep it loaded while
using the interface and do not delete the instance.

- `CheckStudioModel` accepts the existing IDST/IDSQ model formats, version 10.
- `Check8bitBMP` checks for FreeImage's indexed-color classification and applies
  `MaxWidth`, `MaxHeight` and `MaxSize` when a result object is supplied.
- `MaxSize` is the decoded pixel count, not the input file size. All three limits
  default to zero; set them before checking a nonempty image. Passing a null result
  pointer skips the size limits.
- A failed check returns a `UtilAssetsIntegrityCheckReason` and fills `ReasonStr`
  when a result object is supplied. The migration retains the original checks;
  see [the module note](memory/UtilAssetsIntegrity.md) for their boundaries.

## Builds and releases

GitHub Actions builds and tests x86 Release for main pushes, pull requests and manual
runs. Tags matching `v*` create a release. Only `svencoop/` and `include/` from the
installed directory are packaged as `UtilAssetsIntegrity-windows-x86.7z`, preserving
both top-level directories, and verified with `7z t`.

## License

The module retains MetaHookSv's [MIT license](LICENSE). Dependencies retain their own
copyright and license notices; see [third-party notices](THIRD-PARTY-NOTICES.md).
