# 兼容性

## 当前目标

| 组件 | 目标 | 状态 |
| --- | --- | --- |
| 平台 | Windows x64 | 目标平台 |
| Minecraft | Bedrock 客户端 | 目标环境 |
| LeviLamina | 26.1 | 目标版本 |
| 构建目标 | 原生客户端模组 | 目标类型 |
| OreUI 运行时 Hook | 需要目标版本证据 | 尚未实现 |
| UI 挂载 | 需要目标版本证据 | 尚未实现 |

## 兼容性是多维度条件

OreUI 兼容性不能只依据 Minecraft 版本判断。每条验证记录必须包含：

- Minecraft 版本。
- LeviLamina 版本。
- DearOreUI 版本或提交号。
- Windows 版本和架构。
- 检测到的 OreUI 资源布局。
- 入口资源和页面类型。
- OreUI 资源指纹。
- 可用时记录 Coherent 宿主信息。
- Hook 状态和能力报告。
- 诊断 ID 和验证证据。

## 支持状态

```text
Supported       已支持
Experimental    实验性
Unknown         未知
Unsupported     不支持
```

`Unknown` 不能被当作 `Supported`。版本化变换只有在页面、资源、版本、指纹、匹配器和结果校验全部通过后才能执行。

## 兼容性记录

报告兼容性验证结果时使用以下结构：

```text
Minecraft:
LeviLamina:
DearOreUI:
Platform:
Page type:
Entry resource:
Resource fingerprint:
Coherent host:
Hook state:
Capability state:
Injection result:
Diagnostic IDs:
Evidence:
```

## 已知边界

当前仓库尚未确认真实 OreUI 或 Coherent Hook 点、资源拦截边界、JavaScript 执行入口和 UI 挂载机制。这些属于待验证工作，不是当前兼容性保证。
