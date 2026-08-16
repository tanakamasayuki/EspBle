"""Every Bluedroid call from the Classic sources must be namespaced.

EspBle links its own Classic-only Bluedroid host, whose symbols are prefixed
`espble_bd_`. Each source that calls into it maps the public name onto that
prefix with a `#define`. Forgetting one define does not fail the build: the
plain name resolves against the Bluedroid that Arduino-ESP32 ships, so the call
silently reaches a different stack — and the linker pulls that stack in, which
cost over 500 KB of flash and a wrong runtime target when
`esp_spp_stop_srv_scn()` was added without its define.

Nothing about that failure is visible in a compile log, so it is checked here.
"""

import re
from pathlib import Path


ROOT = Path(__file__).parent / ".." / ".." / ".."
SRC = (ROOT / "src").resolve()

# Sources that talk to the bundled Classic host.
CLASSIC_SOURCES = sorted(SRC.glob("EspBleClassic*.cpp"))

# Prefixes of the Bluedroid public API the Classic host provides.
BLUEDROID_PREFIXES = (
    "esp_ble_",
    "esp_bt_gap_",
    "esp_bt_hid_device_",
    "esp_bt_hid_host_",
    "esp_bluedroid_",
    "esp_spp_",
    "esp_a2d_",
    "esp_avrc_",
    "esp_hf_",
)

# Calls that legitimately reach the platform rather than the bundled host: the
# controller API belongs to ESP-IDF itself, not to the Bluedroid host.
PLATFORM_EXCEPTIONS = {
    "esp_bt_controller_init",
    "esp_bt_controller_deinit",
    "esp_bt_controller_enable",
    "esp_bt_controller_disable",
    "esp_bt_controller_get_status",
    "esp_bt_controller_mem_release",
    "esp_bt_mem_release",
    "esp_bt_sleep_enable",
    "esp_bt_sleep_disable",
}

CALL = re.compile(r"\b(esp_[a-z0-9_]+)\s*\(")
DEFINE = re.compile(r"^\s*#\s*define\s+(esp_[a-z0-9_]+)\b", re.MULTILINE)
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT = re.compile(r"//[^\n]*")


def code_only(text):
    """Comments name these functions when explaining why one is not used."""
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def test_every_bluedroid_call_is_namespaced():
    missing = []
    for path in CLASSIC_SOURCES:
        text = path.read_text()
        defined = set(DEFINE.findall(text))
        text = code_only(text)
        for name in sorted(set(CALL.findall(text))):
            if not name.startswith(BLUEDROID_PREFIXES):
                continue
            if name in PLATFORM_EXCEPTIONS or name in defined:
                continue
            # The definition itself matches the call pattern; skip the mapping.
            if name.startswith("espble_bd_"):
                continue
            missing.append(f"{path.name}: {name}")
    assert not missing, (
        "these Bluedroid calls would reach the core's stack instead of the "
        "bundled host: " + ", ".join(missing)
    )


def test_the_check_can_see_the_calls_it_guards():
    """A pattern that matches nothing would pass silently."""
    seen = 0
    for path in CLASSIC_SOURCES:
        text = code_only(path.read_text())
        seen += len([
            name for name in set(CALL.findall(text))
            if name.startswith(BLUEDROID_PREFIXES)
        ])
    assert seen > 20, seen
