import re

import pytest


READY = re.compile(rb"FLASH_BOOTSTRAP_READY nvs=1 free=([1-9]\d*)")


@pytest.mark.espble_flash_bootstrap
def test_ble_fixture_starts_with_writable_nvs(dut, peers, probe):
    probe(dut, "?", READY)
    probe(peers["device"], "?", READY)
