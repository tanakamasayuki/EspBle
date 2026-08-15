# EspBle in depth

> 日本語版: [GUIDE_ADVANCED.ja.md](GUIDE_ADVANCED.ja.md)

The beginner guides ([BLE](GUIDE_BLE_BASICS.md), [Classic](GUIDE_CLASSIC_BASICS.md))
explain what the radios do and which call to reach for. This document is for the
next question: **how the library behaves under load, at its limits, and when
something goes wrong.** It assumes you have a sketch that works and now need to
know what it will do with twelve subscriptions, a full transmit queue, or a peer
that vanishes mid-operation.

Every number here comes from the source and is named with the constant it lives
in, so you can check it rather than trust it.

## 1. Where your callbacks run

EspBle has three kinds of execution context, and knowing which one a callback
runs on decides what you may do inside it.

| Context | What runs there | Lifetime |
|---|---|---|
| Your task (the one calling `update()`, normally `loop()`) | **Almost every application callback** — `onConnected()`, `onWritten()`, `onInputReport()`, `onData()`, and the rest | Yours |
| The host task | The NimBLE (or Classic Bluedroid) host's own processing, plus the few callbacks listed below that cannot be deferred | Created by the backend at `begin()` |
| A transient worker task | One blocking backend operation, then it exits | Per operation |

**The exceptions run on the host task, and each one has a reason:**

| Callback | Why it cannot wait for `update()` |
|---|---|
| `gattServer().onRead()` | The value has to exist before the ATT read transaction completes. Call `setValue()` here and that is what the peer receives. It is a single observer rather than a listener list, because only one owner can fill in a value |
| `a2dpSink().onMedia()`, HFP `onAudio()` | The payload is a read-only view into the backend's buffer, valid only until the callback returns. Copy what you need; the backend frees it immediately after |
| Passkey and numeric-comparison requests | The pairing procedure is waiting. EspBle yields while your `loop()` answers through `providePasskey()` / `confirmNumericComparison()`, and rejects the pairing if the timeout elapses first |
| BLE MIDI parser callbacks | Pointers into the packet being parsed are valid only for the duration of the callback |

Inside those four, do not print, do not block, and do not call back into
`begin()` / `end()`. Everything else can wait for the next `update()`.

`EspBle::update()` and `EspBleClassic::update()` are where the queues drain:
`update()` expires finite advertising and scans, times out a stalled GATT
operation, pumps the GATT operation and server-send queues, releases deferred
notifications, dispatches scan results and connection events, drives
auto-reconnect, and dispatches HID device and host events. Apart from the four
exceptions above, nothing is delivered to your code outside those calls.

Three consequences:

- **A callback that blocks stalls everything**, including the rest of that
  `update()`. There is no second dispatcher to pick up the slack.
- **You do not need locks around your own sketch state** as long as you only
  touch it from `update()`-dispatched callbacks and `loop()` — they are the same
  task. State shared with one of the four host-task callbacks above does need
  care.
- **A `loop()` that skips `update()` looks like a dead radio** even though the
  host task is still connected and buffering. If you gate `update()` behind a
  timer, the queues in section 2 are what fill up.

The transient workers exist because some backend calls block. They are created
per operation, with a fixed stack, at priority 1, and they exit when the
operation completes: `espble-gatt` (6144 bytes) for a client GATT operation,
`espble-gatt-send` (4096) for a server notify/indicate, and `espble-hid-host`
(16384 — descriptor parsing needs the room) for HID host discovery. If task
creation fails, the operation still reports a result through its normal callback
with `ResourceExhausted`, so a low-memory moment never leaves a caller waiting
for a callback that will not come.

The NimBLE host reports when the controller is ready, and nothing may touch GAP
before that. `begin()` waits for it, which is why `begin()` is not instantaneous.

## 2. Capacities, and what happens when they fill

Every queue and table in EspBle is fixed size. That is deliberate: a library that
allocates without bound turns a busy moment into a heap failure somewhere else in
the sketch. The cost is that each limit has a behaviour you should know before
you meet it.

### BLE

