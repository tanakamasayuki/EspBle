# A beginner's guide to Bluetooth Classic

> 日本語版: [GUIDE_CLASSIC_BASICS.ja.md](GUIDE_CLASSIC_BASICS.ja.md)

This guide explains the concepts of Bluetooth Classic (BR/EDR) and where the API
boundaries are. The code lives in the [Classic examples](../examples/README.md).
For BLE see the [BLE communication beginner guide](GUIDE_BLE_BASICS.md); for
deciding between the two radios see [BLE and Classic](CLASSIC_VS_BLE.md).

**Classic works on the original ESP32 only.** ESP32-S3/C3/C6/H2/P4 have no
BR/EDR radio. Which features are hardware-verified, unverified or unimplemented
is tracked in the [Classic feature inventory](CLASSIC_FEATURE_INVENTORY.ja.md).
The precompiled Classic host is ABI-bound to ESP-IDF 5.5.5 and xtensa-esp32 GCC
14.2.0; because the Bluedroid headers it was built against ship with the library,
the measured core range is Arduino-ESP32 3.2.0 through 3.3.11, with HFP audio
(SCO) needing 3.3.8 or newer.

## 1. A different model from BLE

The two can share one controller, but these concepts are not merged:

```text
BLE scan                   != Classic inquiry
BLE connection             != Classic ACL link
BLE GATT client connection != SPP session
LE pairing / bond          != Classic pairing / link key
```

BLE is `EspBle` and Classic is `EspBleClassic` — separate classes, each
delivering its callbacks from its own `update()`. Starting both makes a sketch
dual-host; there is no build flag.

Classic works in this order: find devices with inquiry, establish an ACL link if
needed, then run a profile for the job. SPP, A2DP and HID share the Classic
transport but are separate profiles with their own data formats and connection
procedures.

| | Bluetooth Classic | BLE |
|---|---|---|
| Finding devices | Inquiry | Advertising / scan |
| What runs on top | Profiles: SPP, A2DP, HID | GATT services and characteristics |
| Serial equivalent | SPP's RFCOMM byte stream | A protocol you define over GATT |
| Stored key | Classic link key | LE bond key / IRK |

Classic does not describe profile connections with BLE's Central and Peripheral
roles. In SPP the listening side is the Server and the dialling side the Client —
and those are not GATT Server and Client either.

## 2. Starting up, and being visible

`EspBleClassic::begin()` starts the controller and the host. Profiles do not
initialise the stack themselves.

```cpp
EspBleClassicConfig config;
config.deviceName = "EspBle Classic";
bluetooth.begin(config);
```

**Anything that composes profiles, such as HID, is configured before
`begin()`.** The composed Report Descriptor is part of the device record a Host
reads while pairing; a profile added later is not in the record it already read.

### 2.1 Connectable and discoverable

Classic has two states, and they mean different things:

| State | Meaning |
|---|---|
| connectable | accepts connections from a peer that knows the address |
| discoverable | answers inquiry, so it can be found |

`EspBleClassicConfig::visibility` and `setVisibility()` choose between `Hidden`,
`ConnectableOnly` and `ConnectableDiscoverable`. `EspBleClassic` owns the value
and profiles only re-assert it — each profile used to set it, so whichever
started last decided whether the device could be found.

**A device you can connect to is not necessarily one you can find.** A peer set
to `ConnectableOnly` accepts connections from someone who knows its address while
staying out of every inquiry result.

### 2.2 Class of Device

This is what a Host uses to pick an icon and, on some Hosts, to decide whether to
offer connecting at all. Left at the default a device is "uncategorised" and may
never be presented as a HID device.

```cpp
config.classOfDevice.majorDeviceClass = 0x05;  // Peripheral
config.classOfDevice.minorDeviceClass = 0x10;  // keyboard
```

`minorDeviceClass` is the 6-bit field, and the over-the-air value is that shifted
left by two. `setClassOfDevice()` reports that the **request was accepted**; the
change is applied asynchronously, so `classOfDevice()` read immediately still
returns the previous value. To confirm it took effect, read it back until it
matches.

### 2.3 Radio and link settings

Three settings change how the radio behaves rather than what a profile does:

