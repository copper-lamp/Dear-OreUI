# Contributing to DearOreUI

Thank you for helping improve DearOreUI. Focused changes to the native runtime, compatibility research, documentation, build tooling, and test coverage are welcome.

## Before You Start

- Read [README.md](README.md), the [API architecture](../Docs/DearOreUI-完整API架构设计.md), and the [development plan](../Docs/DearOreUI-模块依赖与开发计划.md).
- Search existing issues before opening a new one.
- Keep changes focused and avoid unrelated refactors.
- Do not modify the read-only reference projects under `libs/`.
- Do not describe an unverified Hook, resource boundary, or UI mechanism as implemented.

## Project Constraints

DearOreUI targets the Windows x64 LeviLamina 26.1 client. The current repository is a runtime scaffold; a successful build does not prove OreUI Hook or injection support.

Development must follow this order:

```text
Runtime facts
    ↓
Public types and diagnostics
    ↓
Manifest and API facades
    ↓
Hook, capability detection, and PageContext
    ↓
Resource snapshot and minimal injection
    ↓
Host bridge and Facet adapters
    ↓
Multi-mod registry and transforms
    ↓
UI mounting
    ↓
Advanced compatibility patches
```

## Build

Run commands from the `DearOreUI/` directory.

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

For a debug build:

```powershell
xmake f -a x64 -m debug -p windows --target_type=client -y
xmake -v -y
```

Build artifacts are written to `bin/`. The supported development target is the Windows x64 client variant.

## Format and Local Checks

Format changed C++ files with the repository configuration:

```powershell
clang-format -i <changed-cpp-or-header-files>
```

Before opening a pull request, run:

```powershell
xmake -r -y
git diff --check
```

Runtime-sensitive changes must also be tested in a fixed Minecraft and LeviLamina client. A successful compilation alone is not sufficient for Hook, resource, Host API, or UI changes.

## Runtime Validation

For changes affecting `hook`, `source`, `resource`, `inject`, `ipc`, `facet`, or `ui`, record:

- Minecraft and LeviLamina versions.
- DearOreUI version or commit.
- Windows version and architecture.
- Page type and entry resource.
- Resource fingerprint where applicable.
- Hook state and capability report.
- Diagnostic IDs and relevant logs.
- Page creation, reload, navigation, and destruction behavior.

Do not attach private server addresses, player data, access tokens, or unsanitized logs.

## API and Compatibility Changes

API, resource URI, Manifest, permission, PageContext, Host API, event, transform, or diagnostic changes must update the relevant documents under `../Docs/`.

Changes to a versioned transform must include:

- The target Minecraft and LeviLamina versions.
- The target resource fingerprint.
- The matching and validation conditions.
- The expected fallback when the target is not found.
- A test proving that zero and multiple matches are rejected.

## Pull Requests

Include:

- A concise description of the problem and solution.
- The modules and contracts affected.
- The validation performed, including runtime checks where applicable.
- Compatibility and migration impact.
- Documentation changes for user-visible or architectural behavior.
- License notices for newly introduced third-party code or assets.

Keep the pull request focused. If a change is exploratory and cannot yet be validated against the target client, label it as research or experimental and state the unresolved facts explicitly.

## Security and Privacy

Do not disclose suspected vulnerabilities in a public issue. Follow [SECURITY.md](SECURITY.md) for private reporting.

Never commit secrets, local absolute paths, build caches, game installations, private logs, worlds, replay files, or player information.
