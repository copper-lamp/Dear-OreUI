<div align="center">
  <h1>DearOreUI</h1>
  <p><strong>在运行时扩展 Minecraft Bedrock 的 OreUI。</strong></p>
  <p>面向 LeviLamina 客户端的原生模组，用于读取、变换和注入 OreUI 资源。</p>

  <p>
    <a href="https://github.com/copper-lamp/Dear-OreUI/releases">发行版本</a>
    ·
    <a href="CHANGELOG.md">更新日志</a>
    ·
    <a href="https://copper-lamp.github.io/dearoreui-docs/">项目文档</a>
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
    <a href="https://qm.qq.com/q/HvKKPKdSwi"><img src="https://img.shields.io/badge/QQ-%E5%8A%A0%E5%85%A5%E7%BE%A4%E8%81%8A-EA0000?style=for-the-badge&amp;logo=qq&amp;logoColor=white" alt="加入QQ群"></a>
  </p>
</div>

> [!NOTE]
> DearOreUI 已经走过了纯脚手架阶段。下面描述的 Hook、资源快照、多 Mod 注册表、变换、注入、Host/Facet 桥、UI 挂载和 API 门面都已实现，显示链路也在真实客户端、OreUI 技术栈页面上验证过
## 项目概览

DearOreUI 是面向 LeviLamina 26.10.x 的 Windows x64 客户端原生模组。它在 Minecraft 客户端运行期间读取原版 OreUI 资源，合并多个 Mod 声明的变更，并把校验后的结果注入当前 OreUI 页面。这让你可以专注地编写你的界面，无需操心与其它模组的ui兼容性
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

## 多 Mod 协作

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

在 LeviLamina 数据目录（如 BDS 服务器根目录）用 lip 安装最新稳定版：

```powershell
lip install github.com/copper-lamp/Dear-OreUI
```

模组会安装到 `mods/DearOreUI/`。开发构建需要准备 Windows x64 LeviLamina 26.10.x 客户端环境，然后在 `DearOreUI/` 目录执行：

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

生成文件位于 `bin/`。模组元数据位于：

- [manifest.json](manifest.json)
- [tooth.json](tooth.json)
- [xmake.lua](xmake.lua)

构建成功只证明编译和打包可用，不等于运行时 OreUI 注入已可用。

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

### 目标客户端验证

已记录：

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
