# Building DearOreUI

## Requirements

- Windows x64
- Git
- xmake
- Visual Studio or LLVM with Clang-CL
- LeviLamina 26.1 client development environment

The current supported development target is the Windows x64 client variant. The repository's `xmake.lua` still exposes a `server` configuration for template compatibility, but DearOreUI's project target is client-only.

## Configure and Build

Run these commands from the repository root:

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

## Output

Build output is written to `bin/`. Do not commit `bin/`, `.xmake/`, generated package files, or local logs.

## Troubleshooting

- Run `xmake repo -u` after changing dependency versions or when the package is not found.
- Verify that the active toolchain provides Clang-CL and targets Windows x64.
- Confirm that the LeviLamina 26.1 development package is available to xmake.
- Remove local generated build state only when a normal reconfigure cannot resolve stale configuration.
- A successful build validates compilation and packaging only; it does not validate OreUI Hook, resource access, injection, or UI behavior in Minecraft.

## Runtime Validation

Runtime-sensitive changes require a fixed Minecraft and LeviLamina client. Record the versions, target page, resource fingerprint, Hook state, diagnostic IDs, and reproducible evidence.
