# Windows 构建中 zlib/libcurl 依赖修复方案

## 需求

修复 Windows 构建工作流在 xmake 安装 libcurl 时因找不到 `zlib.lib` 而失败的问题。目标是让 runner 预先具备可链接的 x64 zlib 开发库，并让 xmake/libcurl 使用一致的静态依赖配置；同时避免复用之前失败产生的不完整 xmake 包缓存。

## 架构

依赖链为：`DearOreUI`/`DearOreUIUnitTests` → `libcurl` → `zlib`。工作流在 xmake 配置前通过 vcpkg 安装 `zlib:x64-windows`，并通过 `VCPKG_ROOT`、`VCPKG_DEFAULT_TRIPLET` 和 `PATH` 暴露工具链环境。xmake 配置层继续显式声明 zlib/libcurl 为静态依赖并加入实际目标，使依赖顺序和链接类型保持一致。缓存键递增，确保旧的部分安装不会被恢复。

## 执行

1. checkout 后安装 vcpkg，并执行 `vcpkg install zlib:x64-windows`。
2. 使用 `GITHUB_ENV` 持久化 vcpkg 环境变量；不使用仅对当前进程有效的 `setx`。
3. 保留 xmake 的静态 `zlib`/`libcurl` 声明和目标包绑定。
4. 更新 xmake 缓存版本，执行仓库更新、配置和构建。
5. 不执行 Git 提交，由提交者审阅后自行提交和推送。