| Setting | Call | What it trades |
|---|---|---|
| transmit power | `setTxPower(dBm)` / `setTxPower(min, max)` | range against current draw |
| page timeout | `setPageTimeout(milliseconds)` | how fast a connection attempt fails against giving up on a slow peer |
| minimum encryption key size | `setMinimumEncryptionKeySize(bytes)` | refusing weak links against refusing some peers |

BR/EDR power control picks a level per packet from a range, which is why there is
a range form; a single value pins both ends. Levels are 3 dB apart between -12
and +9 dBm and a value in between is rounded, so `txPower()` reports what the
radio applied. The BR/EDR power is independent of `EspBle::setTxPower()`.

The page timeout is how long paging a peer that answers nothing lasts, which is
how long `connect()` takes to fail when the peer is off or out of range. The
default is 5120 ms. It applies from the next page, so set it before connecting,
and `setPageTimeout()` returning true means the request was accepted —
`pageTimeout()` reports the value the backend confirmed, or 0 until it does.

Related example: [RadioSettings](../examples/Classic/RadioSettings/)

## 3. Inquiry

Inquiry is Classic device discovery, and it is not BLE scanning. A result may
carry an address, a remote name, a Class of Device and an RSSI.

`start()` returning true does not mean discovery finished. Results arrive at
`onResult()` and the end at `onComplete()`; `stop()` also produces a completion
event, distinguished by `cancelled`. Results dropped because the queue filled up
are counted by `droppedResultCount()`.

A name may be missing from an inquiry result. For a peer whose address you
already know, ask directly:

```cpp
bluetooth.inquiry().requestName(address);      // answered at onRemoteName()
bluetooth.inquiry().requestServices(address);  // answered at onRemoteServices()
```

`requestServices()` returns the service UUIDs the peer publishes. **Neither is
answered while a scan is running** — an inquiry and a query both need the radio —
so wait for `onComplete()` first.

Related example: [Inquiry](../examples/Classic/Inquiry/)

## 4. SPP

SPP is a binary-safe byte stream over Classic, carried by RFCOMM. RFCOMM is a
reliable stream modelled on a serial cable; it has nothing to do with GATT
characteristics, MTU or notifications.

### 4.1 Server and Client

A Server listens with a service name and a channel; channel 0 lets the backend
pick a free one. **`startServer()` can be called repeatedly, publishing up to
four services.** Which channel a service received arrives at
`onServerStarted()`. Stopping is `stopServer()`, which stops all of them.

A Client's `connect()` starts SDP and the RFCOMM connection asynchronously. Being
accepted is not being connected: no such device, no SPP service, or a refused
pairing all fail later, at `onConnectionFailed()`. **`connect()` returning true
only means the attempt started.**

When a peer publishes several services, discovery returns every channel and
cannot say which one is wanted. `connectToChannel(address, channel)` names it.
One outgoing connection at a time.

### 4.2 Sending and receiving

Received data arrives both as packet events at `onData()` and as a stream through
`available()` / `read()`. It is binary-safe: a `0x00` in the middle does not
terminate anything. A write request is queued, and true means queued — delivery
is reported at `onWriteCompleted()`. What had to be dropped is counted by
`droppedWriteCount()` and `droppedReceiveByteCount()`.

`EspBleClassicSppStream` is an Arduino `Stream` over one session, so code written
against `Serial` — `print()`, `readStringUntil()`, `parseInt()` — works. It
borrows a session rather than owning one: attach it when a session opens, detach
it when the session closes. Two things differ from `Serial`. A write becomes one
SPP packet, so write lines rather than characters; and the outgoing queue is
finite, so a write with no room waits up to `setWriteTimeout()` (1000 ms by
default, 0 to never wait) and then reports how much it took.

Related examples: [SppServer](../examples/Classic/SppServer/),
[SppClient](../examples/Classic/SppClient/),
[SppStream](../examples/Classic/SppStream/)

## 5. Security and bonds

Classic security is separate from BLE security. SSP numeric comparison, passkey
entry, and listing and removing bonds are available.

