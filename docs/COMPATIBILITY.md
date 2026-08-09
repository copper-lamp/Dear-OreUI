# Compatibility

## Current Target

| Component | Target | Status |
| --- | --- | --- |
| Platform | Windows x64 | Targeted |
| Minecraft | Bedrock client | Targeted |
| LeviLamina | 26.10.x | Targeted |
| Build target | Native client mod | Targeted |
| OreUI runtime Hook | Target-version evidence required | Not implemented |
| UI mounting | Target-version evidence required | Not implemented |

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

The current repository has not yet established the real OreUI or Coherent Hook point, resource interception boundary, JavaScript execution entry, or UI mounting mechanism. These are validation tasks, not current compatibility guarantees.
