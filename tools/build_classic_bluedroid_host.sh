#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir="$script_dir/classic_bluedroid_host"
repository_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
build_dir=${ESPBLE_CLASSIC_HOST_BUILD_DIR:-"$project_dir/build"}
output=${1:-"$repository_dir/src/esp32/libespble_bluedroid_classic.a"}

if [[ -z ${IDF_PATH:-} ]]; then
  echo "IDF_PATH is not set; source ESP-IDF v5.5.5 export.sh first" >&2
  exit 2
fi

idf_version=$(git -C "$IDF_PATH" describe --tags --always --dirty)
if [[ $idf_version != v5.5.5 ]]; then
  echo "ESP-IDF v5.5.5 is required; found $idf_version" >&2
  exit 2
fi

for tool in idf.py xtensa-esp32-elf-nm xtensa-esp32-elf-objcopy; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 2
  }
done

idf.py -C "$project_dir" -B "$build_dir" \
  -DIDF_TARGET=esp32 \
  -DSDKCONFIG="$build_dir/sdkconfig" \
  build

archive="$build_dir/esp-idf/bt/libbt.a"
symbols="$build_dir/espble-classic-host-redefine.syms"
temporary="$build_dir/libespble_bluedroid_classic.a"
defined_symbols="$build_dir/espble-classic-host-defined.txt"

xtensa-esp32-elf-nm -g --defined-only "$archive" 2>/dev/null |
  awk 'NF >= 3 {print $3}' |
  sort -u |
  awk '{print $1 " espble_bd_" $1}' > "$symbols"

test -s "$symbols"
xtensa-esp32-elf-objcopy --redefine-syms="$symbols" "$archive" "$temporary"
xtensa-esp32-elf-objcopy --strip-debug "$temporary"

mkdir -p "$(dirname -- "$output")"
install -m 0644 "$temporary" "$output"
xtensa-esp32-elf-nm -g --defined-only "$output" 2>/dev/null |
  awk 'NF >= 3 {print $3}' | sort -u > "$defined_symbols"

for symbol in \
  espble_bd_esp_bluedroid_attach_hci_driver \
  espble_bd_esp_spp_register_callback \
  espble_bd_esp_bt_hid_device_register_callback \
  espble_bd_esp_bt_hid_host_register_callback; do
  if ! grep -Fxq "$symbol" "$defined_symbols"; then
    echo "generated archive is missing $symbol" >&2
    exit 1
  fi
done

echo "generated $output from ESP-IDF $idf_version"
