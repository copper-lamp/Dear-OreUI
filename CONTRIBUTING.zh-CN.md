# 参与 DearOreUI 贡献

感谢你帮助改进 DearOreUI。欢迎提交原生运行时、兼容性研究、文档、构建工具和测试覆盖方面的聚焦修改。

## 开始之前

- 阅读 [README_ZH.md](README_ZH.md)、[完整 API 架构](../Docs/DearOreUI-完整API架构设计.md) 和[模块依赖与开发计划](../Docs/DearOreUI-模块依赖与开发计划.md)。
- 创建 Issue 前先搜索已有问题。
- 保持修改聚焦，避免无关重构。
- 不修改 `libs/` 下的只读参考项目。
- 不要把尚未验证的 Hook、资源边界或 UI 机制描述成已经实现。

## 项目约束

DearOreUI 面向 Windows x64 的 LeviLamina 26.1 客户端。当前仓库仍是运行时脚手架，构建成功不代表 OreUI Hook 或注入功能已经可用。

开发必须遵循以下顺序：

```text
运行时事实
    ↓
公共类型与诊断
    ↓
Manifest 与 API 门面
    ↓
Hook、能力探测和 PageContext
    ↓
资源快照与最小注入
    ↓
Host Bridge 与 Facet 适配
    ↓
多 Mod 注册表与变换
    ↓
UI 挂载
    ↓
高级兼容 Patch
```

## 构建

在 `DearOreUI/` 目录执行：

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

构建产物位于 `bin/`。当前支持的开发目标是 Windows x64 客户端变体。

## 格式化与本地检查

使用仓库配置格式化修改过的 C++ 文件：

```powershell
clang-format -i <changed-cpp-or-header-files>
```

创建 Pull Request 前执行：

```powershell
xmake -r -y
git diff --check
```

涉及运行时的修改还必须在固定的 Minecraft 和 LeviLamina 客户端中验证。仅构建成功不足以证明 Hook、资源、Host API 或 UI 修改正确。

## 运行时验证

修改 `hook`、`source`、`resource`、`inject`、`ipc`、`facet` 或 `ui` 时，请记录：

- Minecraft 和 LeviLamina 版本。
- DearOreUI 版本或提交号。
- Windows 版本和架构。
- 页面类型和入口资源。
- 相关资源指纹。
- Hook 状态和能力报告。
- 诊断 ID 及相关日志。
- 页面创建、重载、导航和销毁行为。

不要上传包含私人服务器地址、玩家数据、访问令牌或未经脱敏日志的附件。

## API 与兼容性修改

修改 API、资源 URI、Manifest、权限、PageContext、Host API、事件、变换或诊断协议时，必须同步更新 `../Docs/` 下对应文档。

修改版本化变换时，必须包含：

- 目标 Minecraft 和 LeviLamina 版本。
- 目标资源指纹。
- 匹配和校验条件。
- 目标不存在时的回退行为。
- 证明零命中和多命中都会被拒绝的测试。

## Pull Request

请在 Pull Request 中说明：

- 问题和解决方案的简要描述。
- 受影响的模块和契约。
- 已执行的验证，包括必要的运行时验证。
- 兼容性和迁移影响。
- 用户可见或架构行为对应的文档修改。
- 新增第三方代码或资源的许可证声明。

保持 Pull Request 聚焦。如果修改属于探索性工作，暂时无法在目标客户端验证，请标记为研究或实验性工作，并明确列出未解决的事实。

## 安全与隐私

不要在公开 Issue 中披露疑似漏洞。请按照 [SECURITY.zh-CN.md](SECURITY.zh-CN.md) 私下报告。

禁止提交密钥、个人绝对路径、构建缓存、游戏安装目录、私人日志、世界存档、回放文件或玩家信息。
