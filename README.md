<div align="center">
  <h1>DearOreUI</h1>
  <p><strong>Extend Minecraft Bedrock's OreUI at runtime.</strong></p>
  <p>A native LeviLamina client mod for reading, transforming, and injecting OreUI resources.</p>

  <p>
    <a href="https://github.com/copper-lamp/Dear-OreUI/releases">Releases</a>
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
    <a href="https://github.com/copper-lamp/Dear-OreUI/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/copper-lamp/Dear-OreUI/build.yml?branch=main&amp;style=for-the-badge&amp;label=build" alt="DearOreUI build status"></a>
    <a href="https://github.com/copper-lamp/Dear-OreUI/releases"><img src="https://img.shields.io/github/v/release/copper-lamp/Dear-OreUI?style=for-the-badge&amp;label=release" alt="DearOreUI latest release"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-CC0--1.0-2f6f9f?style=for-the-badge" alt="CC0-1.0 license"></a>
    <a href="https://github.com/copper-lamp/Dear-OreUI/issues"><img src="https://img.shields.io/github/issues/copper-lamp/Dear-OreUI?style=for-the-badge" alt="DearOreUI open issues"></a>
  </p>
</div>

> [!NOTE]
> DearOreUI has moved past the scaffold stage. The hooks, resource snapshot, multi-mod registry, transforms, injection, Host/Facet bridge, UI mounting, and API facades described below are implemented, and the display chain has been verified against a real client on OreUI-stack pages. It is still version 0.1.0 with no stable release published. JsonUI pages (main menu, in-game screens) are not covered, and each OreUI View currently allows one JS-driven native call. Those limits are listed under Compatibility.

## Overview

DearOreUI is a native Windows x64 client mod for LeviLamina 26.10.x. It reads the OreUI resources loaded by the original Minecraft client, combines changes declared by multiple mods, and injects the verified result back into the active OreUI page.

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

## What Works

- **Runtime hooks.** The mod hooks TechStack selection, SceneProvider scene creation, Router navigation, `OreUI::View::initialize`, `OnReadyForBindings`, `triggerEvent`, and `ClientInstance::update`.
- **Real display chain.** A `cohtml::View` is captured, scripts are gated on `OnReadyForBindings`, and `CoherentHostBridge::sendScript` executes them. The DOM overlay is built through CSSOM, not `innerHTML`. This chain was verified on the world list page (`/play/all`).
- **Resource snapshot.** `FileSystemSourceReader` reads the original `gui/dist/hbui` resources; `ResourceUri` and `ResourceIndex` manage paths and access.
- **Multi-mod registry.** Mods register resources, scripts, styles, UI, and transforms. The central registry sorts dependencies, detects conflicts, and builds one change plan per page.
- **Declarative UI.** `registerMod` / `registerOverlay` feed `UiPlanner`, `MountManager`, and `UiStateMachine`. A 52-component showcase renders end to end on the real client.
- **Host bridge.** `HostDispatcher` and `HostMethodRegistry` route JS requests to native methods with permission checks. JS to native runs over the game's native facet protocol: `DearOreUI.call` → `facet:request` → `OreUIFacetBridge` → `HostDispatcher` → `bus.push`.
- **Public API.** `IDearOreUIApi` merges Runtime, Resource, Mod, Host, UI, Page, Event, Transform, Diagnostic, Frame, and RuntimeReport facets. External mods obtain it through the pure C ABI bridge (`DearOreUI_QueryApi`).
- **JS namespace.** `window.__DearOreUI__` (protocol info, `bus`, `ipc`) and `window.DearOreUI` (`call`, `report`) are injected, plus base `oreui.*` namespaces.
- **Diagnostics.** JSONL event stream, per-stage telemetry, injection reports, and crash probes are written without touching the game installation.

## Development Status

| Capability | Status |
| --- | --- |
| LeviLamina mod lifecycle | Implemented |
| Windows x64 client build | Working via xmake + Clang-CL |
| OreUI / Coherent runtime hook | Implemented, client-verified |
| Original OreUI resource snapshot | Implemented |
| Multi-mod resource registry | Implemented |
| Dependency ordering and conflict detection | Implemented |
| Resource and code transformation | Implemented internally (ChangePlanner) |
| UI mounting and page lifecycle | Implemented, client-verified |
| C++ and JavaScript Host API | Basic API implemented, JS side injected |
| Crash isolation experiment | Implemented |

Not covered yet:

| Capability | Status |
| --- | --- |
| JsonUI pages (main menu, in-game screens) | Not supported |
| Multiple JS→C++ calls per View | Limited to one effective dispatch |
| Public event/page subscription facade | Not formed |
| Diagnostic query facade | Not formed |
| Versioned transform as public API | Not exposed |

