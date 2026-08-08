<div align="center">
  <h1>DearOreUI</h1>
  <p><strong>在运行时扩展 Minecraft Bedrock 的 OreUI。</strong></p>
  <p>面向 LeviLamina 客户端的原生模组，用于读取、变换和注入 OreUI 资源。</p>

  <p>
    <a href="https://github.com/DearOreUI/DearOreUI/releases">发行版本</a>
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
    <a href="https://github.com/DearOreUI/DearOreUI/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/DearOreUI/DearOreUI/build.yml?branch=main&amp;style=for-the-badge&amp;label=build" alt="DearOreUI 构建状态"></a>
    <a href="https://github.com/DearOreUI/DearOreUI/releases"><img src="https://img.shields.io/github/v/release/DearOreUI/DearOreUI?style=for-the-badge&amp;label=release" alt="DearOreUI 最新发行版本"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-CC0--1.0-2f6f9f?style=for-the-badge" alt="CC0-1.0 许可证"></a>
    <a href="https://github.com/DearOreUI/DearOreUI/issues"><img src="https://img.shields.io/github/issues/DearOreUI/DearOreUI?style=for-the-badge" alt="DearOreUI 未关闭 Issue"></a>
  </p>
</div>

> [!WARNING]
> DearOreUI 目前处于早期开发阶段。当前仓库提供 LeviLamina 模组脚手架和构建配置；OreUI Hook、资源拦截、运行时注入、UI 挂载和公共 API 尚未成为稳定功能。

## 项目概览

DearOreUI 是面向 LeviLamina 26.1 的 Windows x64 客户端原生模组。项目目标是在 Minecraft 客户端运行期间读取原版 OreUI 资源，合并多个 Mod 声明的变更，并将经过校验的结果注入当前 OreUI 页面。

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

## 功能方向

以下能力属于项目规划，具体实现状态见下表。

- **运行时 OreUI 接入**：发现 Minecraft 客户端中的 OreUI 和 Coherent 页面生命周期。
- **资源拦截**：读取当前 OreUI 的 HTML、CSS、JavaScript 和二进制资源。
- **多 Mod 扩展**：统一收集多个 Mod 的资源、脚本、样式、UI 和变更声明。
- **冲突感知变换**：在修改目标前校验版本、指纹、依赖、冲突和唯一匹配。
- **运行时注入**：不修改游戏原始安装目录，将校验后的结果提交给当前 OreUI 页面。
- **渐进式 API**：从运行时查询和资源注册开始，逐步扩展到页面事件、UI 挂载、Host API 和高级变换。
- **宿主桥接**：提供显式注册、权限校验的页面 JavaScript 与原生代码通信能力。
- **运行时诊断**：为注册、变换、注入、运行时和清理失败生成结构化记录。

## 开发状态

| 能力 | 状态 |
| --- | --- |
| LeviLamina 模组生命周期 | 脚手架已具备 |
| Windows x64 客户端构建 | 已配置 |
| OreUI / Coherent 运行时 Hook | 规划中，需目标版本验证 |
| 原版 OreUI 资源快照 | 规划中，需目标版本验证 |
| 多 Mod 资源注册表 | 规划中 |
| C++ 与 JavaScript Host API | 规划中 |
| UI 挂载与页面生命周期 | 规划中，需目标版本验证 |
| 版本化代码变换 | 规划中 |

