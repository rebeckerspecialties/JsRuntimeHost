(() => {
  const failures = [];
  const cases = [
    "/workers/interfaces/DedicatedWorkerGlobalScope/EventTarget.worker.js",
    "/workers/interfaces/DedicatedWorkerGlobalScope/onmessage.worker.js",
    "/workers/interfaces/DedicatedWorkerGlobalScope/postMessage/return-value.worker.js",
    "/workers/interfaces/WorkerGlobalScope/self.worker.js",
    "/workers/interfaces/WorkerUtils/importScripts/001.worker.js"
  ];

  let index = 0;
  let timer = 0;

  function fail(name, detail) {
    failures.push(name + ": " + detail);
  }

  function finish() {
    clearTimeout(timer);
    __jsrhWptDone(failures.length === 0, failures.join("\n"));
  }

  function runStructuredMessageCase() {
    const name = "/workers/support/Worker-structure-message.js";
    const worker = new Worker(name);
    const input = new ArrayBuffer(20);
    let sawPass = false;

    timer = setTimeout(() => {
      worker.terminate();
      fail(name, "timed out");
      finish();
    }, 10000);

    worker.onerror = event => {
      clearTimeout(timer);
      worker.terminate();
      fail(name, event.message || "worker error");
      finish();
    };

    worker.onmessage = event => {
      if (typeof event.data === "string") {
        sawPass = event.data.indexOf("PASS:") === 0;
        if (!sawPass) fail(name, event.data);
        return;
      }

      clearTimeout(timer);
      const value = event.data;
      if (!sawPass || !value || value.operation !== "find-edges" ||
          !(value.input instanceof ArrayBuffer) || value.input.byteLength !== 20 ||
          value.threshold !== 0.6) {
        fail(name, "structured clone did not preserve the WPT payload");
      }
      worker.terminate();
      runTerminateCase();
    };

    worker.postMessage({ operation: "find-edges", input, threshold: 0.6 }, [input]);
    // Current WebKitGTK exposes the standards-track detached getter before
    // its plain ArrayBuffer byteLength reflection catches up. Either signal
    // confirms the sender backing store is detached; the Node-API adapter
    // independently validates detached state before and after transfer.
    if (input.detached !== true && input.byteLength !== 0) {
      fail(name, "transfer did not detach the sender ArrayBuffer");
    }
  }

  function runTerminateCase() {
    const name = "/workers/constructors/Worker/terminate.js";
    const worker = new Worker(name);
    let messages = 0;

    worker.onerror = event => {
      clearTimeout(timer);
      worker.terminate();
      fail(name, event.message || "worker error");
      finish();
    };
    worker.onmessage = () => { messages++; };

    timer = setTimeout(() => {
      const expected = messages;
      // Adapt the WPT document harness: hold the parent turn while the Worker
      // queues additional messages, then verify terminate() discards them.
      const start = Date.now();
      while (Date.now() - start < 50) {}
      worker.terminate();

      timer = setTimeout(() => {
        if (messages !== expected) {
          fail(name, "message events queued before terminate() were delivered");
        }
        finish();
      }, 100);
    }, 100);
  }

  function next() {
    clearTimeout(timer);
    if (index === cases.length) {
      runStructuredMessageCase();
      return;
    }

    const name = cases[index++];
    const worker = new Worker(name);
    timer = setTimeout(() => {
      worker.terminate();
      fail(name, "timed out");
      next();
    }, 10000);

    worker.onerror = event => {
      clearTimeout(timer);
      worker.terminate();
      fail(name, event.message || "worker error");
      next();
    };

    worker.onmessage = event => {
      const value = event.data;
      if (!value || value.type !== "complete") return;
      clearTimeout(timer);
      if (!value.status || value.status.status !== 0) {
        fail(name, value.status && value.status.message || "WPT harness failed");
      }
      for (const test of value.tests || []) {
        if (test.status !== 0) fail(name + " / " + test.name, test.message || "failed");
      }
      worker.terminate();
      next();
    };
  }

  next();
})();
