// PM2-Config für den WIRRWARR Online-Server
// Usage: pm2 start deploy/ecosystem.config.cjs
module.exports = {
  apps: [
    {
      name: "wirrwarr",
      script: "server.mjs",
      cwd: __dirname + "/..",
      instances: 1,          // EIN Prozess = eine Arena-World (State lebt im Prozess)
      exec_mode: "fork",
      autorestart: true,
      max_memory_restart: "400M",
      env: {
        NODE_ENV: "production",
      },
    },
  ],
};