The unit suite (`DearOreUIUnitTests`) covers the registry, transforms, IPC, UI, components, and diagnostics and exits 0 on the current build.

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
├── api/          Public facades and stable API types (IDearOreUIApi)
├── bridge/       Pure C ABI bridge for external mods
├── runtime/      Runtime state and subsystem coordination
├── hook/         OreUI / Coherent lifecycle integration
├── capability/   Version, page, and capability detection
├── page/         PageContext and page events
├── source/       Original OreUI resource snapshots
├── resource/     Resource index, URIs, and access control
├── registry/     Mod, resource, and change registrations
├── transform/    Dependency resolution, conflicts, change plans
├── render/       HTML DOM parsing and script serialization
├── component/    Vanilla assets, theme tokens, component renderer
├── inject/       Result validation and page injection
├── ipc/          C++ and JavaScript communication
├── facet/        Host capability adapters
├── ui/           UI mounting, planning, and state
├── diagnostic/   Logs, telemetry, reports, crash probes
└── poc/          Historical navigation proofs of concept (kept for reference)
```

The dependency order is documented in the module development plan, beginning with runtime facts and public contracts before higher-risk page and bundle operations.

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

OreUI bundle compatibility cannot be inferred from the Minecraft version alone. Runtime support depends on the detected OreUI resources, Coherent host, page type, resource fingerprint, and available capabilities.

Verified boundary, from the stage 7.1 and stage 8-A client records:

- The display chain works on OreUI-stack pages, currently the world list page.
- One JS→C++ roundtrip has been recorded against the real client.
- JsonUI pages (main menu, in-game screens) run on a different stack and are outside this pipeline.
- Other Minecraft / LeviLamina versions are unverified. "Unknown" must never be treated as "Supported".

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

A successful build validates compilation and packaging. It does not prove runtime OreUI injection. Follow the validation records under `../Docs/` and record target-client evidence before claiming page behavior.

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

Build artifacts are written to `bin/`.

## Testing

### Deterministic tests

Run `DearOreUIUnitTests` from the build output. The suite covers:

- Manifest, namespace, version, permission, and result validation
- Resource URI normalization and path safety
- Resource fingerprints and replacement conditions
- Registration, unregistration, dependency, and conflict handling
- Transform unique-match and fallback behavior
- IPC request, response, timeout, cancellation, and error serialization
- PageContext and UI state transitions
- Component rendering, vanilla assets, and theme tokens
- Diagnostic record correlation

The current build exits 0.

### Target-client validation

Already recorded (see `../Docs/`):

- Hook discovery and page lifecycle events
- Real `cohtml::View` capture and `OnReadyForBindings` gating
- C++→JS script execution and CSSOM overlay build
- One JS→C++ facet roundtrip with `bus.push` response
- A 52-component UI showcase mount and cleanup on the real client

Still open:

- JsonUI page injection (main menu, in-game screens)
- Multiple JS→C++ dispatches per View
- Uninstall and lifecycle cleanup regression
- Version matrix evidence beyond the recorded target

Runtime validation must record the Minecraft, LeviLamina, and DearOreUI versions, page type, resource fingerprint, hook state, diagnostic IDs, and reproducible evidence where applicable.

## Roadmap

| Milestone | Scope | Status |
| --- | --- | --- |
| M0 | Runtime facts and Hook feasibility | Done |
| M1 | Public types, Manifest, and diagnostics | Done |
| M2 | Page lifecycle and PageContext | Done |
| M3 | Resource snapshot and minimal injection | Done |
| M4 | C++ to JavaScript Host communication | Done |
| M5 | Multi-mod changes, dependencies, and conflicts | Done |
| M6 | UI mounting and page display | Done |
| M7 | Versioned transforms and Facet providers | Mostly done; transform not yet a public API |
| M8 | App, Web, and example-mod integration | In progress: external mod example and ABI done; App/Web pending |

See the [API status check (2026-08-22)](../Docs/DearOreUI-API状态核对-2026-08-22.md) and the [progress summary and stage 8 plan](../Docs/DearOreUI-当前进度总结与阶段8规划.md) for the current picture and remaining gates.

## Documentation

- [Complete API architecture](../Docs/DearOreUI-完整API架构设计.md)
- [Progressive API architecture](../Docs/DearOreUI-渐进式API架构设计.md)
- [Runtime Hook injection design](../Docs/方案A-运行时Hook注入-顶层设计.md)
- [Module dependency and development plan](../Docs/DearOreUI-模块依赖与开发计划.md)
- [API status check (2026-08-22)](../Docs/DearOreUI-API状态核对-2026-08-22.md)
- [Production-grade API completion](../Docs/DearOreUI-生产级API完整化-需求架构执行.md)
- [External mod API example and ABI contract](../Docs/DearOreUI-外部Mod-API示例与ABI契约.md)
- [External mod minimal connection validation](../Docs/DearOreUI-外部Mod最小连接验证-需求架构执行.md)
- [Progress summary and stage 8 plan](../Docs/DearOreUI-当前进度总结与阶段8规划.md)
- [Development index](../Docs/development-index.md)
- [OreUI Customizer reference analysis](../Docs/ore-ui-customizer/01-整体架构与应用通信.md)

## Reference Project

[`libs/Ore-UI-Customizer-App`](../libs/Ore-UI-Customizer-App) is a read-only reference project used to study OreUI resource structure, version differences, injection flows, Facet access, diagnostics, and preview behavior.

Its Electron permission model, compiled-bundle regular-expression strategy, and installation-directory modification workflow are not copied into DearOreUI as runtime contracts.

## Contributing

Before contributing, read the development and API documents listed above. Contributions should:

- Distinguish verified facts, design targets, and unresolved runtime questions.
- Keep runtime-sensitive changes behind the existing validation records.
- Include tests or target-client validation for runtime-sensitive changes.
- Update the relevant API, compatibility, diagnostic, and development documents when contracts change.
- Avoid committing secrets, personal paths, build caches, game logs, or private player data.
- Leave the read-only reference project under `libs/` unchanged.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the contribution workflow and [SECURITY.md](SECURITY.md) for private vulnerability reporting. 中文入口：[README_ZH.md](README_ZH.md)。

## License

DearOreUI is released under the [CC0-1.0](LICENSE) license.