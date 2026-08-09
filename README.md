<div align="center">
  <h1>DearOreUI</h1>
  <p><strong>Extend Minecraft Bedrock's OreUI at runtime.</strong></p>
  <p>A native LeviLamina client mod for reading, transforming, and injecting OreUI resources.</p>

  <p>
    <a href="https://github.com/DearOreUI/DearOreUI/releases">Releases</a>
    ·
    <a href="CHANGELOG.md">Changelog</a>
    ·
    <a href="../Docs/DearOreUI-完整API架构设计.md">API architecture</a>
    ·
    <a href="../Docs/DearOreUI-模块依赖与开发计划.md">Development plan</a>
    ·
    <a href="CONTRIBUTING.md">Contributing</a>
    ·
    <a href="README_ZH.md">简体中文</a>
  </p>

  <p>
    <a href="https://github.com/DearOreUI/DearOreUI/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/DearOreUI/DearOreUI/build.yml?branch=main&amp;style=for-the-badge&amp;label=build" alt="DearOreUI build status"></a>
    <a href="https://github.com/DearOreUI/DearOreUI/releases"><img src="https://img.shields.io/github/v/release/DearOreUI/DearOreUI?style=for-the-badge&amp;label=release" alt="DearOreUI latest release"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-CC0--1.0-2f6f9f?style=for-the-badge" alt="CC0-1.0 license"></a>
    <a href="https://github.com/DearOreUI/DearOreUI/issues"><img src="https://img.shields.io/github/issues/DearOreUI/DearOreUI?style=for-the-badge" alt="DearOreUI open issues"></a>
  </p>
</div>

> [!WARNING]
> DearOreUI is in an early development stage. The current repository provides the LeviLamina mod scaffold and build configuration; OreUI hooks, resource interception, runtime injection, UI mounting, and public APIs are not yet stable features.

## Overview

DearOreUI is a native Windows x64 client mod for LeviLamina 26.10.x. It is designed to read the OreUI resources loaded by the original Minecraft client, combine changes declared by multiple mods, and inject the verified result back into the active OreUI page.

```text
Original OreUI
    ↓
Runtime hook and page discovery
    ↓
Original resource snapshot
    ↓
Mod registrations
    ↓
Dependency and conflict resolution
    ↓
Resource and code transformation
    ↓
Validated injection result
    ↓
OreUI page, mod UI, and Host API
```

DearOreUI is not a replacement UI and does not directly expose page pointers, compiled bundle internals, or arbitrary local file access to other mods.

## Features

The following capabilities are part of the project direction. Their implementation status is listed in the compatibility table below.

- **Runtime OreUI integration** — Discover OreUI and Coherent page lifecycle events from the Minecraft client.
- **Resource interception** — Read HTML, CSS, JavaScript, and binary resources from the active OreUI runtime.
- **Multi-mod extensions** — Collect resource, script, style, UI, and transform declarations from multiple mods.
- **Conflict-aware transformation** — Validate versions, fingerprints, dependencies, conflicts, and unique matches before changing a target.
- **Runtime injection** — Submit a verified result to the active OreUI page without modifying the original game installation.
- **Progressive APIs** — Start with runtime queries and resource registration, then opt into page events, UI mounting, Host APIs, and advanced transforms.
- **Host bridge** — Provide explicitly registered and permission-checked communication between page JavaScript and native code.
- **Diagnostics** — Associate registration, transformation, injection, runtime, and cleanup failures with structured diagnostic records.

## Development Status

| Capability | Status |
| --- | --- |
| LeviLamina mod lifecycle | Available in the scaffold |
| Windows x64 client build | Configured |
| OreUI / Coherent runtime hook | Planned; target-version validation required |
| Original OreUI resource snapshot | Planned; target-version validation required |
| Multi-mod resource registry | Planned |
| C++ and JavaScript Host API | Planned |
| UI mounting and page lifecycle | Planned; target-version validation required |
| Versioned code transformation | Planned |

The current repository must not be treated as a complete runtime implementation. A successful build only verifies the current mod scaffold and build configuration.

## Progressive API Model

```text
L0  Runtime queries
    ↓
L1  Resource, script, and stylesheet registration
    ↓
L2  Page lifecycle and PageContext
    ↓
L3  UI mounting and page extensions
    ↓
L4  Permission-checked Host API
    ↓
L5  Versioned code and resource transforms
    ↓
L6  Facet providers and advanced compatibility adapters
```

Most mods should remain at L1. Higher levels are opt-in because they require more knowledge of page lifecycle, host capabilities, compatibility constraints, or original bundle structure.

## Multi-Mod Model

Mods do not directly mutate the same intermediate string or write to the game installation. They register declarations with DearOreUI, which creates one page-scoped change plan from the original resource snapshot.

```text
Mod A registration
Mod B registration
Mod C registration
    ↓
Central registry
    ↓
Page, version, and capability filtering
    ↓
Dependency ordering
    ↓
Conflict detection
    ↓
Resource and code transformation
    ↓
Integrity validation
    ↓
One injection submission
```

The default rules are:

- Resource paths are isolated by mod namespace.
- Identical registrations are idempotent.
- Different contents targeting the same owned resource produce a conflict.
- Replacements require an expected original fingerprint.
- Multiple replacements of the same original code region conflict by default.
- A failed mod change is isolated from unrelated changes.
- An unsupported version preserves the original page.
- Every conflict, skip, failure, and fallback produces a report.

## Runtime Architecture

