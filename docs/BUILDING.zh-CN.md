# 构建 DearOreUI

## 环境要求

- Windows x64
- Git
- xmake
- Visual Studio 或提供 Clang-CL 的 LLVM 环境
- LeviLamina 26.1 客户端开发环境

当前支持的开发目标是 Windows x64 客户端变体。仓库的 `xmake.lua` 为保持模板兼容仍暴露 `server` 配置，但 DearOreUI 的项目目标是仅客户端。

## 配置与构建

在仓库根目录执行：

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

Debug 构建：

```powershell
xmake f -a x64 -m debug -p windows --target_type=client -y
xmake -v -y
```

## 输出

构建产物写入 `bin/`。不要提交 `bin/`、`.xmake/`、生成的打包文件或本地日志。

## 故障排查

- 修改依赖版本或找不到依赖包时，执行 `xmake repo -u`。
- 确认当前工具链提供 Clang-CL 并以 Windows x64 为目标。
- 确认 xmake 可以获取 LeviLamina 26.1 开发包。
- 只有普通重新配置无法解决过期配置时，才清理本地生成的构建状态。
- 构建成功只能验证编译和打包，不能验证 Minecraft 中的 OreUI Hook、资源访问、注入或 UI 行为。

## 运行时验证

涉及运行时的修改必须在固定的 Minecraft 和 LeviLamina 客户端中验证，并记录版本、目标页面、资源指纹、Hook 状态、诊断 ID 和可复现证据。
