# UtilAssetsIntegrity

用于检查 GoldSrc StudioModel 和索引色 BMP 图像的 Windows x86 工具 DLL。
源码来自 [MetaHookSv](https://github.com/hzqst/MetaHookSv/tree/main/PluginLibs/UtilAssetsIntegrity)，
保持原有校验行为及 `CreateInterface("UtilAssetsIntegrityAPI_001", ...)` 接口兼容。

## 构建与测试

需要 Windows、Visual Studio 2022 的 C++ 桌面开发组件和 Windows SDK、CMake 3.21 以上、Git。
首次配置自动获取固定版本的 MetaHook SDK、FreeImage、ScopeExit，以及通过 SHA-256 校验的
VC-LTL 5.3.1 二进制包。仅使用 MetaHook 的 SDK，不构建启动器。

```bat
scripts\build-UtilAssetsIntegrity-x86-Release.bat
scripts\build-UtilAssetsIntegrity-x86-Debug.bat
```

脚本依次配置、构建、运行 CTest、安装，并对安装目录重复 DLL 加载与接口测试。
构建结果位于 `build/x86/<Configuration>`，安装结果位于 `install/x86/<Configuration>`。
测试无需启动游戏。

可通过 CMake 参数或同名环境变量指定 `METAHOOK_SOURCE_PATH`、`FREEIMAGE_SOURCE_PATH`、
`SCOPEEXIT_SOURCE_PATH` 和 `VC_LTL_Root`。CMake 缓存参数优先于环境变量，外部源码树保持只读。
自动下载的 VC-LTL 缓存在 `thirdparty/cache`。详见[构建说明](docs/build.md)。

## 安装与调用

将安装结果中的 `svencoop` 目录覆盖到游戏的同名 mod 目录。
`UtilAssetsIntegrity.dll` 和 PDB 位于 `svencoop/metahook/dlls`；FreeImage 位于其 `FreeImage`
子目录，Release 使用 `FreeImage.dll`，Debug 使用 `FreeImaged.dll`。
加载前，宿主需要将 FreeImage 目录加入 DLL 搜索范围，或先加载对应的运行库。

公共头文件位于仓库的 `include/Interface/IUtilAssetsIntegrity.h`（不随发布压缩包分发），
调用方还需 MetaHook SDK 的 `interface.h`。从 DLL 的 `CreateInterface` 请求
`UTIL_ASSETS_INTEGRITY_INTERFACE_VERSION`。
返回对象由 DLL 持有，使用期间保持 DLL 已加载，不要删除该对象。

- `CheckStudioModel` 检查原有 IDST/IDSQ、版本 10 的模型格式。
- `Check8bitBMP` 使用 FreeImage 的索引色分类检查图像，传入结果对象时应用
  `MaxWidth`、`MaxHeight` 和 `MaxSize` 限制。
- `MaxSize` 指解码后的像素数量，三项限制默认为零，检查非空图像前需要设置；
  传入空结果指针时跳过尺寸限制。
- 失败通过 `UtilAssetsIntegrityCheckReason` 和结果对象的 `ReasonStr` 返回。
  校验边界见[工程概览](memory/project_overview.md)。

## CI 与发布

main push、PR 和手动触发执行 x86 Release 构建测试，`v*` 标签触发发布。
仅将安装目录中的运行时 `svencoop/` 目录打包为 `UtilAssetsIntegrity-windows-x86.7z`，
保留该顶层目录，上传前执行 `7z t` 校验。

## 许可证

模块沿用原始 [MIT 许可证](LICENSE)。依赖保留各自许可证，详见
[第三方声明](THIRD-PARTY-NOTICES.md)。
