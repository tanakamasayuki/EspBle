import re


def test_gatt_read_write(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("DATABASE_REQUESTED", timeout=20)
    dut.expect_exact(
        "DATABASE success=1 services_ok=1 chars=1 descs=1 found=1/1 write_nr=1 context=loop",
        timeout=30,
    )
    dut.expect_exact("DISCOVER_REQUESTED", timeout=20)
    dut.expect_exact(
        "DISCOVER success=1 read=1 write=1 context=loop",
        timeout=20,
    )
    dut.expect_exact("READ_REQUESTED", timeout=10)
    dut.expect_exact(
        "READ success=1 value=peer-ready context=loop",
        timeout=20,
    )
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE phase=0 success=1 response=1 context=loop", timeout=20)
    dut.expect_exact("WRITE_NR_REQUESTED", timeout=10)
    dut.expect_exact("WRITE phase=1 success=1 response=0 context=loop", timeout=20)
    dut.expect_exact("DESCRIPTOR_READ_REQUESTED", timeout=10)
    dut.expect_exact(
        "DESCRIPTOR_READ success=1 value=peer-description context=loop", timeout=20
    )
    dut.expect_exact("DESCRIPTOR_WRITE_REQUESTED", timeout=10)
    dut.expect_exact(
        "DESCRIPTOR_WRITE success=1 response=1 context=loop", timeout=20
    )

    peripheral.expect_exact(
        "SERVER_WRITE id=1 value=central-write stored=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SERVER_WRITE id=1 value=central-no-response stored=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SERVER_DESCRIPTOR_WRITE value=descriptor-written stored=1 context=loop",
        timeout=20,
    )
    peripheral.write("d")
    peripheral.expect_exact(
        "SERVER_DESCRIPTOR found=1 value=descriptor-written", timeout=20
    )
    dut.write("t")
    dut.expect_exact("TIMEOUT_ZERO accepted=0 error=INVALID_ARGUMENT", timeout=10)
    dut.expect_exact("TIMEOUT_REQUESTED", timeout=10)
    dut.expect_exact(
        "TIMEOUT_RESULT success=0 error=TIMEOUT count=1 context=loop", timeout=10
    )
    # The recovery read issued during the still-in-flight (timed-out) slow read
    # is accepted and queued, then completes once the slow backend read returns.
    dut.expect_exact("TIMEOUT_BUSY next=1", timeout=10)
    dut.expect_exact(
        "TIMEOUT_RECOVERY success=1 value=central-no-response timeout_count=1 context=loop",
        timeout=20,
    )
    dut.write("x")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("DISCONNECTED services=0 context=loop", timeout=20)


HEAP = re.compile(rb"HEAP free=(\d+)")


def test_discovery_cycles_do_not_leak_heap(dut, peers):
    """Repeated discovery + read/write cycles must not leak per cycle.

    The generic GATT client talks to the peer through the NimBLE host API
    against its own attribute-table snapshot, so no wrapper remote objects are
    created and nothing is left behind when the connection ends.
    """
    peripheral = peers["device"]
    heaps = []
    cycles = 8

    for _ in range(cycles):
        peripheral.write("?")
        peripheral.expect_exact("ADVERTISING 1", timeout=20)
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
