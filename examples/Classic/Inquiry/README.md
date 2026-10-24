# Inquiry (Classic)

> 日本語版: [README.ja.md](README.ja.md)

Bluetooth Classic device discovery. Every other Classic profile connects by
address, so this is where an address comes from when a sketch does not already
know one. **Classic works on the original ESP32 only**, and it finds Classic
devices — a BLE-only peripheral never answers an inquiry, and BLE scanning
(`EspBle::scanner()`) never finds a Classic-only device.

## Hardware

- 1 × original ESP32 running this sketch
- Any discoverable Classic device nearby: a phone with its Bluetooth screen open,
  a headset in pairing mode, or a second ESP32 running
  [SppServer](../SppServer/)

## What it does

- Runs one inquiry for about 10 s and prints each result
- Prints the name, Class of Device and RSSI only when the response carried them,
  because an inquiry result often has no name
- Reports the end separately, distinguishing a cancelled inquiry from one that
  ran its course

## Key APIs

- `bluetooth.inquiry().start(config)` — begin discovery; true means it started,
  not that it finished
- `onResult()` — one call per response, so a peer answering twice is reported
  twice; deduplicate by address if that matters
- `onComplete()` — `cancelled` tells `stop()` apart from the duration expiring
- `bluetooth.inquiry().requestName(address)` /
  `requestServices(address)` — ask a peer directly, which is how a missing name
  or a service list is obtained. Neither is answered while an inquiry is running

## Notes

`durationSeconds` is rounded up: the controller counts in 1.28 s units, so 10
becomes 10.24 s.

A device that accepts connections is not necessarily one an inquiry finds. A peer
set to `ConnectableOnly` stays out of every result while still accepting
connections from someone who knows its address.

## Related guides

- [Classic guide §3 Inquiry](../../../docs/GUIDE_CLASSIC_BASICS.md#3-inquiry) — what a scan returns, and the queries that take an address

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Inquiry.ino](Inquiry.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

Classic device discovery. Every other Classic profile connects by address, so this is where an address comes from when the sketch does not already know one.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.inquiry().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.

## Verify and adapt

Classic inquiry finds Classic peers; BLE-only devices are not a failed inquiry result. Put the intended peer in discoverable mode and record the address for the matching Classic client example.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