| IO capability | What reaches the application |
|---|---|
| `None` | nothing to confirm (Just Works) |
| `DisplayOnly` | shows a six-digit passkey |
| `KeyboardOnly` | types in the passkey the peer showed |
| `DisplayYesNo` | both sides show the same number and confirm |

**Without `EspBleClassicSecurityConfig::enabled` the pairing is Just Works and
the IO capability has no effect.** SSP only involves the application when a
service asks for it, so enabling security makes the service require MITM
protection.

Legacy PIN pairing is **refused**. There is no way to answer it here, and
refusing is safer than accepting automatically with a guessable fixed PIN.

Classic bonds are link keys and BLE bonds are LE keys. Removing one does not
remove the other.

Related example: [SppPairing](../examples/Classic/SppPairing/)

## 6. HID

Report Descriptors and report packing come from the same module the BLE side
uses. **The bytes on the air are the same, and the API names and signatures
match.**

```cpp
bluetooth.hidKeyboard().configure(hidConfig);  // before begin()
bluetooth.hidGamepad().configure(hidConfig);
// ...
bluetooth.hidKeyboard().write("hi");
bluetooth.hidGamepad().send(0, 0, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_UP, 1);
```

Classic registers one device record, so every configured profile goes into one
composed Report Descriptor and each keeps its own report ID.

**How many profiles fit is limited.** The descriptor and the `name`,
`description` and `provider` strings share one SDP record and may total 214
bytes. With the default strings (57 bytes) keyboard + mouse + consumer (144
bytes) fits and adding the gamepad, at 212, does not. The backend does not report
that failure — the device comes up "started" with no record at all — so
`begin()` checks before registering and refuses with `ResourceExhausted`. BLE has
no such limit.

### 6.1 The host side

`hidHost().connect(address)` connects. Classic has no advertisement to filter, so
an address is required. The Report Descriptor received over SDP is parsed into
keyboard state, per-usage keyboard events and mouse events — state first, the
same order the BLE host uses. Reports that cannot be classified arrive raw at
`onInputReport()`.

**The host decodes less here than on BLE: keyboard and mouse only.** Consumer,
system and gamepad reports arrive raw.

### 6.2 The control channel

Get_Report and Set_Report are **exchanges that must be answered**. Without an
answer the Host waits, and some Hosts stop asking afterwards.

- Get_Report: `onReportRequested()` delivers it; answer with
  `respondToReportRequest(request, ...)` or decline with
  `refuseReportRequest(error)`. Passing the request back is what keeps the type
  and report ID matched to what the Host asked for
- Set_Report: the library sends the HID handshake unless the sketch refuses it
- The protocol mode (Boot or Report) belongs to the Host; a device only observes
  it through `protocolMode()` and `onProtocolMode()`

The report ID sits in different places per channel. On the device side `reportId`
is authoritative and `value` is the payload alone. On the host side a report
arrives exactly as the device sent it, so a device that declares report IDs puts
one in front of the payload.

Related examples: [HidKeyboard](../examples/Classic/HidKeyboard/),
[HidGamepad](../examples/Classic/HidGamepad/),
[HidComposite](../examples/Classic/HidComposite/),
[HidKeyboardHost](../examples/Classic/HidKeyboardHost/)

## 7. A2DP and AVRCP

A2DP carries music. The receiving side is the Sink and the sending side the
Source. **EspBle handles already-encoded payloads; codecs and PCM I/O belong to
another library.** SBC encoding and decoding are deliberately not part of EspBle.

For PCM audio, use the released
[PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth) library. It
includes examples that play PC/phone audio on an M5Stack Core2 speaker, send its
microphone to an A2DP-Sink-capable PC, inspect decoded PCM without audio hardware,
and connect the stream to PCMFlow.

`send()` returns `Accepted`, `WouldBlock` or a failure. `WouldBlock` is normal
backpressure, not an error: keep the frame and retry it rather than dropping it,
or the stream develops gaps.

A Sink tells the Source its own playback latency with `setDelay()`, and the
Source receives it at `onSinkDelay()`, in tenths of a millisecond. A Source
showing video holds the picture back by that much. Only the Sink knows its own
latency; this library cannot measure it.

