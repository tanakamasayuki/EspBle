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
