# STW Lightning T4 runner

This directory is the controlled GitHub-to-Lightning execution boundary for
the canonical STW production repository.

## Security boundary

- The workflow has no pull-request trigger.
- It only runs for pushes to `brauny/stw-game-production` that modify the
  runner workflow, bootstrap, or fixed task.
- The job requires the dedicated `stw-lightning-t4` runner label.
- GitHub permissions are read-only and checkout credentials are not persisted.
- The task rejects any other repository or branch before doing work.
- No browser request, workflow input, or arbitrary command is passed to Bash.
- Runner registration tokens are requested at runtime and never committed.

The repository is public, so adding a pull-request trigger to this self-hosted
workflow would be unsafe. Keep external contributions away from this runner.

## Studio commands

From the repository checkout in Lightning AI:

```bash
git fetch
git checkout origin/brauny/stw-game-production
bash runner.sh
```

Operational commands:

```bash
bash runner.sh status
bash runner.sh stop
bash runner.sh remove
```

`stop` and `remove` signal only the PID recorded by this bootstrap after
verifying that it belongs to the runner directory. They never use `pkill`.

## Lifecycle

The runner is installed outside the checkout at `~/.stw-github-runner`. It
stays available while the Lightning Studio machine is running. If Lightning
suspends or terminates the machine, GitHub will show the runner as offline;
start the Studio and run `bash runner.sh` again.

Future controlled work is committed to `task.sh`. Do not accept arbitrary
workflow text or shell commands from web endpoints.
