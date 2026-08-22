# 发布工作流手动触发方案

## 需求

参考 `libs/Playback/.github/workflows/release.yml`，让 DearOreUI 的发布工作流除了在 Release 发布时自动运行外，还可以从 GitHub Actions 页面通过 `workflow_dispatch` 手动运行。

## 架构

现有发布链路由 `build`、`update-release-notes` 和 `upload-to-release` 三个 job 组成，依赖关系保持不变。仅扩展 workflow 触发器，增加 `workflow_dispatch`，不改变构建、Release Notes 更新和产物上传逻辑。手动运行时仍需选择一个 GitHub Release 事件上下文；如果没有 `release` 事件，依赖 `github.event.release` 的上传动作可能无法确定目标 Release，因此手动触发主要提供入口，实际发布参数仍沿用现有 Release 事件模型。

## 执行

1. 在 `.github/workflows/release.yml` 的 `on` 下增加 `workflow_dispatch:`。
2. 保留 `release.types.published`，确保正式发布流程不受影响。
3. 复核 YAML 触发器及现有 job 依赖关系。
4. 不执行 Git 提交。
