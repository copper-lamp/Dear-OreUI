<div align="center">
  <h1>DearOreUI</h1>
  <p><strong>在运行时扩展 Minecraft Bedrock 的 OreUI。</strong></p>
  <p>面向 LeviLamina 客户端的原生模组，用于读取、变换和注入 OreUI 资源。</p>

  <p>
    <a href="https://github.com/copper-lamp/Dear-OreUI/releases">发行版本</a>
    ·
    <a href="CHANGELOG.md">更新日志</a>
    ·
    <a href="../Docs/DearOreUI-完整API架构设计.md">API 架构</a>
    ·
    <a href="../Docs/DearOreUI-模块依赖与开发计划.md">开发计划</a>
    ·
    <a href="CONTRIBUTING.zh-CN.md">参与贡献</a>
    ·
    <a href="README.md">English</a>
  </p>

  <p>
    <a href="https://github.com/copper-lamp/Dear-OreUI/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/copper-lamp/Dear-OreUI/build.yml?branch=main&amp;style=for-the-badge&amp;label=build" alt="DearOreUI 构建状态"></a>
    <a href="https://github.com/copper-lamp/Dear-OreUI/releases"><img src="https://img.shields.io/github/v/release/copper-lamp/Dear-OreUI?style=for-the-badge&amp;label=release" alt="DearOreUI 最新发行版本"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-CC0--1.0-2f6f9f?style=for-the-badge" alt="CC0-1.0 许可证"></a>
    <a href="https://github.com/copper-lamp/Dear-OreUI/issues"><img src="https://img.shields.io/github/issues/copper-lamp/Dear-OreUI?style=for-the-badge" alt="DearOreUI 未关闭 Issue"></a>
  </p>
</div>

> [!NOTE]
> DearOreUI 已经走过了纯脚手架阶段。下面描述的 Hook、资源快照、多 Mod 注册表、变换、注入、Host/Facet 桥、UI 挂载和 API 门面都已实现，显示链路也在真实客户端、OreUI 技术栈页面上验证过。版本仍是 0.1.0，还没有公开发行版。JsonUI 页面（主菜单、世界内界面）不在覆盖范围内，每个 OreUI View 目前只允许一次 JS 发起的原生调用。这些边界列在「兼容性」一节。

## 项目概览

DearOreUI 是面向 LeviLamina 26.10.x 的 Windows x64 客户端原生模组。它在 Minecraft 客户端运行期间读取原版 OreUI 资源，合并多个 Mod 声明的变更，并把校验后的结果注入当前 OreUI 页面。

```text
原版 OreUI
    ↓
运行时 Hook 与页面发现
    ↓
原始资源快照
    ↓
多个 Mod 注册扩展
    ↓
依赖与冲突处理
    ↓
资源和代码变换
    ↓
校验后的注入结果
    ↓
OreUI 页面、Mod UI 与 Host API
```

DearOreUI 不是独立 UI 替代品，也不会向其他 Mod 直接暴露页面指针、编译后 Bundle 内部变量或任意本地文件访问。

## 已实现的能力

