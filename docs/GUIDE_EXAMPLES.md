# Run a first example and build on it

> 日本語版: [GUIDE_EXAMPLES.ja.md](GUIDE_EXAMPLES.ja.md)

The [example index](../examples/README.md) lists the sketches by feature. Each folder has an English and Japanese README explaining the exact peer, Serial commands, API sequence and expected result. This guide covers the steps shared across them. For Bluetooth concepts, use [BLE basics](GUIDE_BLE_BASICS.md) or [Classic basics](GUIDE_CLASSIC_BASICS.md); for platform support, consult the [library README](../README.md) and its linked matrices rather than assuming every ESP32 has the same radio features.

## Choose a small working pair

| Goal | Start here | Peer or next step |
|---|---|---|
| Observe BLE advertisements | [Gap/Scan](../examples/Gap/Scan/README.md) | [Gap/Advertise](../examples/Gap/Advertise/README.md) on a second board |
| Read and write a value | [Gatt/Basics/Client](../examples/Gatt/Basics/Client/README.md) | [Server](../examples/Gatt/Basics/Server/README.md) |
| Receive periodic changes | [SubscribeClient](../examples/Gatt/Basics/SubscribeClient/README.md) | [NotifyServer](../examples/Gatt/Basics/NotifyServer/README.md) |
| Observe acknowledged updates | [IndicateClient](../examples/Gatt/Basics/IndicateClient/README.md) | [IndicateServer](../examples/Gatt/Basics/IndicateServer/README.md) |
| Exchange Serial lines over BLE | [NusClient](../examples/Gatt/Basics/NusClient/README.md) | [NusServer](../examples/Gatt/Basics/NusServer/README.md) |
| Implement a sensor value | [BatteryServer](../examples/Gatt/Device/BatteryServer/README.md) | [BatteryClient](../examples/Gatt/Device/BatteryClient/README.md), then the matching sensor/health/fitness pair |
| Type from an ESP32 | [Hid/KeyboardDevice](../examples/Hid/KeyboardDevice/README.md) | A compatible computer/phone, then [KeyboardHost](../examples/Hid/KeyboardHost/README.md) |
| Understand authenticated pairing | [NumericComparisonServer](../examples/Security/NumericComparisonServer/README.md) | [NumericComparisonClient](../examples/Security/NumericComparisonClient/README.md) |
| Exchange MIDI events | [MidiDevice](../examples/Midi/MidiDevice/README.md) | [MidiHost](../examples/Midi/MidiHost/README.md) or a BLE MIDI application |
| Use a Classic byte stream | [SppServer](../examples/Classic/SppServer/README.md) | [SppClient](../examples/Classic/SppClient/README.md), original ESP32 hardware |
| Inspect advertisements or links | [ScanDump](../examples/Info/ScanDump/README.md) | [ConnectionInspector](../examples/Info/ConnectionInspector/README.md) |
| Use a P4 with a radio peer | [Hosted setup](ESP_HOSTED_SETUP.ja.md) | [Hosted examples](../examples/Hosted/CustomPins/README.md) |

A central finds and initiates a BLE connection; a peripheral advertises and accepts it. A GATT client requests values or subscribes, and a GATT server exposes them. These are distinct role pairs, although many examples combine central/client and peripheral/server. HID Device and HID Host describe input/report roles, not USB wiring. Classic inquiry/SPP/audio are separate from BLE advertising/GATT; choose matching peers.

## Prepare and upload

1. Install EspBle using the root README instructions and select the actual Arduino board, port and required board options. Use the root support information for core/board compatibility; do not copy options from an unrelated ESP32 model.
2. Open the chosen example `.ino`. A pair normally requires two boards, or one board plus a compatible phone/computer/device. Start the server/device first. Put a Classic peer into discoverable/pairing mode when its example requires it.
3. Change only the explicit application settings: peer address, device name, UUID filter, or local Wi-Fi credentials where relevant. Keep address type with the address. Changing a local advertised name does not automatically change the peer's UUID filter.
4. Upload and match the Serial monitor baud rate to `Serial.begin(...)`. Most examples print their command keys; a server that publishes binary values may be intentionally quiet. Verify it from the matching client instead of expecting a continuous server log.
5. First reproduce the unmodified payload exchange. Then replace one source of sample data at a time, retaining packet encoding, field units and validation.

For ESP-Hosted, configure wiring and radio-side firmware as described in the linked setup guide. Custom pin overrides must precede initialization by either BLE or Wi-Fi because both share the transport. Prefer the board definition when it already represents your hardware.

## Follow the lifecycle in the sketch

For a BLE server, create services/characteristics and initial values before `ble.begin()`. HID/MIDI device helpers also compose a server database before stack initialization. Client helpers may install their callbacks after initialization: follow the order in that example rather than applying the device sequence to a host.

Check `begin()` and the results of configuration/start calls. Discover a suitable peer or advertise your service, request a connection, then wait for connection events. A successful immediate `connect()` return means an attempt was accepted, not that the link exists. Likewise, a read request returns before its value arrives and subscribing is a separate stage after connecting. The callback names and result types show where to continue.

Keep `ble.update()` or `bluetooth.update()` running from `loop()`. Registering `onConnected(...)` installs a function; it does not run the body at registration time. Short callbacks can update state for later loop work. Avoid long blocking Serial reads, delays and sensor operations that postpone all other queued events.

