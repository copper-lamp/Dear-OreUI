# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-08-08

### Added

- LeviLamina 26.10.x native mod scaffold.
- Windows x64 client build configuration using C++20, Clang-CL, and xmake.
- Initial mod metadata and Tooth packaging configuration.
- Runtime, API, compatibility, testing, and development architecture documentation.
- English and Simplified Chinese project entry points and contributor guidance.

### Known Limitations

- OreUI and Coherent runtime hooks are not implemented.
- Original OreUI resource snapshots are not implemented.
- Runtime resource injection and UI mounting are not implemented.
- The public multi-mod registry, Host API, Facet providers, and versioned transforms are not stable features.
- This release should be treated as an early development scaffold, not as a complete OreUI runtime.

### Compatibility

- Target platform: Windows x64.
- Target environment: LeviLamina 26.10.x client.
- Build target: native shared-library mod.
- A successful build does not establish runtime OreUI compatibility.

### 中文说明

#### 新增

- LeviLamina 26.10.x 原生模组脚手架。
- 基于 C++20、Clang-CL 和 xmake 的 Windows x64 客户端构建配置。
- 初始模组元数据和 Tooth 打包配置。
- 运行时、API、兼容性、测试和开发架构文档。
- 英文和简体中文项目入口及贡献指南。

#### 已知限制

- 尚未实现 OreUI 和 Coherent 运行时 Hook。
- 尚未实现原版 OreUI 资源快照。
- 尚未实现运行时资源注入和 UI 挂载。
- 多 Mod 注册表、Host API、Facet Provider 和版本化变换尚未成为稳定功能。
- 本版本应视为早期开发脚手架，而不是完整 OreUI 运行时。

#### 兼容性

- 目标平台：Windows x64。
- 目标环境：LeviLamina 26.10.x 客户端。
- 构建目标：原生共享库模组。
- 构建成功不代表运行时 OreUI 兼容性已经成立。