- **运行时 Hook。** 模组挂钩了 TechStack 选择、SceneProvider 场景创建、Router 导航、`OreUI::View::initialize`、`OnReadyForBindings`、`triggerEvent` 和 `ClientInstance::update`。
- **真实显示链路。** 捕获 `cohtml::View` 后，脚本以 `OnReadyForBindings` 为门控，通过 `CoherentHostBridge::sendScript` 执行；DOM Overlay 用 CSSOM 构建，不用 `innerHTML`。这条链路已在世界列表页（`/play/all`）真实验证过。
- **资源快照。** `FileSystemSourceReader` 读取原版 `gui/dist/hbui` 资源；`ResourceUri` 和 `ResourceIndex` 负责路径与访问管理。
- **多 Mod 注册表。** Mod 注册资源、脚本、样式、UI 和变更；统一注册表做依赖排序、冲突检测，并为每个页面生成一份变更计划。
- **声明式 UI。** `registerMod` / `registerOverlay` 进入 `UiPlanner`、`MountManager` 和 `UiStateMachine`。52 个组件的 showcase 已在真实客户端完整显示。
- **Host 桥。** `HostDispatcher` 和 `HostMethodRegistry` 把 JS 请求路由到原生方法并做权限校验。JS→C++ 走游戏原生 Facet 协议：`DearOreUI.call` → `facet:request` → `OreUIFacetBridge` → `HostDispatcher` → `bus.push`。
- **公共 API。** `IDearOreUIApi` 合并了 Runtime、Resource、Mod、Host、UI、Page、Event、Transform、Diagnostic、Frame、RuntimeReport 等门面。外部 Mod 通过纯 C ABI 桥（`DearOreUI_QueryApi`）获取实例。
- **JS 命名空间。** 页面内注入 `window.__DearOreUI__`（协议信息、`bus`、`ipc`）和 `window.DearOreUI`（`call`、`report`），以及基础的 `oreui.*` 命名空间。
- **诊断。** JSONL 事件流、各阶段遥测、注入报告和崩溃探针，全程不修改游戏安装目录。

## 开发状态

| 能力 | 状态 |
| --- | --- |
| LeviLamina 模组生命周期 | 已实现 |
| Windows x64 客户端构建 | 可用（xmake + Clang-CL） |
| OreUI / Coherent 运行时 Hook | 已实现，客户端验证通过 |
| 原版 OreUI 资源快照 | 已实现 |
| 多 Mod 资源注册表 | 已实现 |
| 依赖排序与冲突检测 | 已实现 |
| 资源与代码变换 | 内部已实现（ChangePlanner） |
| UI 挂载与页面生命周期 | 已实现，客户端验证通过 |
| C++ 与 JavaScript Host API | 基础 API 已实现，JS 侧已注入 |
| 崩溃隔离实验 | 已实现 |

尚未覆盖：

| 能力 | 状态 |
| --- | --- |
| JsonUI 页面（主菜单、世界内界面） | 不支持 |
| 同一 View 内多次 JS→C++ 调用 | 仅支持一次有效派发 |
| 公开事件 / 页面订阅门面 | 未形成 |
| 诊断查询门面 | 未形成 |
| 版本化变换作为公共 API | 未开放 |

单元测试套件（`DearOreUIUnitTests`）覆盖注册表、变换、IPC、UI、组件和诊断，当前构建退出码为 0。

## 渐进式 API 模型

```text
L0  运行时查询
    ↓
L1  资源、脚本和样式注册
    ↓
L2  页面生命周期与 PageContext
    ↓
L3  UI 挂载和页面扩展
    ↓
L4  经过权限校验的 Host API
    ↓
L5  版本化代码和资源变换
    ↓
L6  Facet Provider 与高级兼容适配
```

大多数 Mod 应停留在 L1。更高层能力需要理解页面生命周期、宿主能力、兼容条件或原版 Bundle 结构，因此采用显式选择。

## 多 Mod 协作模型

Mod 不直接修改同一份中间字符串，也不写入游戏安装目录。每个 Mod 向 DearOreUI 注册声明，DearOreUI 以原始资源快照为输入，为当前页面生成统一变更计划。

```text
Mod A 注册
Mod B 注册
Mod C 注册
    ↓
统一注册表
    ↓
页面、版本和能力筛选
    ↓
依赖排序
    ↓
冲突检测
    ↓
资源和代码变换
    ↓
完整性校验
    ↓
一次性提交注入
```

默认规则：

- 资源路径按 Mod 命名空间隔离。
- 相同注册内容按幂等处理。
- 同一资源的不同内容产生冲突。
- 资源替换必须声明原始指纹。
- 同一原始代码区域的多个替换默认冲突。
- 单个 Mod 失败不会影响无关变更。
- 不支持版本保留原始页面。
- 所有冲突、跳过、失败和回退都生成报告。

## 运行时架构