| Limit | Value | Constant | What happens at the limit |
|---|---|---|---|
| Simultaneous connections | 4 slots, but the controller decides | `ConnectionCapacity`, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` (3 on the bundled builds) | A connect beyond the controller's limit fails at the backend even though a slot is free |
| GATT client operations | 1 in flight + 8 queued | `GattQueueCapacity` | `ResourceExhausted`, refused before it reaches the air |
| Server notify/indicate | 8 queued | `SendQueueCapacity` | `ResourceExhausted` through the send result |
| Active client subscriptions | 16 | `ClientSubscriptionCapacity` | `subscribe()` fails without writing the CCCD |
| Persistent subscription records | 16 | `PersistentSubscriptionCapacity` | The subscription still succeeds; only the record is lost, counted by `droppedPersistentSubscriptionCount()` |
| Deferred notifications | 4 | `DeferredNotificationCapacity` | Held only to keep ordering against a GATT completion |
| Scan results awaiting dispatch | 16 | `ScanQueueCapacity` | The newest result is dropped and counted; call `update()` more often rather than expecting a backlog to be kept |
| Connection events awaiting dispatch | 8 | `ConnectionEventQueueCapacity` | A lifecycle or completion event evicts the oldest queued notification rather than being lost itself; with no notification to evict, the new event is dropped. Either way `droppedEventCount()` counts it |
| Listeners per event | 4 + the primary `on*()` | `EspBleCallbackList<Callback, 4>` | `addXListener()` returns no id; existing listeners are never evicted |
| Bonds | `CONFIG_BT_NIMBLE_MAX_BONDS` (16 when undefined) | `BondCapacity` | The backend's own replacement policy applies |
| Discovered attributes | 16 services, 48 characteristics, 48 descriptors | `MaxDiscoveredGatt*` | Discovery stops recording and **fails** with `ResourceExhausted` rather than reporting a partial database as complete |
| Advertising payload | 31 bytes each for advertising and scan response | `EspBleAdvertising::Payload::Capacity` | The field that does not fit is refused with a reason |
| Accept list | 8 | `MaxAcceptListEntries` | Further entries refused |
| Custom HID reports | 4 | `EspBleHidCustom::MaxReports` | Refused |
| Auto-rediscover peers remembered | 4 | `MaxRediscoverPeers` | Oldest forgotten |

Two of these deserve emphasis because they are the ones a working sketch hits
first.

**The GATT operation queue is one deep in flight.** Subscribing to twelve
characteristics in a `for` loop does not work: the first goes out, eight queue,
and the rest come back `ResourceExhausted` before touching the radio. Issue them
one at a time from the completion callback of the previous one. This is also why
the peer test that fills the persistent-subscription registry subscribes serially
— otherwise it would be testing the queue, not the registry.

**A full persistent-subscription registry is silent by design.** `subscribe()`
succeeds, data flows, and only reconnection reveals that the subscription was not
restored. `droppedPersistentSubscriptionCount()` is the only evidence, which is
exactly why it exists; check it in any sketch that subscribes across more than a
handful of peers.

### Classic

| Limit | Value | Constant | What happens at the limit |
|---|---|---|---|
| SPP payload per write | 990 bytes | `EspBleClassicSpp::MaximumWriteSize` | Larger writes are split by `EspBleClassicSppStream`; the raw API refuses |
| SPP writes queued per session | 8 | `WriteQueueCapacity` | The write reports how much it took; `Stream::write()` waits up to `setWriteTimeout()` |
| SPP receive buffer per session | 2048 bytes | `ReceiveBufferCapacity` | Unread bytes are dropped and counted (`droppedReceiveByteCount()`) |
| SPP services per device | 4 | `EspBleClassicSpp::MaximumServers` | `startServer()` refuses |
| HID report length | 1024 bytes | `MaximumReportLength` | `InvalidArgument` before transmission |
| HID descriptor + profile strings | 214 bytes | `MaximumSdpRecordPayload` | `begin()` refuses with `ResourceExhausted` |
| Remote services per query | 12 | `MaximumServices` | Extra UUIDs not reported |
| AVRCP notifications declared | 8 | `MaximumNotifications` | Refused |
| Page timeout | 14..40959 ms (default 5120) | checked in `setPageTimeout()` | `InvalidArgument`, refused locally |
| Transmit power | -12..+9 dBm in 3 dB steps | `ClassicTxPowerLevels` | Rounded to the nearest supported level |

The 214-byte SDP budget is the one limit that changes what you can build rather
than how fast you can run. The composed Report Descriptor and the `name`,
`description` and `provider` strings share a 300-byte pad
(`CONFIG_BT_SDP_PAD_LEN`) with the record's standard attributes, which take 86
bytes. With the default strings, keyboard + mouse + consumer fits at 201 and
adding the gamepad does not at 215 — measured on hardware, not estimated. BLE has
no equivalent limit because the Report Map is a characteristic, read like any
other value.

## 3. Accepted is not applied

Most EspBle calls that reach the radio return whether the request was **accepted**
— arguments valid, state legal, request handed to the backend — and report what
actually **happened** through a callback. Treating the return value as the outcome
is the most common way to write a sketch that works on the bench and fails in the
field.

Calls that are round trips, each with its own callback: GATT read, write,
subscribe and discovery; server notify and indicate; MTU negotiation
(`preferredMtu` defaults to 247, the live value starts at 23 and arrives through
`onMtuChanged()`); security requests; HID host report requests, protocol-mode and
idle-rate changes and virtual cable unplug; Classic name and service queries;
A2DP delay set and get; every AVRCP command; every HFP command.

The distinction matters for verification too. `setPageTimeout()` returning true
proves nothing about the controller having taken the value, so the peer test for
it measures **how long a connection attempt to a dead address takes** — under
three seconds at 1000 ms, at least a second longer at the default 5120 ms. When
you add a setting of your own, prefer a check with that shape.

Where the backend gives no completion, EspBle says so rather than inventing one.
Where the backend reports success but the operation did not happen — an SDP
record that overflowed its pad, an AT exchange left without its terminating OK —
the library closes the gap itself, because a silent backend failure is
indistinguishable from a working device until a Host refuses to talk to it.

## 4. The error model

`EspBleError` has six values, and EspBle uses them consistently enough that the
value tells you where to look.

| Value | Means | Where the fix is |
|---|---|---|
| `InvalidState` | The call is legal but not now — not started, not connected, wrong role, a conflicting operation in flight | Sequencing in your sketch |
| `InvalidArgument` | The arguments cannot produce a valid request; refused before transmission | Your call site |
| `ResourceExhausted` | A fixed capacity from section 2 is full, or a worker task could not be created | Pacing, or fewer simultaneous requests |
| `NotFound` | The named thing does not exist — unknown handle, unknown UUID, unknown connection or session id | Your identifiers, or a peer that changed |
| `Timeout` | Accepted, sent, and nothing came back in the allotted time | The peer or the radio environment |
| `BackendFailure` | The host stack refused or failed, with its own status carried in the detail string | The detail string, then this table's other rows |

`BackendFailure` always carries a message. Log it — the difference between "the
controller rejected this because flow control is already enabled" and "the peer
disconnected" lives in that string, not in the enum.

Classic audio has its own result type because a media send is a hot path where
"try again shortly" is a normal answer rather than an error:
`EspBleClassicAudioSendResult` is `Accepted`, `WouldBlock`, `InvalidState`,
`InvalidArgument`, `TooLarge` or `BackendFailure`.

## 5. Backpressure and throughput

Anything that streams has a finite queue, and the correct response to a full one
is always the same shape: **stop producing, keep calling `update()`, retry.**

- **GATT client operations**: one in flight. Chain from the completion callback.
- **Server notify/indicate**: eight queued, drained by `update()`. A burst larger
  than that comes back `ResourceExhausted`; the queue does not grow.
- **SPP**: 990 bytes per packet, eight packets queued. One `write()` becomes one
  packet, so a line is much cheaper than a character at a time. `Stream::write()`
  waits up to `setWriteTimeout()` (1000 ms by default, 0 to never wait) and
  returns how much it took, exactly like a `Serial` that ran out of buffer.
  `flush()` returns when `pendingWriteCount()` reaches zero.
- **A2DP Source**: `WouldBlock` is the normal signal that the transport is busy.
  A 20,000-packet transfer completes with a non-zero `WouldBlock` count and no
  loss — the retry path is part of normal operation, not an error path.
- **HFP SCO**: frames arrive and must be sent on the codec's cadence (57-byte
  mSBC frames). There is no queue to hide a late producer; a missed frame is a
  gap in the audio.

MTU is the other half of BLE throughput. `preferredMtu` (247 by default) is a
request; until `onMtuChanged()` fires you have 23 bytes, of which 20 are payload
for a notification. Sizing a packet at 244 bytes and sending it during connection
setup is a common self-inflicted failure.

## 6. Identity, bonds and coming back

Reconnection is the area where BLE's identity model surprises people, so it is
worth stating the pieces separately.

- **Connection ids are stable for the life of the connection and not reused
  while it is live.** They are the library's identifier, not the controller's
  handle; do not persist them.
- **A per-connection cache keeps discovery results and subscription state
  separate** between simultaneous connections, so two peers offering the same
  UUIDs cannot be confused for each other.
- **Persistent subscriptions** record what a peer was subscribed to and re-apply
  it after reconnection — 16 records, dropped ones counted (section 2).
- **Auto-reconnect** (`setAutoReconnect()`, off by default) retries every 2000 ms
  (`ReconnectIntervalMilliseconds`) after an unexpected drop, driven from
  `update()`.
- **HID host rediscovery** is separate (`setAutoRediscover()`, off by default,
  remembering 4 peers) because the HID host does not use the generic subscription
  registry. It skips the automatic pass if your sketch already called
  `discover()`.
- **Address privacy** decides whether reconnection can work at all. With
  `ResolvablePrivate`, the peer's address changes periodically (every 900 s on the
  original ESP32's bundled host), and only a bonded peer holding the IRK can
  resolve it. RPA without bonding is an address that nobody can follow.
- **A BLE bond and a Classic bond are different objects.** Removing one leaves
  the other. On the original ESP32 both live in NVS, and a sketch that clears
  bonds should be explicit about which radio it means.

## 7. Running BLE and Classic together

On the original ESP32 both hosts share one controller, and an HCI broker sits
between them. There is no build flag: one registered host is a pass-through, and
starting both `EspBle` and `EspBleClassic` makes the broker route. This mode is
**experimental** — the fallback is to `end()` one host — but if you are using it,
these are the parts worth understanding.

What the broker owns, and therefore what neither host controls any more:

- **The command FIFO and its scheduling** — 16 entries
  (`ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY`), one transaction in flight, released
  by the controller's own `Num_HCI_Command_Packets` credit, with a high-water mark
  and a full counter.
- **Command response routing** back to the host that issued the opcode, and ACL
  routing by connection handle ownership.
- **The event mask**, formed as the union of both hosts' requests, so neither
  host's mask can silence the other's events.
- **Controller lifecycle**: whichever host started the controller hands shutdown
  to the broker, so either host may stop, or be destroyed, first.
- **Virtual completions** for a reattaching Classic host's HCI Reset and
  host-flow-control setup, because resetting a controller that an active LE link
  depends on would take the link down.
- **Controller-to-host ACL flow control**, which neither host can do on a shared
  controller: Bluedroid credits only its own traffic and the bundled NimBLE
  credits none, which drains the controller's buffers and stalls both transports.

The policy is **fail closed**: every HCI opcode observed on hardware is
classified by transport and scope, and an unclassified opcode, or one issued by
the host that does not own it, is refused before it reaches the controller — in
dual-host mode only, so single-host behaviour is untouched. If you extend the
library and a command starts failing only when both hosts run, an unclassified
opcode is the first thing to check.

For diagnosis, `espble_hci_broker_get_diagnostics()` in
[`EspBleHciBroker.h`](../src/EspBleHciBroker.h) fills a struct with per-host
enqueue and send counts, the queue high-water mark, `command_queue_full`,
`command_response_mismatch`, `command_unregister_busy`, ACL credits returned and
dropped, whether the broker owns flow control, per-host security event summaries,
and a per-host inventory of the opcodes actually seen. The peer tests assert on
these counters, and a sketch can print them the same way.

One thing the broker does **not** do: apportion outgoing ACL buffers between the
hosts. Each host sizes its own traffic against the controller as though it were
alone, so neither can account for the other's.

## 8. Footprint, and how to measure yours

EspBle's cost is dominated by which host you link, not by how many features you
call.

- **On the native-controller targets** (S3 / C3 / C6 / H2) the NimBLE host comes
  from the core, and EspBle adds its own code only.
- **On the original ESP32** the core's prebuilt libraries are Bluedroid, so
  EspBle bundles a NimBLE host for that chip (`src/nimble_esp32/`). That is the
  single largest addition on that target.
- **Classic** links a separately built, namespaced Classic-only Bluedroid archive
  (`src/esp32/libespble_bluedroid_classic.a`). The archive on disk is large
  (~4.6 MB), but only the members the linker needs reach flash, so what your
  sketch pays depends on which profiles you start.
- **Names matter for size**: every Classic call in the library's sources goes
  through a `espble_bd_`-prefixed macro. That is what keeps the core's own
  Bluedroid out of the link. A single unprefixed call pulls a second Bluedroid in
  and costs roughly half a megabyte, which a unit test now guards against.

Measure rather than assume:

- `arduino-cli compile` prints program and global-variable sizes per sketch. Diff
  two profiles of the same sketch to price a feature.
- At runtime, `ESP.getFreeHeap()` / `ESP.getMinFreeHeap()` around `begin()` and
  around your first connection separate steady-state cost from peak cost. The
  A2DP peer tests report exactly this (baseline, current, minimum, largest block)
  and it is a good pattern to copy.
- Remember the transient workers in section 1: peak heap includes a 16 KB stack
  during HID host discovery.

## 9. A debugging playbook

Real defects found on hardware, with the signature that identifies each. If you
are seeing one of these, the cause is known.

| Symptom | Cause |
|---|---|
| A Classic HID device pairs, but the Host never sees a usable device | The SDP record overflowed its pad. `begin()` now refuses this with `ResourceExhausted` — if you are on older code, count descriptor plus strings against 214 bytes |
| A HID host receives nothing from a device that uses report IDs | The transport puts the report ID in front of the payload while descriptor offsets are payload-relative. Fixed; if you parse reports yourself, mind the same offset |
| An HFP peer sends one AT command and then goes quiet | An unknown-AT response without its terminating OK leaves the exchange open, so the peer's next command stays queued forever. `respondToUnknownAt()` closes it |
| Classic pairing is always Just Works despite a configured IO capability | Secure Simple Pairing only involves the application when a service demands it. The service requirement now follows the configured security |
| `subscribe()` fails partway through a loop with `ResourceExhausted` | The GATT queue (1 + 8). Chain from completion callbacks |
| Subscriptions do not come back after reconnection | Registry full — check `droppedPersistentSubscriptionCount()` |
| Notifications stop but the connection is alive | `update()` is not being called often enough, or a callback blocks inside it |
| A command works with one host started and fails with both | Dual-host fail-closed policy: unclassified or wrong-host opcode |
| A test waits for a line the sketch prints at boot and times out | The serial monitor attached after the reset. Ask the sketch for the line with a command instead — the `probe` fixture in `tests/conftest.py` does this |

Two general habits pay for themselves: log the detail string of every
`BackendFailure`, and print the dropped counters
(`droppedEventCount()`, `droppedPersistentSubscriptionCount()`,
`droppedReceiveByteCount()`) when something behaves as though data went missing,
because those counters exist precisely for the failures that are otherwise
invisible.

## 10. Extending EspBle, and what is deliberately absent

Extension points that are part of the public API:

- **Arbitrary HID Report Descriptors** through `EspBleHidCustom` (4 reports) or
  the vendor report path, on both BLE and Classic, composed into the same HID
  service as the fixed profiles. Report IDs must be unique and must avoid the
  fixed profiles' reserved 1..6.
- **GATT servers built from raw attribute tables**, which is what lets EspBle
  publish two services with the same UUID, or two characteristics with the same
  UUID inside one service, each with its own handle.
- **Attribute-handle addressing** everywhere UUIDs are accepted, for peers whose
  UUIDs are ambiguous.
- **Raw payload paths** for Classic audio: A2DP carries already-encoded media and
  HFP carries raw SCO frames, so a codec library can sit on top without EspBle
  taking a position on PCM.

Absent on purpose, so you do not go looking:

- **No raw HCI API.** The broker's classification is what makes dual-host safe;
  an escape hatch would defeat it.
- **No codecs, PCM processing or device I/O.** That boundary is what keeps the
  Bluetooth side testable on its own.
- **No SPP over VFS.** `EspBleClassicSppStream` covers the same need without
  adding a file-descriptor path.
- **No AVRCP Target metadata or play-status transmission.** The bundled host's
  public API has no means to send it; a Target may declare volume changes only,
  which `supportedNotifications()` reports.
- **No call waiting or three-way calling (CHLD, BTRH).** The Audio Gateway here
  is a single-call model, so there would be nothing to verify an implementation
  against.
- **No more than one simultaneous HID Host device.** Supporting it changes
  existing signatures to take a per-device id.

The full inventory of what Classic exposes, what is verified, what is unverified
and what is unimplemented — with the reason in each case — is
[CLASSIC_FEATURE_INVENTORY.ja.md](CLASSIC_FEATURE_INVENTORY.ja.md) (Japanese).
For the reasoning behind the API shapes, see
[API_DESIGN.md](API_DESIGN.md) and [DECISIONS.ja.md](DECISIONS.ja.md) (Japanese).
