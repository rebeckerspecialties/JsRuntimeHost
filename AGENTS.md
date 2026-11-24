# Repository notes

<!-- quest-qualification-handoff -->
## Runtime qualification

Android addons and the embedding must see the same runtime and N-API shared library.

Start with the [shared runtime handoff](https://github.com/rebeckerspecialties/BabylonNative/blob/wgpu/Documentation/QuestRuntimeHandoff.md) for artifact pins and related PRs. This is a historical pre-monorepo branch. Keep fork instructions out of upstream ports. Use the monorepo PR for new runtime development; exercise the same inputs on each supported JS engine.

- Implementation: Check DT_NEEDED and packaged native library ownership in the final APK; do not link a second adapter.
- Device: Load a real Android addon on the Quest; test detachment and shutdown on the same adapter instance.
