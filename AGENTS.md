# Repository notes

<!-- quest-qualification-handoff -->
## Runtime qualification

Use the maintained BabylonNative conformance port (#2097) for new cases; this is a historical CI twin.

Start with the [shared runtime handoff](https://github.com/rebeckerspecialties/BabylonNative/blob/wgpu/Documentation/QuestRuntimeHandoff.md) for artifact pins and related PRs. This is a historical pre-monorepo branch. Keep fork instructions out of upstream ports. Use the monorepo PR for new runtime development; exercise the same inputs on each supported JS engine.

- Implementation: Keep relevant regressions and explicit per-engine skips; do not duplicate the maintained harness.
- Device: Run the detach and typed-array subsets on Quest V8, then run pointer-pinning on physical system JSC.
