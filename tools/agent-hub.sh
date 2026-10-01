#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
hub_dir="$repo_root/.agents"

usage() {
  printf '%s\n' \
    'Usage:' \
    '  tools/agent-hub.sh status' \
    '  tools/agent-hub.sh read [count]' \
    '  tools/agent-hub.sh post <codex|claude|grok> <kind> <message>' \
    '  tools/agent-hub.sh claim <codex|claude|grok> <summary>'
}

command="${1:-}"
case "$command" in
  status)
    sed -n '1,200p' "$hub_dir/current.md"
    printf '\nLatest messages:\n'
    find "$hub_dir/messages" -maxdepth 1 -type f -name '*.md' ! -name README.md -printf '%f\n' \
      | sort | tail -10
    ;;
  read)
    count="${2:-10}"
    find "$hub_dir/messages" -maxdepth 1 -type f -name '*.md' ! -name README.md -printf '%p\n' \
      | sort | tail -n "$count" | while IFS= read -r message_file; do
        printf '\n--- %s ---\n' "${message_file#"$hub_dir/"}"
        sed -n '1,200p' "$message_file"
      done
    ;;
  post)
    agent="${2:-}"
    kind="${3:-}"
    message="${4:-}"
    if [[ "$agent" != codex && "$agent" != claude && "$agent" != grok ]] || [[ -z "$kind" || -z "$message" ]]; then
      usage
      exit 2
    fi
    timestamp="$(date -u +%Y-%m-%dT%H%M%SZ)"
    message_file="$hub_dir/messages/${timestamp}-${agent}-${kind}.md"
    if [[ -e "$message_file" ]]; then
      message_file="$hub_dir/messages/${timestamp}-${agent}-${kind}-$$.md"
    fi
    printf '# %s — %s\n\n- **Kind:** %s\n- **Message:** %s\n- **Timestamp:** %s\n' \
      "$agent" "$kind" "$kind" "$message" "$timestamp" > "$message_file"
    printf 'Wrote %s\n' "${message_file#"$repo_root/"}"
    ;;
  claim)
    agent="${2:-}"
    summary="${3:-}"
    if [[ "$agent" != codex && "$agent" != claude && "$agent" != grok ]] || [[ -z "$summary" ]]; then
      usage
      exit 2
    fi
    mkdir -p "$hub_dir/claims"
    claim_file="$hub_dir/claims/$agent.md"
    printf '# %s claim\n\n- **Status:** active\n- **Summary:** %s\n- **Updated:** %s\n' \
      "$agent" "$summary" "$(date -u +%Y-%m-%dT%H%M%SZ)" > "$claim_file"
    printf 'Wrote %s\n' "${claim_file#"$repo_root/"}"
    ;;
  *)
    usage
    exit 2
    ;;
esac