当前仓库不能被视为完整运行时实现。构建成功只能证明当前模组脚手架和构建配置可用。

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
├── api/          公共门面和稳定 API 类型
├── runtime/      运行时状态和子系统协调
├── hook/         OreUI / Coherent 生命周期接入
├── capability/   版本、页面和能力探测
├── page/         PageContext 和页面事件
├── source/       原版 OreUI 资源快照
├── registry/     Mod、资源和变更注册表
├── transform/    资源和代码变换
├── resource/     资源索引、URI 和权限控制
├── inject/       结果校验和页面注入
├── ipc/          C++ 与 JavaScript 通信
├── facet/        宿主能力适配
├── ui/           UI 挂载和显示抽象
└── diagnostic/   日志、错误和执行报告
```

当前实现以 `mod/` 下的脚手架为中心。其余模块会按照依赖顺序逐步加入，先验证运行时事实和公共契约，再实现高风险的页面与 Bundle 操作。

## 兼容性

当前目标是 Windows x64 的 LeviLamina 客户端原生模组：

| 组件 | 目标 |
| --- | --- |
| Minecraft Bedrock | Windows x64 客户端 |
| LeviLamina | 26.1 |
| 原生入口 | `DearOreUI.dll` |
| C++ 标准 | C++20 |
| 工具链 | Clang-CL |
| 构建系统 | xmake |
| 模组版本 | `0.1.0` |

OreUI Bundle 的兼容性不能只依据 Minecraft 版本判断，还取决于检测到的 OreUI 资源、Coherent 宿主、页面类型、资源指纹和可用能力。

## 快速开始

DearOreUI 尚未发布稳定运行时版本。开发构建需要准备 Windows x64 LeviLamina 26.1 客户端环境，然后在 `DearOreUI/` 目录执行：

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

生成文件位于 `bin/`。模组元数据位于：

- [manifest.json](manifest.json)
- [tooth.json](tooth.json)
- [xmake.lua](xmake.lua)

构建成功不代表运行时 OreUI 注入已经可用。测试 Hook 或页面变更前，请先按照目标版本验证计划收集证据。

## 从源码构建

### 环境要求

- Windows x64
- Git
- xmake
- Visual Studio 或提供 Clang-CL 的 LLVM 环境
- LeviLamina 26.1 开发环境

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

构建产物位于：

```text
bin/
```

## 测试

开发计划将确定性测试与目标客户端验证分开。

### 确定性测试

- Manifest、命名空间、版本、权限和结果校验
- 资源 URI 规范化和路径安全
- 资源指纹与替换条件
- 注册、注销、依赖和冲突处理
- 变换唯一命中和回退行为
- IPC 请求、响应、超时、取消和错误序列化
- PageContext 与 UI 状态转换
- 诊断记录关联

### 目标客户端验证

- Hook 发现和页面生命周期事件
- 原始 HTML、CSS、JavaScript 和二进制资源访问
- 无副作用 JavaScript 最小注入
- 页面重载、导航、并行页面和销毁
- C++ 到 JavaScript、JavaScript 到 C++ 通信
- 最小 UI 挂载和清理闭环
- 多个 Mod 同时参与同一页面
- 不支持版本安全禁用

真实运行时验证必须记录 Minecraft、LeviLamina 和 DearOreUI 版本、页面类型、资源指纹、Hook 状态、诊断 ID 和可复现证据。

## 开发路线

| 里程碑 | 范围 |
| --- | --- |
| M0 | 运行时事实与 Hook 可行性 |
| M1 | 公共类型、Manifest 与诊断 |
| M2 | 页面生命周期与 PageContext |
| M3 | 资源快照与最小注入 |
| M4 | C++ 到 JavaScript 的 Host 通信 |
| M5 | 多 Mod 变更、依赖与冲突 |
| M6 | UI 挂载与页面显示 |
| M7 | 版本化变换与 Facet Provider |
| M8 | App、Web 与示例 Mod 对接 |

完整的模块依赖、测试计划、里程碑门禁和风险见[模块依赖与开发计划](../Docs/DearOreUI-模块依赖与开发计划.md)。

## 文档

- [完整 API 架构](../Docs/DearOreUI-完整API架构设计.md)
- [渐进式 API 架构](../Docs/DearOreUI-渐进式API架构设计.md)
- [运行时 Hook 注入设计](../Docs/方案A-运行时Hook注入-顶层设计.md)
- [模块依赖与开发计划](../Docs/DearOreUI-模块依赖与开发计划.md)
- [开发索引](../Docs/development-index.md)
- [OreUI Customizer 参考分析](../Docs/ore-ui-customizer/01-整体架构与应用通信.md)

## 参考项目

[`libs/Ore-UI-Customizer-App`](../libs/Ore-UI-Customizer-App) 是只读参考项目，用于研究 OreUI 资源结构、版本差异、注入流程、Facet 访问、运行时诊断和预览行为。

它的 Electron 权限模型、编译 Bundle 正则策略和修改安装目录的工作流不会直接成为 DearOreUI 的运行时契约。

## 参与贡献

贡献前请阅读上方列出的开发和 API 文档。贡献应当：

- 区分已验证事实、设计目标和未解决的运行时问题。
- 在目标 Hook 和资源边界验证前，避免高风险 Bundle 变换。
- 为运行时敏感的修改补充测试或目标客户端验证。
- 修改 API、兼容性、诊断或开发契约时同步更新相关文档。
- 不提交密钥、个人路径、构建缓存、游戏日志或玩家隐私数据。
- 保持 `libs/` 下的只读参考项目不变。

请阅读[贡献指南](CONTRIBUTING.zh-CN.md)了解贡献流程，并阅读[安全政策](SECURITY.zh-CN.md)了解漏洞私下报告方式。

## 许可证

DearOreUI 使用 [CC0-1.0](LICENSE) 许可证发布。