```text
src/
├── mod/          模组入口、生命周期和配置
├── api/          公共门面和稳定 API 类型（IDearOreUIApi）
├── bridge/       供外部 Mod 使用的纯 C ABI 桥
├── runtime/      运行时状态和子系统协调
├── hook/         OreUI / Coherent 生命周期接入
├── capability/   版本、页面和能力探测
├── page/         PageContext 和页面事件
├── source/       原版 OreUI 资源快照
├── resource/     资源索引、URI 和权限控制
├── registry/     Mod、资源和变更注册表
├── transform/    依赖解析、冲突检测、变更计划
├── render/       HTML DOM 解析与脚本序列化
├── component/    原版资源、主题令牌、组件渲染器
├── inject/       结果校验和页面注入
├── ipc/          C++ 与 JavaScript 通信
├── facet/        宿主能力适配
├── ui/           UI 挂载、规划和状态
├── diagnostic/   日志、遥测、报告、崩溃探针
└── poc/          历史导航 PoC（保留作参考）
```

模块的依赖顺序见「模块依赖与开发计划」，先验证运行时事实和公共契约，再实现高风险的页面与 Bundle 操作。

## 兼容性

当前目标是 Windows x64 的 LeviLamina 客户端原生模组：

| 组件 | 目标 |
| --- | --- |
| Minecraft Bedrock | Windows x64 客户端 |
| LeviLamina | 26.10.x |
| 原生入口 | `DearOreUI.dll` |
| C++ 标准 | C++20 |
| 工具链 | Clang-CL |
| 构建系统 | xmake |
| 模组版本 | `0.1.0` |

OreUI Bundle 的兼容性不能只依据 Minecraft 版本判断，还取决于检测到的 OreUI 资源、Coherent 宿主、页面类型、资源指纹和可用能力。

已验证的边界（来自阶段 7.1 和阶段 8-A 的客户端记录）：

- 显示链路在 OreUI 技术栈页面上可用，当前验证目标是世界列表页。
- 真实客户端上记录到一次完整的 JS→C++ roundtrip。
- JsonUI 页面（主菜单、世界内界面）走的是另一套技术栈，不在本管线内。
- 其他 Minecraft / LeviLamina 版本未验证。「未知」绝不能当作「支持」。

## 快速开始

DearOreUI 尚未发布稳定运行时版本。开发构建需要准备 Windows x64 LeviLamina 26.10.x 客户端环境，然后在 `DearOreUI/` 目录执行：

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

生成文件位于 `bin/`。模组元数据位于：

- [manifest.json](manifest.json)
- [tooth.json](tooth.json)
- [xmake.lua](xmake.lua)

构建成功只证明编译和打包可用，不等于运行时 OreUI 注入已可用。验证页面行为前，请先参照 `../Docs/` 下的验证记录收集目标客户端证据。

## 从源码构建

### 环境要求

- Windows x64
- Git
- xmake
- Visual Studio 或提供 Clang-CL 的 LLVM 环境
- LeviLamina 26.10.x 开发环境

### Release 构建

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

### Debug 构建

```powershell
xmake f -a x64 -m debug -p windows --target_type=client -y
xmake -v -y
```

构建产物位于 `bin/`。

## 测试

### 确定性测试

在构建输出中运行 `DearOreUIUnitTests`。套件覆盖：

- Manifest、命名空间、版本、权限和结果校验
- 资源 URI 规范化和路径安全
- 资源指纹与替换条件
- 注册、注销、依赖和冲突处理
- 变换唯一命中和回退行为
- IPC 请求、响应、超时、取消和错误序列化
- PageContext 与 UI 状态转换
- 组件渲染、原版资源和主题令牌
- 诊断记录关联

当前构建退出码为 0。

### 目标客户端验证

已记录（见 `../Docs/`）：

- Hook 发现和页面生命周期事件
- 真实 `cohtml::View` 捕获与 `OnReadyForBindings` 门控
- C++→JS 脚本执行与 CSSOM Overlay 构建
- 一次 JS→C++ Facet roundtrip，含 `bus.push` 响应
- 52 个组件的 UI showcase 在真实客户端挂载与清理

