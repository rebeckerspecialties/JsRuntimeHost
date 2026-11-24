# Repository notes

<!-- quest-qualification-handoff -->
## Runtime qualification

QuickJS exception state affects Worker failure reporting independently of message delivery.

Start with the [shared runtime handoff](https://github.com/rebeckerspecialties/BabylonNative/blob/wgpu/Documentation/QuestRuntimeHandoff.md) for artifact pins and related PRs. This is a historical pre-monorepo branch. Keep fork instructions out of upstream ports. Use the monorepo PR for new runtime development; exercise the same inputs on each supported JS engine.

- Implementation: Compare pending-exception status, error message and stack preservation with the monorepo adapter before porting.
- Device: Run the worker failed-getter and missing-script fixtures on QuickJS and verify the parent stays usable.
