import re

HEAP = re.compile(rb"HEAP free=(\d+)\r?\n")


def test_discovery_cycles_do_not_leak_heap(dut, peers):
    """Repeated discovery + read/write cycles must not leak per cycle.

    The generic GATT client talks to the peer through the NimBLE host API
    against its own attribute-table snapshot, so no wrapper remote objects are
    created and nothing is left behind when the connection ends.
    """
    peripheral = peers["device"]
    heaps = []
    cycles = 8

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=20)
    for cycle in range(cycles):
        if cycle != 0:
            # The peer restarts advertising from its own disconnect handler.
            peripheral.expect_exact("DEVICE_READVERTISING 1", timeout=20)
        dut.write("r")
        dut.expect_exact("REARMED", timeout=10)
        dut.write("s")
        dut.expect_exact("SCAN_STARTED", timeout=10)
        dut.expect_exact("CONNECT_REQUESTED", timeout=20)
        dut.expect_exact("DESCRIPTOR_WRITE success=1 response=1 context=loop", timeout=30)
        dut.write("x")
        dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
        dut.expect_exact("DISCONNECTED services=0 context=loop", timeout=20)
        dut.write("h")
        heaps.append(int(dut.expect(HEAP, timeout=10).group(1)))

    # Skip the first cycles while allocator pools settle; the rest must be flat.
    settled = heaps[2:]
    loss = settled[0] - settled[-1]
    per_cycle = loss / (len(settled) - 1)
    assert loss < 3000, (
        f"heap shrank by {loss} bytes over {len(settled) - 1} cycles "
        f"(~{per_cycle:.0f} bytes/cycle, heap samples: {heaps})"
    )