```text
src/
├── mod/          Mod entry point, lifecycle, and configuration
├── api/          Public facades and stable API types
├── runtime/      Runtime state and subsystem coordination
├── hook/         OreUI / Coherent lifecycle integration
├── capability/   Version, page, and capability detection
├── page/         PageContext and page events
├── source/       Original OreUI resource snapshots
├── registry/     Mod, resource, and change registrations
├── transform/    Resource and code transformations
├── resource/     Resource indexes, URIs, and access control
├── inject/       Result validation and page injection
├── ipc/          C++ and JavaScript communication
├── facet/        Host capability adapters
├── ui/           UI mounting and display abstractions
└── diagnostic/   Logs, errors, and execution reports
```

The current implementation is centered on the `mod/` scaffold. The remaining modules are introduced in dependency order, beginning with runtime facts and public contracts before higher-risk page and bundle operations.

## Compatibility

The current target is a client-only LeviLamina mod for Windows x64:

| Component | Target |
| --- | --- |
| Minecraft Bedrock | Client on Windows x64 |
| LeviLamina | 26.10.x |
| Native entry | `DearOreUI.dll` |
| C++ standard | C++20 |
| Toolchain | Clang-CL |
| Build system | xmake |
| Mod version | `0.1.0` |

OreUI bundle compatibility cannot be inferred from the Minecraft version alone. Runtime support will also depend on the detected OreUI resources, Coherent host, page type, resource fingerprint, and available capabilities.

## Quick Start

DearOreUI is not yet distributed as a stable runtime release. For development builds, prepare a Windows x64 LeviLamina 26.10.x client environment and build from the `DearOreUI/` directory.

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

The generated files are placed under `bin/`. The package metadata is defined in:

- [manifest.json](manifest.json)
- [tooth.json](tooth.json)
- [xmake.lua](xmake.lua)

Do not interpret a successful build as proof that runtime OreUI injection is available. Follow the target-version validation plan before testing hooks or page modifications.

## Build From Source

### Requirements

- Windows x64
- Git
- xmake
- Visual Studio or an LLVM installation providing Clang-CL
- A LeviLamina 26.10.x development environment

### Release build

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

### Debug build

```powershell
xmake f -a x64 -m debug -p windows --target_type=client -y
xmake -v -y
```

### Output

Build artifacts are written to:

```text
bin/
```

## Testing

The development plan separates deterministic tests from target-client validation.

### Deterministic tests

- Manifest, namespace, version, permission, and result validation
- Resource URI normalization and path safety
- Resource fingerprints and replacement conditions
- Registration, unregistration, dependency, and conflict handling
- Transform unique-match and fallback behavior
- IPC request, response, timeout, cancellation, and error serialization
- PageContext and UI state transitions
- Diagnostic record correlation

### Target-client validation

- Hook discovery and page lifecycle events
- Original HTML, CSS, JavaScript, and binary resource access
- Minimal side-effect-free JavaScript injection
- Page reload, navigation, parallel pages, and destruction
- C++ to JavaScript and JavaScript to C++ communication
- A minimal UI mount and cleanup cycle
- Multiple mods contributing to the same page
- Safe disable behavior on unsupported versions

Runtime validation must record the Minecraft, LeviLamina, and DearOreUI versions, page type, resource fingerprint, hook state, diagnostic IDs, and reproducible evidence where applicable.

## Roadmap

| Milestone | Scope |
| --- | --- |
| M0 | Runtime facts and Hook feasibility |
| M1 | Public types, Manifest, and diagnostics |
| M2 | Page lifecycle and PageContext |
| M3 | Resource snapshot and minimal injection |
| M4 | C++ to JavaScript Host communication |
| M5 | Multi-mod changes, dependencies, and conflicts |
| M6 | UI mounting and page display |
| M7 | Versioned transforms and Facet providers |
| M8 | App, Web, and example-mod integration |

See the [module dependency and development plan](../Docs/DearOreUI-模块依赖与开发计划.md) for dependencies, test plans, milestone gates, and known risks.

## Documentation

- [Complete API architecture](../Docs/DearOreUI-完整API架构设计.md)
- [Progressive API architecture](../Docs/DearOreUI-渐进式API架构设计.md)
- [Runtime Hook injection design](../Docs/方案A-运行时Hook注入-顶层设计.md)
- [Module dependency and development plan](../Docs/DearOreUI-模块依赖与开发计划.md)
- [Development index](../Docs/development-index.md)
- [OreUI Customizer reference analysis](../Docs/ore-ui-customizer/01-整体架构与应用通信.md)

## Reference Project

[`libs/Ore-UI-Customizer-App`](../libs/Ore-UI-Customizer-App) is a read-only reference project used to study OreUI resource structure, version differences, injection flows, Facet access, diagnostics, and preview behavior.

Its Electron permission model, compiled-bundle regular-expression strategy, and installation-directory modification workflow are not copied into DearOreUI as runtime contracts.

## Contributing

Before contributing, read the development and API documents listed above. Contributions should:

- Distinguish verified facts, design targets, and unresolved runtime questions.
- Avoid high-risk bundle transforms until the target Hook and resource boundaries are verified.
- Include tests or target-client validation for runtime-sensitive changes.
- Update the relevant API, compatibility, diagnostic, and development documents when contracts change.
- Avoid committing secrets, personal paths, build caches, game logs, or private player data.
- Leave the read-only reference project under `libs/` unchanged.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the contribution workflow and [SECURITY.md](SECURITY.md) for private vulnerability reporting. 中文入口：[README_ZH.md](README_ZH.md)。

## License

DearOreUI is released under the [CC0-1.0](LICENSE) license.
