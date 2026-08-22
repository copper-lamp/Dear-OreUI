# Compatibility

## Current Target

| Component | Target | Status |
| --- | --- | --- |
| Platform | Windows x64 | Targeted |
| Minecraft | Bedrock client | Targeted |
| LeviLamina | 26.10.x | Targeted |
| Build target | Native client mod | Targeted |
| OreUI runtime Hook | Target-version evidence required | Implemented; real-client verified on OreUI-stack pages |
| UI mounting | Target-version evidence required | Implemented; real-client verified on OreUI-stack pages |
| JS→C++ Host call | Target-version evidence required | Implemented; one roundtrip verified per View |
| JsonUI pages (main menu, in-game) | Separate tech stack | Not supported |
| Other Minecraft / LeviLamina versions | Not tested | Unknown |

## Compatibility Is Multi-Dimensional

OreUI compatibility cannot be inferred from the Minecraft version alone. A validation record must include:

- Minecraft version.
- LeviLamina version.
- DearOreUI version or commit.
- Windows version and architecture.
- Detected OreUI resource layout.
- Entry resource and page type.
- OreUI resource fingerprint.
- Coherent host information where available.
- Hook state and capability report.
- Diagnostic IDs and evidence.

## Support States

```text
Supported
Experimental
Unknown
Unsupported
```

`Unknown` must never be treated as `Supported`. A versioned transform is eligible only when its page, resource, version, fingerprint, matcher, and validation conditions all pass.

## Validation Record

Use this structure when reporting a compatibility result:

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

## Known Boundary

Validated compatibility today comes from the stage 7.1 and stage 8-A client records, on the recorded target versions:

- The display chain (capture `cohtml::View` → `OnReadyForBindings` gate → `ExecuteScript` → CSSOM overlay) works on OreUI-stack pages, currently the world list page (`/play/all`).
- One JS→C++ facet roundtrip has been recorded against the real client.
- JsonUI pages (main menu `start_screen`, in-game `hud_screen` / `in_game_play_screen`) run on a different stack and are outside this pipeline.
- Other Minecraft or LeviLamina versions are untested; their status is `Unknown`, which must never be treated as `Supported`.

A validation record must capture the environment and evidence for any claim beyond these boundaries.