AVRCP carries control and volume, not audio. **AVRCP is started before A2DP,
which the backend requires.** One `avrcp()` object holds both the Controller and
the Target role.

Target notifications are **limited by the bundled host**: volume is the only
event a Target may declare, so reporting play status or track changes as a Target
is unreachable with this build. `supportedNotifications()` reports what is
allowed, and declaring anything else is refused with a message saying so.
Sending metadata or play-status responses as a Target has no public backend API
and is unsupported.

Related examples: [A2dpSinkRaw](../examples/Classic/A2dpSinkRaw/),
[A2dpSource](../examples/Classic/A2dpSource/),
[A2dpSinkAvrcp](../examples/Classic/A2dpSinkAvrcp/),
[AvrcpController](../examples/Classic/AvrcpController/)

Complete audio examples: [PCMFlowBluetooth examples](https://github.com/tanakamasayuki/PCMFlowBluetooth/tree/main/examples)

## 8. HFP

HFP is for calls. The headset side is `hfpClient()` and the phone side
`hfpAudioGateway()`, and **the two roles are mutually exclusive within a
process**. Unlike A2DP the audio is mono: CVSD is 8 kHz and mSBC 16 kHz.

SCO payloads are handed over as already-encoded raw views. On hardware a 57-byte
mSBC transmission arrives as a padded 58 or 60-byte view, and bad frames arrive as
60 bytes too. Pass the length and the bad-frame flag through to the decoder rather
than discarding them.

Beyond calls, an accessory can ask the phone about itself and tell the phone
about itself. `queryOperatorName()` and `requestSubscriberNumber()` are requests,
answered at `onOperatorName()` and `onSubscriberNumber()`; an empty answer is a
legal answer. `disableNoiseReduction()` asks the phone to stop its own noise
reduction, for an accessory that does its own — two in series sound worse than
one. `enableAppleExtensions()` followed by `reportBatteryLevel()` is how a
battery level reaches a phone; Apple defined it and Android and Windows accept
it, and the level runs from 0 to 9.

`dialMemory(location)` dials from the phone's memory by position. On the Audio
Gateway side that arrives as `DialMemory` rather than `Dial`, because a position
is not a number and dialling the digits would call the wrong party. The Apple
extensions arrive there as `UnknownAt` text: nothing in the backend decodes them.

`setInBandRingTone()` on the Audio Gateway tells the accessory who makes the ring
sound, and the accessory hears it at `onInBandRingTone()`. An accessory told the
wrong thing either rings twice or waits for ring audio that never arrives.

Call waiting and three-way calling (CHLD, BTRH) are **not implemented**. This
library's Audio Gateway has a single-call model, so there is nothing here to test
them against, and an API that can only be exercised against an external phone
would ship unverified.

Related examples: [HfpClientRaw](../examples/Classic/HfpClientRaw/),
[HfpAudioGatewayRaw](../examples/Classic/HfpAudioGatewayRaw/)

## 9. Using BLE at the same time

Starting both `EspBle` and `EspBleClassic` makes a sketch dual-host, with the
broker routing HCI between them. Starting one makes the broker a pass-through.

Being able to run both does not mean unlimited concurrency. The radio, the heap
and the callback queues are shared. Watch the drop counters and the queues, and
keep the application's own queues bounded.

**A sketch that links Classic starts the controller in BTDM mode whichever host
starts first.** BLE's controller memory therefore cannot be released, so such a
sketch has less heap than one that uses BLE alone.

Dual-host is verified between EspBle boards and against the core's bundled host;
**interoperability with external devices is unverified**. If it misbehaves,
`end()` one host and use a single one.

The bundled host's configuration, how the archive is regenerated and what has
been verified are in the [handover notes](HANDOFF_ESP32_CLASSIC.ja.md) and
[Classic host archive rebuild](CLASSIC_HOST_BUILD.ja.md) (both Japanese).

## Where to go next

For a step-by-step account of Controller/Host roles, the source/`.a` layout,
HCI routing, flow control and lifecycle, see the
[dual-host technical guide](GUIDE_DUAL_HOST.md). For the SDP record budget, SPP
queue geometry and known failure signatures, see [EspBle in depth](GUIDE_ADVANCED.md).