There are important execution-context exceptions:

| Callback | Execution and data handling |
|---|---|
| Ordinary BLE events and Classic control events | Dispatched by the corresponding `update()`; keep them short so the loop continues |
| GATT server `onRead()` | Runs synchronously on the BLE stack task; prepare the value promptly with `setValue()` and avoid slow I/O or waits |
| Classic A2DP `onMedia()` and HFP `onAudio()` | Run in the Bluetooth host context; copy encoded, borrowed bytes to a bounded queue before return, then decode/play elsewhere |

Event callback references should not be saved as pointers for later work. Copy the required values into application-owned storage; pointer-bearing audio/report/SysEx views need their payload copied as well. Calling a view's copy constructor does not necessarily copy the pointed-to bytes. For HFP audio, do not replace/remove the callback or call `end()` from inside it.

## Interpret results and binary values

| Operation | What to check |
|---|---|
| Configure a local GATT database | Returned handle `.valid()` or bool, followed by successful stack initialization |
| Request connection | Immediate acceptance, then connected or connection-failed event |
| Read/write a remote value | The corresponding completion result, including `success`, connection and UUID/handle |
| Subscribe | Subscription completion, then matching updates; the notify/indicate mode must match the characteristic |
| `setValue()` | Changes the stored value; it does not send a notification on its own |
| `notify()` | Immediate queued acceptance and send result where used; there is no ATT delivery confirmation from the peer |
| `indicate()` | Immediate queued acceptance, then per-peer ATT confirmation through the send result |
| Pair/security request | Security result and separate encrypted/authenticated/bonded flags |

A successful write or indication does not prove your peer application performed a real-world action. Add an application response if that distinction matters. In Write Without Response mode, no ATT write response comes back from the peer.

GATT values are bytes, even when carried by Arduino `String`. Use length checks before indexing and do not use null-terminated text operations for arbitrary binary data. Validate flags, signedness, little-endian order and units before converting a packet. Medical SFLOAT/FLOAT formats are not native C++ floating-point memory layouts. Standard sensor examples publish demonstration values; attach data acquisition separately.

The negotiated ATT MTU, not merely the preferred setting, determines usable payload size. Define message framing and chunking explicitly for NUS or long application data. SPP is a byte stream, so callback chunks can split or combine your messages; use a bounded parser and retain partial input.

## Adapt roles without losing state

Keep state per connection/session. In disconnect callbacks, release readiness and discard stale IDs and discovered handles. Some examples intentionally do not reconnect; add a clear policy rather than assuming scanning/advertising restarts itself. Automatic reconnect and persistent subscription restoration solve different parts of the problem. With multiple peers, route reads, notifications and outputs to the correct connection.

Primary `onX(...)` setters can replace earlier registrations. When adding application callbacks alongside profile helpers, use the API's listener variants where provided and check their returned listener IDs instead of accidentally removing helper behavior. See [advanced execution and resource guidance](GUIDE_ADVANCED.md) for queueing, backpressure and reconnection.

For HID, connection alone may precede encryption, discovery and subscription. Send only when the relevant profile is ready. Press/release operations maintain state; send a release for held keys/buttons and keep descriptor/report sizes consistent. Test input in a harmless window because a computer may act on reports immediately. If the descriptor changes, a host may need its stored pairing/cache removed before it discovers the new layout.

For security, pairing establishes link security and bonding saves trust records. Just Works can encrypt without MITM authentication. Numeric Comparison requires the user to compare both displays, while passkey examples require the currently displayed or deliberately configured value. A fixed demonstration passkey is not a deployment policy. Restarting a sketch does not erase saved bonds; reset both sides' records deliberately when testing a fresh pairing.

For audio, encoded SBC or CVSD/mSBC data is not PCM. Add the external encoder/decoder and audio I/O appropriate to the negotiated codec. Preserve pending source frames on `WouldBlock` and retry with bounded buffering; never move a slow decoder into a host callback to avoid a queue.

## Troubleshoot by the last completed stage

| Symptom | Next check |
|---|---|
| Initialization fails | Actual board/core/backend, root support docs and `lastErrorName()` / `lastErrorDetail()` immediately after failure |
| Nothing discovered | Matching BLE versus Classic role, peer advertising/discoverable state, UUID/address filter, address type, active scan if name is in scan response |
| Connected but no values | Service discovery, security requirements, correct characteristic, subscription success and notify/indicate mode |
| Values appear truncated or inconsistent | Actual payload length, flags, units, negotiated MTU and your message framing |
| Commands appear to stop responding | Continued update calls, blocking Serial or callback work, dropped-event/result counters |
| Pairing prompt does not recur | Saved bonds on both sides; disconnect before intentional bond deletion |
| Host connects but input does nothing | Profile readiness, encryption/subscription, descriptor layout, host cache and input focus |
| Audio packets arrive but no sound | Encoded codec configuration, external decoder, buffering and physical audio output |
| Requests are rejected under load | Return values, pending work, bounded retry/backpressure and available resources |

The per-example README explains its exact operation and peer. Keep changing compatibility/verification details in the root README and linked status documents rather than copying them into an application walkthrough.
