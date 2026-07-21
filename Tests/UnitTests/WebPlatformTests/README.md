# Pinned Worker WPT subset

`UPSTREAM_REVISION` records the exact
[web-platform-tests/wpt](https://github.com/web-platform-tests/wpt) commit used
for this directory. `LICENSE.md`, `resources/testharness.js`, and the files
below are copied from that revision:

- `workers/interfaces/DedicatedWorkerGlobalScope/EventTarget.worker.js`
- `workers/interfaces/DedicatedWorkerGlobalScope/onmessage.worker.js`
- `workers/interfaces/DedicatedWorkerGlobalScope/postMessage/return-value.worker.js`
- `workers/interfaces/WorkerGlobalScope/self.any.js`
- `workers/interfaces/WorkerUtils/importScripts/001.worker.js`
- `workers/support/Worker-structure-message.js`
- `workers/constructors/Worker/terminate.js`

`self.worker.js` is a local worker-harness wrapper for `self.any.js`.
`runner.js` replaces WPT's browser document/server runner: it launches the
worker tests sequentially, consumes `testharness.js` completion records, and
adapts the document-side structured-clone, transfer, and termination checks to
the JsRuntimeHost unit-test host.
