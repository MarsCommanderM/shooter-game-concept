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
    `Native scene=${config.initialScene} stream=${config.streamFramesPerSecond}fps display=${config.display}`,
  );
  console.log("Remote control enabled with bearer-token authentication");

  if (shuttingDown) return;
  await bridge.supervisor.start(config.initialScene);
  if (!shuttingDown) {
    console.log(`Native STW playtest started: ${config.initialScene}`);
  }
}

main().catch(async (error) => {
  console.error(`STW playtest bridge failed: ${error.message}`);
  await shutdown("startup failure", 1);
});
