# Manual (multi-board) tests

Tests here need hardware that is not always attached — typically a third
ESP32-S3 board in addition to the two used by `tests/peer/`. They are kept out
of `tests/peer/` so the default `pytest peer/` run never depends on the extra
board.

Each test skips automatically when a required peer port is not configured, so
running one without the extra board is safe.

## Run

**Name the file.** These tests are deliberately not named `test_*.py`, which is
what keeps them out of every default run — including `pytest` and `pytest .` —
without any setting to get wrong. The cost is that naming the directory
collects nothing: `pytest manual/` reports "no tests ran" rather than an error.

From `tests/`, with the extra board's port set in `.env`:

```
uv run --env-file .env pytest manual/multi_connection/multi_connection.py
```

## Boards / ports

| Role | Profile | Port env (`.env`) | Default |
| --- | --- | --- | --- |
| Central (host) | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` | `/dev/ttyACM0` |
| Peripheral A | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` | `/dev/ttyACM1` |
| Peripheral B | `s3_peer_device2` | `TEST_SERIAL_PORT_PEER_DEVICE2` | `/dev/ttyACM3` |

## Tests

- `multi_connection`: one Central holds two Peripheral connections at once,
  routes each peer's notifications to the correct connection, then Peripheral A
  drops unexpectedly. With `setAutoReconnect(true)` the Central reconnects to A
  on its own and the default persistent subscription is restored (notifications
  resume with no re-subscribe), while Peripheral B stays connected throughout.
