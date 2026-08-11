def test_custom_classic_host_initializes_a2dp_sink(dut):
    dut.expect_exact(
        "CLASSIC_A2DP_STACK started=1 error=None:", timeout=20
    )
    dut.expect_exact(
        "CLASSIC_A2DP_SINK started=1 initialized=1 error=None:", timeout=20
    )
    dut.expect_exact("CLASSIC_A2DP_SINK_ENDED initialized=0", timeout=20)
    dut.expect_exact("CLASSIC_A2DP_STACK_ENDED initialized=0", timeout=20)
