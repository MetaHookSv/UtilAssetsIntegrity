# Repository guidance

This repository builds the standalone UtilAssetsIntegrity utility DLL. Read
`memory/UtilAssetsIntegrity.md` for the module and public interface, then
`docs/build.md` for build inputs and verification. Knowledge notes use the
`utilassetsintegrity/` permalink prefix. Read and edit them locally unless a Basic
Memory project has been bound to this repository; the original `metahooksv` project
belongs to the source repository.

Runtime code lives in `src/`, the public interface in `include/Interface/`, CMake
dependency setup in `cmake/`, and DLL integration tests in `tests/`.

Preserve `UtilAssetsIntegrityAPI_001`, its vtable and public result layouts unless an
interface change is explicitly requested. The initial migration retains the original
validation behavior. Do not modify external dependency trees or fetched vendor sources.

Run both x86 Debug and Release build scripts for relevant changes. Each runs CTest and
tests installed DLLs. Keep tests focused on public behavior and integration; avoid
assertions over documentation, configuration text or generated build artifacts.
Build, install and dependency-cache directories are ignored. See `docs/build.md` for
the release delivery gates. Do not commit, push or publish without authorization.
