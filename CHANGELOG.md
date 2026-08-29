# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2026-08-29

> 本版本包含阶段 1~8 的运行时实现与优化，已作为稳定运行时发布。已知边界见 [README_ZH.md](README_ZH.md)「兼容性」一节。

### Added

- OreUI / Coherent 运行时 Hook 层：TechStack、SceneProvider、Router、`OreUI::View::initialize`、`OnReadyForBindings`、`triggerEvent`、`ClientInstance::update`。
- 真实显示链路：捕获 `cohtml::View` → `OnReadyForBindings` 门控 → `CoherentHostBridge::sendScript` 执行 → CSSOM 构建 DOM Overlay，已在世界列表页真实验证。
- 原版资源快照：`FileSystemSourceReader` 读取 `gui/dist/hbui`，配 `ResourceUri` / `ResourceIndex` 路径与权限管理。
- 多 Mod 注册表：注册/注销、依赖排序、冲突检测、每页面统一变更计划。
- 声明式 UI：`registerMod` / `registerOverlay` → `UiPlanner` / `MountManager` / `UiStateMachine`；52 个组件 showcase 在真实客户端显示。
- Host / Facet 桥：`HostDispatcher`、`HostMethodRegistry`、`OreUIFacetBridge`、`RuntimeInfoFacet`，JS→C++ 走游戏原生 Facet 协议并完成一次真实 roundtrip。
- 公共 API 门面：`IDearOreUIApi` 合并 Runtime / Resource / Mod / Host / UI / Page / Event / Transform / Diagnostic / Frame / RuntimeReport；外部 Mod 经纯 C ABI 桥 `DearOreUI_QueryApi` 获取实例。
- 页面 JS API：注入 `window.__DearOreUI__`（`bus`、`ipc`）与 `window.DearOreUI`（`call`、`report`）及基础 `oreui.*` 命名空间。
- 诊断设施：JSONL 事件流、各阶段遥测、注入报告、崩溃探针。
- 单元测试：注册表、变换、IPC、UI、组件、诊断覆盖；`DearOreUIUnitTests` 当前构建退出码为 0。
- 外部 Mod 示例与 ABI 契约文档。

### Known Boundaries

- JsonUI 页面（主菜单、世界内界面）不在 OreUI 管线覆盖范围内。
- 每个 OreUI View 目前只允许一次有效 JS→C++ 业务派发；同页多次调用待攻坚。
- 资源 / 代码变换已内部实现，尚未作为公共 API 开放。
- 公开事件订阅、页面订阅、诊断查询门面尚未形成。
- 诊断设施（vtable 转储、ODS 钩子等）发布前需评估精简。
- 卸载与生命周期清理路径仍需回归验证。
- 仅对记录的目标版本验证过；其余版本状态为「未知」。

## [0.1.0] - 2026-08-08

### Added

- LeviLamina 26.10.x native mod scaffold.
- Windows x64 client build configuration using C++20, Clang-CL, and xmake.
- Initial mod metadata and Tooth packaging configuration.
- Runtime, API, compatibility, testing, and development architecture documentation.
- English and Simplified Chinese project entry points and contributor guidance.

### 中文说明

#### 新增

- LeviLamina 26.10.x 原生模组脚手架。
- 基于 C++20、Clang-CL 和 xmake 的 Windows x64 客户端构建配置。
- 初始模组元数据和 Tooth 打包配置。
- 运行时、API、兼容性、测试和开发架构文档。
- 英文和简体中文项目入口及贡献指南。

> 0.1.0 是脚手架基线版本。阶段 1~8 的实现与验证记录在 `[Unreleased]` 中，当前代码树的状态以 README 为准。