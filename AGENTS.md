# Repository notes

<!-- quest-qualification-handoff -->
## Runtime qualification

Historical Workers implementation; compare with BabylonNative #2101, not with the XR renderer.

Start with the [shared runtime handoff](https://github.com/rebeckerspecialties/BabylonNative/blob/wgpu/Documentation/QuestRuntimeHandoff.md) for artifact pins and related PRs. This is a historical pre-monorepo branch. Keep fork instructions out of upstream ports. Use the monorepo PR for new runtime development; exercise the same inputs on each supported JS engine.

- Implementation: Keep production Android APK resolution separate from host filesystem roots and from a test-only resolver.
- Device: Load an APK Worker entry and its relative import without injection; ensure pending messages drain on shutdown.
