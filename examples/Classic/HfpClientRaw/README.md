# HfpClientRaw (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The headset side of HFP: a call's control path, already-encoded SCO audio, and the
things an accessory tells the phone about itself. **EspBle carries encoded
payloads** — decoding CVSD or mSBC and driving a speaker belong to another library.
**Classic works on the original ESP32 only**, and HFP reaches Classic phones and
Audio Gateways. **The Client and Audio Gateway roles are mutually exclusive within
one process**, so this cannot also be
[HfpAudioGatewayRaw](../HfpAudioGatewayRaw/).

## Hardware

- 1 × original ESP32 running this sketch
- 1 × phone, or a second original ESP32 running
  [HfpAudioGatewayRaw](../HfpAudioGatewayRaw/)

Put the Audio Gateway's address in `audioGatewayAddress`; [Inquiry](../Inquiry/)
finds one.

## What it does

- Connects and reports the connection and call state
- Once the service-level connection exists, tells the phone about the accessory:
  enables the Apple extensions, reports a battery level, and asks the phone to stop
  its own noise reduction
- Asks for the network operator and the subscriber number, and prints the answers
- Receives SCO frames as encoded views

## Key APIs

- `bluetooth.hfpClient().connect(address)` / `onConnectionChanged()` /
  `serviceLevelConnected()`
- `enableAppleExtensions(identification)` then `reportBatteryLevel(level, docked)`
  — level 0 to 9; the extensions have to be enabled first
- `disableNoiseReduction()` — for an accessory that does its own
- `queryOperatorName()` / `requestSubscriberNumber()` — requests, answered at
  `onOperatorName()` / `onSubscriberNumber()`
- `dialMemory(location)` — dial from the phone's memory by position
- `onAudio()` — encoded SCO; `audio.data` is valid only inside the callback

## Notes

**Everything about the accessory needs the service-level connection**, not just an
ACL link, which is why this sketch waits for `serviceLevelConnected()` rather than
for `connect()` to return.

A phone may answer a query with an empty string, and may ignore
`disableNoiseReduction()`. There is no call to turn noise reduction back on: the
request lasts for the connection.

**Copy the SCO payload before returning from `onAudio()`.** Keep `badFrame` so an
mSBC decoder can conceal the loss instead of clicking.

Call waiting and three-way calling (CHLD, BTRH) are not implemented.
