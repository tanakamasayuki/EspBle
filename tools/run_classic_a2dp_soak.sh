#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
tests_dir="$repository_dir/tests"

packet_target=${ESPBLE_A2DP_SOAK_PACKETS:-20000}
clean=${ESPBLE_A2DP_SOAK_CLEAN:-1}
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
log_dir=${ESPBLE_A2DP_SOAK_LOG_DIR:-"$tests_dir/.soak/classic-a2dp-$timestamp"}

if [[ ! $packet_target =~ ^[0-9]+$ ]] ||
   ((packet_target < 100 || packet_target > 500000)); then
  echo "ESPBLE_A2DP_SOAK_PACKETS must be an integer from 100 to 500000" >&2
  exit 2
fi
if [[ $clean != 0 && $clean != 1 ]]; then
  echo "ESPBLE_A2DP_SOAK_CLEAN must be 0 or 1" >&2
  exit 2
fi
if [[ ! -f $tests_dir/.env ]]; then
  echo "missing $tests_dir/.env" >&2
  exit 2
fi

mkdir -p "$log_dir"
log_file="$log_dir/run.log"
{
  echo "Classic-only A2DP soak start: $(date --iso-8601=seconds)"
  echo "repository: $repository_dir"
  echo "revision: $(git -C "$repository_dir" rev-parse HEAD)"
  echo "dirty files: $(git -C "$repository_dir" status --short | wc -l)"
  echo "packet_target=$packet_target clean=$clean"
} | tee "$log_file"

clean_args=()
if ((clean == 1)); then
  clean_args+=(--clean)
fi

(
  cd "$tests_dir"
  ESPBLE_A2DP_PACKET_TARGET="$packet_target" \
    uv run --env-file .env pytest -s "${clean_args[@]}" \
      peer/classic_a2dp_media/ \
      --profile esp32_peer_host \
      --peer-profile device:esp32_peer_device
) 2>&1 | tee -a "$log_file"

echo "Classic-only A2DP soak pass: $(date --iso-8601=seconds)" | tee -a "$log_file"
echo "log: $log_file"
