# Repository notes

<!-- quest-qualification-handoff -->
## Runtime qualification

Historical v7 port. The maintained monorepo adapter is upstream BabylonNative #2100; do not reapply this tree wholesale.

Start with the [shared runtime handoff](https://github.com/rebeckerspecialties/BabylonNative/blob/wgpu/Documentation/QuestRuntimeHandoff.md) for artifact pins and related PRs. This is a historical pre-monorepo branch. Keep fork instructions out of upstream ports. Use the monorepo PR for new runtime development; exercise the same inputs on each supported JS engine.

- Implementation: Diff detach and pinned-pointer behavior against #2100 rather than rebasing an archived runtime.
- Device: Run the Quest Node Lite typed-array fixture and run the pinned-byte-pointer case separately on iOS system JSC.
