# Releasing DearOreUI

## Version Sources

Before publishing, keep these values aligned:

- Git tag, using the `v<version>` format.
- `manifest.json` version.
- `tooth.json` version.
- `CHANGELOG.md` release heading.

## Release Workflow

The current release workflow is triggered when a GitHub Release is published. It builds Windows x64 artifacts, extracts release notes from `CHANGELOG.md`, and uploads packaged artifacts to the release.

Review the workflow before changing its matrix. The project support target is the client variant, while the template workflow may still expose both `server` and `client` configurations.

## Release Checklist

- Confirm the version in metadata, tag, and changelog.
- Confirm the changelog describes known limitations and compatibility.
- Run a clean client Release build.
- Run repository checks and `git diff --check`.
- Review the generated package contents.
- Confirm no secrets, local paths, caches, logs, or reference-project files are included.
- Verify the Tooth URL and destination layout.
- Mark unverified runtime features as experimental or unsupported.

## Runtime Claims

Do not describe a release as supporting OreUI Hook, resource injection, UI mounting, Host API, or Bundle transforms unless the target-client evidence exists for that release and compatibility line.
