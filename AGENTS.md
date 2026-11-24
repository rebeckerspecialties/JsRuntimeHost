# Repository notes

<!-- quest-qualification-handoff -->
## Runtime qualification

The in-memory IndexedDB facility is not persistent browser storage. Factotum reports IndexedDB availability at startup.

Start with the [shared runtime handoff](https://github.com/rebeckerspecialties/BabylonNative/blob/wgpu/Documentation/QuestRuntimeHandoff.md) for artifact pins and related PRs. This is a historical pre-monorepo branch. Keep fork instructions out of upstream ports. Use the monorepo PR for new runtime development; exercise the same inputs on each supported JS engine.

- Implementation: Confirm which opening and navigation code paths require persistence; keep IndexedDB separate from Worker and N-API.
- Device: Cold start a fresh qualification package, navigate twice, then relaunch and compare application storage behavior with the browser.
