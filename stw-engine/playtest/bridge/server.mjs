#!/usr/bin/env node

import {
  createPlaytestBridge,
  loadBridgeConfig,
} from "./bridge.mjs";

let bridge = null;
let shuttingDown = false;

async function shutdown(signal, exitCode = 0) {
  if (shuttingDown) return;
  shuttingDown = true;
  console.log(`STW playtest bridge received ${signal}; stopping owned runtime`);
  try {
    await bridge?.close();
  } catch (error) {
    exitCode = 1;
    console.error(`STW playtest bridge cleanup failed: ${error.message}`);
  }
  process.exitCode = exitCode;
}

async function main() {
  const config = loadBridgeConfig(process.env);
  bridge = createPlaytestBridge(config);
  const address = await bridge.listen();
  process.once("SIGINT", () => void shutdown("SIGINT"));
  process.once("SIGTERM", () => void shutdown("SIGTERM"));
  const host = typeof address === "object" && address ? address.address : config.host;
  const port = typeof address === "object" && address ? address.port : config.port;

  console.log(
    `STW playtest bridge listening on http://${host}:${port}${config.routePrefix}/`,
  );
  console.log(
    `Native scene=game stream=${config.streamFramesPerSecond}fps display=${config.display}`,
  );
  console.log("Fixed game controls enabled; technical debug remains token-protected");

  if (shuttingDown) return;
  await bridge.supervisor.connectGame();
  if (!shuttingDown) {
    console.log("Native STW game runtime started at its real menu");
  }
}

main().catch(async (error) => {
  console.error(`STW playtest bridge failed: ${error.message}`);
  await shutdown("startup failure", 1);
});
