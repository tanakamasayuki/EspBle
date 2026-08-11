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

for tool in idf.py xtensa-esp32-elf-gcc xtensa-esp32-elf-nm \
  xtensa-esp32-elf-objcopy sha256sum; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 2
  }
done

gcc_version=$(xtensa-esp32-elf-gcc -dumpfullversion)
if [[ $gcc_version != 14.2.0 ]]; then
  echo "xtensa-esp32 GCC 14.2.0 is required; found $gcc_version" >&2
  exit 2
fi

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

if grep -Ev '^espble_bd_' "$defined_symbols" | grep -q .; then
  echo "generated archive contains unprefixed global defined symbols" >&2
  grep -Ev '^espble_bd_' "$defined_symbols" >&2
  exit 1
fi

for symbol in \
  espble_bd_esp_bluedroid_attach_hci_driver \
  espble_bd_esp_spp_register_callback \
  espble_bd_esp_bt_hid_device_register_callback \
  espble_bd_esp_bt_hid_host_register_callback \
  espble_bd_esp_a2d_register_callback \
  espble_bd_esp_a2d_audio_buff_free \
  espble_bd_esp_a2d_audio_buff_alloc \
  espble_bd_esp_a2d_media_ctrl \
  espble_bd_esp_a2d_sink_init \
  espble_bd_esp_a2d_sink_deinit \
  espble_bd_esp_a2d_sink_connect \
  espble_bd_esp_a2d_sink_disconnect \
  espble_bd_esp_a2d_sink_register_audio_data_callback \
  espble_bd_esp_a2d_sink_register_stream_endpoint \
  espble_bd_esp_a2d_source_init \
  espble_bd_esp_a2d_source_deinit \
  espble_bd_esp_a2d_source_connect \
  espble_bd_esp_a2d_source_disconnect \
  espble_bd_esp_a2d_source_register_stream_endpoint \
  espble_bd_esp_a2d_source_audio_data_send \
  espble_bd_esp_avrc_ct_init \
  espble_bd_esp_avrc_tg_init \
  espble_bd_esp_hf_client_register_audio_data_callback \
  espble_bd_esp_hf_client_audio_data_send \
  espble_bd_esp_hf_ag_register_audio_data_callback \
  espble_bd_esp_hf_ag_audio_data_send; do
  if ! grep -Fxq "$symbol" "$defined_symbols"; then
    echo "generated archive is missing $symbol" >&2
    exit 1
  fi
done

archive_size=$(wc -c < "$output" | tr -d '[:space:]')
archive_sha256=$(sha256sum "$output" | awk '{print $1}')
defined_count=$(wc -l < "$defined_symbols" | tr -d '[:space:]')
echo "generated $output"
echo "source: ESP-IDF $idf_version, xtensa-esp32 GCC $gcc_version"
echo "archive: $archive_size bytes, $defined_count global defined symbols"
echo "sha256: $archive_sha256"
