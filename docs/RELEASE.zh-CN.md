# 发布 DearOreUI

## 版本来源

发布前必须保持以下版本信息一致：

- Git 标签，使用 `v<version>` 格式。
- `manifest.json` 中的版本。
- `tooth.json` 中的版本。
- `CHANGELOG.md` 中的版本标题。

## 发布工作流

当前发布工作流在 GitHub Release 发布后触发，负责构建 Windows x64 产物、从 `CHANGELOG.md` 提取发布说明，并将打包产物上传到 Release。

修改构建矩阵前必须先审查工作流。项目正式支持目标是客户端变体，而模板工作流可能仍暴露 `server` 和 `client` 两种配置。

## 发布检查清单

- 确认元数据、标签和 CHANGELOG 版本一致。
- 确认 CHANGELOG 写明已知限制和兼容性。
- 执行干净的客户端 Release 构建。
- 执行仓库检查和 `git diff --check`。
- 检查生成的发布包内容。
- 确认没有打包密钥、个人路径、缓存、日志或参考项目文件。
- 确认 Tooth 下载地址和安装目录布局正确。
- 将未验证的运行时能力标记为实验性或不支持。

## 运行时能力声明

除非当前发布版本和兼容性分支已经有目标客户端验证证据，否则不要宣称支持 OreUI Hook、资源注入、UI 挂载、Host API 或 Bundle 变换。
