# Security Policy

## Supported Versions

DearOreUI is in early development. Security handling follows the latest published release and the active development branch. Runtime Hook, resource processing, JavaScript injection, Host API, and UI integration features may change while the architecture is being validated.

## Reporting a Vulnerability

Do not disclose suspected vulnerabilities in a public issue. Use GitHub's private **Security** reporting flow for this repository.

Include:

- The affected DearOreUI version or commit.
- Minecraft, LeviLamina, and Windows versions.
- The affected page, resource, API, or permission boundary.
- Reproduction steps or a minimal proof of concept.
- Expected impact and any known mitigation.
- Sanitized logs and diagnostic IDs where available.

Do not attach worlds, game installations, private server addresses, access tokens, player information, or unsanitized logs unless they are necessary and have been removed of sensitive data.

## Security Boundaries

DearOreUI is designed to enforce:

- Mod namespace isolation for resources.
- Normalized resource paths without arbitrary local file access.
- Explicit Host API registration and permission checks.
- Context-bound requests that become invalid after page destruction.
- Version and resource-fingerprint checks for advanced transforms.
- Safe fallback to the original page when a target is unsupported or validation fails.

Reports that bypass these boundaries are especially valuable. Do not publish a working exploit before the maintainers have had an opportunity to investigate.
