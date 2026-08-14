"""The HCI layer must stay portable enough to be proposed upstream.

The routing modules answer questions about H4 packets and controller state, so
nothing in them needs a platform. The broker drives the physical transport and
therefore needs ESP-IDF, but it must not learn how a particular SDK links its
libraries: the integration layer answers that through the broker's hook. These
checks fail the moment a platform header creeps back in.
"""

import re
from pathlib import Path


ROOT = Path(__file__).parent / ".." / ".." / ".."
SRC = (ROOT / "src").resolve()

# Pure logic: H4 parsing, ownership tables, scheduling arithmetic, credits.
PORTABLE_MODULES = [
    "EspBleHciRouter",
    "EspBleHciCommandScheduler",
    "EspBleHciControllerPolicy",
    "EspBleHciAclCredits",
]

# The broker owns the physical transport, so ESP-IDF and FreeRTOS are expected.
BROKER_ALLOWED_INCLUDES = {
    "sdkconfig.h",
    "EspBleClassicBuild.h",
    "EspBleHciBroker.h",
    "EspBleHciAclCredits.h",
    "EspBleHciCommandScheduler.h",
    "EspBleHciControllerPolicy.h",
    "EspBleHciRouter.h",
    "stdbool.h",
    "stddef.h",
    "stdint.h",
    "string.h",
    "esp_bt.h",
    "esp_err.h",
    "esp_log.h",
    "freertos/FreeRTOS.h",
    "freertos/semphr.h",
    "freertos/task.h",
}

PORTABLE_ALLOWED_INCLUDES = {"stdbool.h", "stddef.h", "stdint.h", "string.h"}

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def includes_of(path):
    return set(INCLUDE.findall(path.read_text()))


def test_routing_modules_have_no_platform_dependency():
    for module in PORTABLE_MODULES:
        for suffix in (".h", ".c"):
            path = SRC / (module + suffix)
            allowed = PORTABLE_ALLOWED_INCLUDES | {module + ".h"}
            unexpected = includes_of(path) - allowed
            assert not unexpected, f"{path.name} pulled in {sorted(unexpected)}"


def test_broker_depends_on_esp_idf_only():
    for suffix in (".h", ".c"):
        path = SRC / ("EspBleHciBroker" + suffix)
        unexpected = includes_of(path) - BROKER_ALLOWED_INCLUDES
        assert not unexpected, f"{path.name} pulled in {sorted(unexpected)}"


def test_vendored_nimble_port_asks_the_broker_instead_of_the_sdk():
    """The NimBLE port needs to know whether a Classic host shares the
    controller. It must ask the broker, not an Arduino core header, or the
    vendored host stops being portable to a plain ESP-IDF build."""
    path = SRC / "nimble_esp32" / "src" / "porting" / "nimble" / "src" / "nimble_port.c"
    text = path.read_text()
    assert "espble_hci_broker_classic_host_expected" in text
    assert "esp32-hal" not in text


def test_only_the_integration_layer_knows_the_sdk():
    """Exactly one translation unit may read the core's linked-library flags."""
    users = [
        path.name
        for path in SRC.glob("*.cpp")
        if "esp32-hal-bt.h" in path.read_text()
    ]
    assert users == ["EspBleClassic.cpp"], users
