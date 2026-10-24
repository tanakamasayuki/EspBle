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

The Client keeps a single-call model on purpose, matching the Audio Gateway:
call waiting and three-way calling (CHLD, BTRH) are not implemented.

## Related guides

- [Classic guide §8 HFP](../../../docs/GUIDE_CLASSIC_BASICS.md#8-hfp) — the service-level connection, call control and raw SCO

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HfpClientRaw.ino](HfpClientRaw.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

The headset side of HFP: a call's control path plus already-encoded SCO audio. EspBle carries encoded payloads, so decoding CVSD or mSBC and driving a speaker belong to another library. An accessory also has things to say about itself, which is the second half of this sketch.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.hfpClient().onAudio(...)` — This encoded SCO audio callback runs in the Bluetooth host context. Copy data before return, keep work bounded and do not replace the callback or call end from inside it.
3. `bluetooth.hfpClient().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Connect to a real audio gateway and inspect service-level and audio-connection events separately. Receiving encoded SCO frames is not microphone/speaker I/O; handle codecs and buffering outside the host callback.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. The stack/host callbacks described above are exceptions: perform only the immediate response or bounded copy they require, rather than assuming they run from update.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