仍待解决：

- JsonUI 页面注入（主菜单、世界内界面）
- 同一 View 内多次 JS→C++ 派发
- 卸载与生命周期清理回归
- 目标版本之外的版本矩阵证据

真实运行时验证必须记录 Minecraft、LeviLamina 和 DearOreUI 版本、页面类型、资源指纹、Hook 状态、诊断 ID 和可复现证据。

## 开发路线

| 里程碑 | 范围 | 状态 |
| --- | --- | --- |
| M0 | 运行时事实与 Hook 可行性 | 完成 |
| M1 | 公共类型、Manifest 与诊断 | 完成 |
| M2 | 页面生命周期与 PageContext | 完成 |
| M3 | 资源快照与最小注入 | 完成 |
| M4 | C++ 到 JavaScript 的 Host 通信 | 完成 |
| M5 | 多 Mod 变更、依赖与冲突 | 完成 |
| M6 | UI 挂载与页面显示 | 完成 |
| M7 | 版本化变换与 Facet Provider | 基本完成；变换尚未开放为公共 API |
| M8 | App、Web 与示例 Mod 对接 | 进行中：外部 Mod 示例与 ABI 已完成；App/Web 待推进 |

当前状态与剩余门禁见 [API 状态核对（2026-08-22）](../Docs/DearOreUI-API状态核对-2026-08-22.md) 和[进度总结与阶段 8 规划](../Docs/DearOreUI-当前进度总结与阶段8规划.md)。

## 文档

- [完整 API 架构](../Docs/DearOreUI-完整API架构设计.md)
- [渐进式 API 架构](../Docs/DearOreUI-渐进式API架构设计.md)
- [运行时 Hook 注入设计](../Docs/方案A-运行时Hook注入-顶层设计.md)
- [模块依赖与开发计划](../Docs/DearOreUI-模块依赖与开发计划.md)
- [API 状态核对（2026-08-22）](../Docs/DearOreUI-API状态核对-2026-08-22.md)
- [生产级 API 完整化](../Docs/DearOreUI-生产级API完整化-需求架构执行.md)
- [外部 Mod API 示例与 ABI 契约](../Docs/DearOreUI-外部Mod-API示例与ABI契约.md)
- [外部 Mod 最小连接验证](../Docs/DearOreUI-外部Mod最小连接验证-需求架构执行.md)
- [进度总结与阶段 8 规划](../Docs/DearOreUI-当前进度总结与阶段8规划.md)
- [开发索引](../Docs/development-index.md)
- [OreUI Customizer 参考分析](../Docs/ore-ui-customizer/01-整体架构与应用通信.md)

## 参考项目

[`libs/Ore-UI-Customizer-App`](../libs/Ore-UI-Customizer-App) 是只读参考项目，用于研究 OreUI 资源结构、版本差异、注入流程、Facet 访问、运行时诊断和预览行为。

它的 Electron 权限模型、编译 Bundle 正则策略和修改安装目录的工作流不会直接成为 DearOreUI 的运行时契约。

## 参与贡献

贡献前请阅读上方列出的开发和 API 文档。贡献应当：

- 区分已验证事实、设计目标和未解决的运行时问题。
- 让运行时敏感的改动维持在既有验证记录之内。
- 为运行时敏感的修改补充测试或目标客户端验证。
- 修改 API、兼容性、诊断或开发契约时同步更新相关文档。
- 不提交密钥、个人路径、构建缓存、游戏日志或玩家隐私数据。
- 保持 `libs/` 下的只读参考项目不变。

请阅读[贡献指南](CONTRIBUTING.zh-CN.md)了解贡献流程，并阅读[安全政策](SECURITY.zh-CN.md)了解漏洞私下报告方式。

## 许可证

DearOreUI 使用 [CC0-1.0](LICENSE) 许可证发布。