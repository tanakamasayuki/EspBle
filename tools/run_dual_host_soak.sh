#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
tests_dir="$repository_dir/tests"

runs=${ESPBLE_DUAL_SOAK_RUNS:-20}
contention_cycles=${ESPBLE_DUAL_SOAK_CONTENTION_CYCLES:-100}
restart_cycles=${ESPBLE_DUAL_SOAK_RESTART_CYCLES:-100}
gatt_repeats=${ESPBLE_DUAL_SOAK_GATT_READ_REPEATS:-0}
clean_first=${ESPBLE_DUAL_SOAK_CLEAN_FIRST:-1}
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
log_dir=${ESPBLE_DUAL_SOAK_LOG_DIR:-"$tests_dir/.soak/dual-host-$timestamp"}

for value_name in runs contention_cycles restart_cycles gatt_repeats; do
  value=${!value_name}
  if [[ ! $value =~ ^[0-9]+$ ]]; then
    echo "$value_name must be a non-negative integer; found $value" >&2
    exit 2
  fi
done
if ((runs < 1 || contention_cycles < 1 || contention_cycles > 100 ||
     restart_cycles < 1 || restart_cycles > 100 || gatt_repeats > 1000)); then
  echo "invalid soak bounds: runs=$runs contention=$contention_cycles restart=$restart_cycles gatt=$gatt_repeats" >&2
  exit 2
fi
if [[ $clean_first != 0 && $clean_first != 1 ]]; then
  echo "ESPBLE_DUAL_SOAK_CLEAN_FIRST must be 0 or 1" >&2
  exit 2
fi
if [[ ! -f $tests_dir/.env ]]; then
  echo "missing $tests_dir/.env" >&2
  exit 2
fi

mkdir -p "$log_dir"
summary="$log_dir/summary.log"
{
  echo "dual-host soak start: $(date --iso-8601=seconds)"
  echo "repository: $repository_dir"
  echo "revision: $(git -C "$repository_dir" rev-parse HEAD)"
  echo "dirty files: $(git -C "$repository_dir" status --short | wc -l)"
  echo "runs=$runs contention_cycles=$contention_cycles restart_cycles=$restart_cycles gatt_repeats=$gatt_repeats clean_first=$clean_first"
} | tee "$summary"

for ((run = 1; run <= runs; ++run)); do
  run_log=$(printf '%s/run-%03d.log' "$log_dir" "$run")
  clean_args=()
  if ((run == 1 && clean_first == 1)); then
    clean_args+=(--clean)
  fi
  echo "run $run/$runs start: $(date --iso-8601=seconds)" | tee -a "$summary"
  (
    cd "$tests_dir"
    ESPBLE_DUAL_CONTENTION_CYCLES="$contention_cycles" \
    ESPBLE_DUAL_RESTART_CYCLES="$restart_cycles" \
    ESPBLE_DUAL_GATT_READ_REPEATS="$gatt_repeats" \
      uv run --env-file .env pytest -s "${clean_args[@]}" \
        peer/dual_host_smoke/ \
        --profile esp32_peer_host \
        --peer-profile device:esp32_peer_device
  ) 2>&1 | tee "$run_log"
  echo "run $run/$runs pass: $(date --iso-8601=seconds)" | tee -a "$summary"
done

echo "dual-host soak pass: $(date --iso-8601=seconds)" | tee -a "$summary"
echo "logs: $log_dir"
