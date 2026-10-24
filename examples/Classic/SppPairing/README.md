# SppPairing (Classic)

> 日本語版: [README.ja.md](README.ja.md)

Classic pairing under application control, over an SPP server. Without a security
configuration the stack pairs with Just Works and accepts every request, which is
fine for a closed setup and wrong for anything a stranger can reach. **Classic
works on the original ESP32 only**, and Classic pairing produces a link key that
is separate from a BLE bond: removing one does not remove the other.

## Hardware

- 1 × original ESP32 running this sketch
- 1 × peer that pairs: a PC or Android device, or a second original ESP32 running
  [SppClient](../SppClient/)

## What it does

- Enables security with the `DisplayYesNo` IO capability, which is what produces a
  number both sides compare
- Answers the numeric comparison from the application — auto-accepting here, where
  a real product asks the user
- Reports the pairing result, success or failure, with the backend status
- Lists the bonds stored in NVS at startup

## Key APIs

- `EspBleClassicConfig::security` — `enabled` and `ioCapability`, both read during
  `begin()`
- `onNumericComparisonRequested()` / `confirmNumericComparison()` — the question
  and the answer
- `onPasskeyDisplayed()` / `onPasskeyRequested()` / `providePasskey()` — the other
  IO capabilities
- `bondCount()` / `bond(index)` / `deleteBond()` / `deleteAllBonds()`

## Notes

**Without `security.enabled` the IO capability has no effect.** Secure Simple
Pairing only involves the application when a service asks for it, so enabling
security is what makes the SPP service require MITM protection.

Answering nothing rejects the pairing once `responseTimeoutMilliseconds` elapses.
A peer left waiting forever is worse than a refusal.

Legacy PIN pairing is refused. There is no way to answer it here, and a fixed PIN
would be a fixed key.

## Related guides

- [Classic guide §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.md#4-spp) — RFCOMM channels, several services and the byte stream
- [EspBle in depth](../../../docs/GUIDE_ADVANCED.md) — the 990-byte packet, the eight-write queue and backpressure

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [SppPairing.ino](SppPairing.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

Classic pairing under application control. Without a security config the stack pairs with Just Works and accepts every request, which is fine for a closed setup and wrong for anything a stranger can reach.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.confirmNumericComparison(...)` — Confirm only after comparing the displayed values on both peers; reject a mismatch rather than automatically accepting the event.
2. `bluetooth.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
3. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.

## Verify and adapt

Compare the displayed numeric values before approving and inspect the security result. Stored Classic bonds can bypass a fresh pairing prompt after a restart; use bond management deliberately when repeating the test.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
