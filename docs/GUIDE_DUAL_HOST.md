# BLE / Bluetooth Classic Dual-Host Technical Guide

> 日本語版: [GUIDE_DUAL_HOST.ja.md](GUIDE_DUAL_HOST.ja.md)

This guide explains how EspBle shares the original ESP32's single Bluetooth
controller between the NimBLE host for BLE and the Bluedroid host for Bluetooth
Classic. It starts with the controller/host boundary, builds up from a normal
single-host system, and then explains each responsibility added by dual-host
operation. The central point is why this requires much more than splitting
packets into "BLE" and "Classic."

> **Scope:** Dual host is available only on the **original ESP32**, the only
> supported SoC with Bluetooth Classic. It activates automatically when both
> `EspBle` and `EspBleClassic` are begun; there is no build flag. It remains
> experimental. The fallback is to `end()` one stack and use a single host.

## 1. Separate the five layers first

"Host" has several meanings in Bluetooth discussions. This guide uses it for
the software stack on the controller side of HCI.

| Layer | Responsibility | EspBle example |
|---|---|---|
| Application | Product behavior | Arduino sketch, `setup()`, `loop()` |
| Profile / protocol | Meaning and procedure for data | GATT, SMP, HID, SPP, A2DP, HFP |
| Bluetooth Host | Connections, security, profiles and logical Bluetooth state | NimBLE for BLE, Bluedroid for Classic |
| HCI transport | Commands, events and data between Host and Controller | EspBle HCI broker and ESP32 VHCI |
| Bluetooth Controller | Link Layer/Baseband, timing, encryption and radio control | Original ESP32 BTDM controller |

```text
Arduino sketch
  │
  ├─ EspBle API ───────── GAP / GATT / SMP / BLE HID / BLE MIDI
  │                         │
  │                       NimBLE Host
  │                         │ logical HCI
  │
  └─ EspBleClassic API ─── Inquiry / SPP / HID / A2DP / AVRCP / HFP
                            │
                     Classic-only Bluedroid Host
                            │ logical HCI
                            ▼
                    EspBle HCI Broker
                            │ physical VHCI (H4 packets)
                            ▼
                 ESP32 BTDM Controller + Radio
```

This Bluetooth Host is not an HID Host, which is a profile role inside a
Bluetooth Host. It is also unrelated to the ESP-Hosted Host in a P4 + C6
configuration. Central/Peripheral are BLE link roles, while GATT Client/Server
are attribute roles; neither pair selects a Bluetooth Host implementation.

## 2. A normal single-host system

With one Host, NimBLE can send an HCI command to start a BLE connection, the
Controller performs the radio procedure, and the result returns as an HCI
event. GATT traffic then travels in ACL data packets.

| H4 packet | Direction | Purpose |
|---|---|---|
| Command (`0x01`) | Host → Controller | Scan, connect, encrypt, disconnect and other requests |
| ACL Data (`0x02`) | Both | BLE L2CAP/ATT/SMP and Classic L2CAP/profile data |
| SCO Data (`0x03`) | Both | Classic HFP audio data |
| Event (`0x04`) | Controller → Host | Command responses, connection state, encryption and receive notifications |

A single Host can treat every command credit and connection handle as its own.
The EspBle broker preserves that assumption in single-host pass-through mode.

## 3. Why two Hosts cannot attach directly

The ESP32 BTDM Controller can run LE and BR/EDR concurrently, but VHCI still
has one physical input and output. The Controller sees one Host, not NimBLE and
Bluedroid as separate owners. Connecting both directly creates conflicts:

1. Both can issue HCI commands against one shared command-credit budget.
2. A Command Complete carries an opcode, but no NimBLE/Bluedroid destination.
3. Disconnect, encryption and completed-packet events are shared by both radios.
4. Reset, event masks and flow control change the whole Controller.
5. Both directions consume shared Controller buffer pools.
6. Either Host might stop the Controller while the other still has a live link.

The solution must therefore own shared Controller state. A packet-type
demultiplexer alone is insufficient.

## 4. Why not use standard Bluedroid alone?

First, **standard Bluedroid can use BLE and Bluetooth Classic concurrently.**
The normal ESP-IDF configuration is one dual-mode Host containing both BLE and
Classic protocols, profiles and APIs. Because there is only one Host, that
arrangement does not need the HCI broker described in this guide.

```text
Standard Bluedroid-only arrangement

Application
   │
   └─ dual-mode Bluedroid Host
        ├─ BLE API / GAP / GATT / SMP
        └─ Classic API / GAP / SPP / HID / Audio
                     │ HCI
                     ▼
               BTDM Controller
```

Dual host was therefore not chosen because Bluedroid lacks a feature. The
reason is the desired **BLE architecture and API boundary**.

A dual-mode Bluedroid Host is broad: BLE and Classic APIs, state, callbacks and
build options all meet in one large stack. That breadth is valuable when one
Host should expose everything, but it also gives a BLE-only application more
API surface and more combinations of configuration and state to understand.

NimBLE is BLE-only. It contains no Classic APIs or state, keeping its Host
responsibility focused on BLE GAP, GATT and SMP. EspBle was built around NimBLE
from the beginning and exposes a small, consistent public API over that model.
Switching the BLE backend to Bluedroid merely to add Classic would also mean
moving the existing BLE implementation, behavior and resource model to another
stack.

EspBle instead divides responsibility this way:

1. **Keep NimBLE for BLE.** Preserve the focused BLE Host and existing EspBle API.
2. **Build Bluedroid as Classic-only and Host-only.** Exclude its BLE and
   Controller portions; retain the Classic profiles such as SPP, HID, A2DP,
   AVRCP and HFP.
3. **Connect both Hosts through an HCI broker.** Keep their internals separate
   and isolate only the shared-Controller arbitration in one layer.

```text
EspBle dual-host arrangement

Application
   ├─ EspBle API ───────── NimBLE Host (BLE-only)
   └─ EspBleClassic API ── Bluedroid Host (Classic-only)
                                   │
                          EspBle HCI Broker
                                   │ HCI
                                   ▼
                             BTDM Controller
```

This design does not enable something impossible with Bluedroid. It keeps
**BLE simple in a BLE-only NimBLE Host, while using Classic-only Bluedroid for
native Classic profiles**. The trade-off is that command, event, buffer and
lifecycle sharing—which one dual-mode Bluedroid Host could solve internally—
must now be implemented correctly by the EspBle broker.

## 5. How EspBle supplies two Hosts

On the original ESP32, EspBle brings its own BLE and Classic Hosts:

| Purpose | Host | Distribution form |
|---|---|---|
| BLE | Bundled NimBLE | Source under `src/nimble_esp32/` |
| Classic | Classic-only Bluedroid | `src/esp32/libespble_bluedroid_classic.a` |

The Classic archive is Host-only and Classic-only: it contains no Controller.
Its defined symbols are moved into the `espble_bd_` namespace, preventing
collisions with Arduino Core symbols. Bluedroid attaches its injected HCI
driver to the broker instead of physical VHCI. The bundled NimBLE transport
also uses the broker as its logical transport.

### 5.1 What is source, and what is a prebuilt `.a`

- The **EspBle library itself**—public APIs, profiles, broker, router,
  scheduler, policy and ACL-credit logic—is source compiled with the sketch.
- The **NimBLE BLE Host** is also bundled as source. Its broker transport,
  attach-to-an-existing-controller path and original-ESP32 changes remain
  reviewable in source and regeneration scripts.
- Only the **Classic-only Bluedroid Host** is a prebuilt static archive,
  `libespble_bluedroid_classic.a`.
- The **BTDM Controller** is neither of those. EspBle uses the prebuilt
  original-ESP32 Controller supplied by Arduino-ESP32 Core through VHCI.

```text
compiled with the sketch                     built in a pinned environment
┌──────────────────────────────┐          ┌─────────────────────────┐
│ EspBle / EspBleClassic API   │          │ Classic-only Bluedroid  │
│ HCI broker components        │          │ namespaced static .a    │
│ bundled NimBLE source        │          └────────────┬────────────┘
└──────────────┬───────────────┘                       │ link
               └──────────────────┬────────────────────┘
                                  ▼
                         Arduino sketch ELF
                                  │ VHCI
                                  ▼
                    Core's prebuilt BTDM Controller
```

### 5.2 Why the two Hosts use different forms

Source distribution suits NimBLE because target conditions and local transport
patches are straightforward to track and regenerate. Bluedroid depends heavily
on ESP-IDF Kconfig, generated headers and several components; compiling all of
that inside every Arduino build would make configuration reproducibility and
build time much worse. EspBle therefore builds the Host-only, Classic-only
archive in a pinned ESP-IDF v5.5.5 / GCC 14.2.0 environment.

Namespacing prevents an EspBleClassic call from resolving to Core's Bluedroid.
Stable external dependencies such as FreeRTOS, NVS, timers and logging remain
unrenamed and resolve from Core at final link. A static archive is not copied
wholesale into flash: the linker selects only members needed by the profiles in
use. Because the archive has an ABI contract, measured Core compatibility,
required-symbol link checks, and clean reproducible SHA-256-identical builds
are release gates. See [rebuilding the Classic host archive](CLASSIC_HOST_BUILD.ja.md)
(Japanese).

## 6. From `begin()` to routed mode

Startup works in either BLE→Classic or Classic→BLE order.

1. **Declare future Classic presence.** The integration layer tells the broker
   that the Classic Host is linked before `setup()`. Releasing Classic
   Controller memory is irreversible, so BLE must preserve it if Classic may
   start later.
2. **Start the Controller once.** The first Host selects BLE mode for BLE-only,
   Classic mode where a truly Classic-only build is known, or BTDM when both
   may run. It transfers shutdown responsibility to the broker.
3. **Register the first logical Host.** The broker installs the one physical
   VHCI callback and uses pass-through mode, while already tracking command and
   handle state across the transition.
4. **Attach the second Host only.** It does not reinitialize the running
   Controller. The broker creates its command-dispatch task, enters routed
   mode, and takes ownership of Controller-to-Host ACL flow control.
5. **Run profiles independently.** NimBLE owns BLE state and Bluedroid owns
   Classic state. The application calls both `update()` methods.

```cpp
#include <EspBle.h>
#include <EspBleClassic.h>

EspBle ble;
EspBleClassic classic;

void setup() {
  EspBleConfig bleConfig;
  bleConfig.deviceName = "Dual Device";
  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "Dual Device";

  if (!ble.begin(bleConfig)) {
    Serial.println(ble.lastErrorDetail());
    return;
  }
  if (!classic.begin(classicConfig)) {
    Serial.println(classic.lastErrorDetail());
    ble.end();
    return;
  }
  // Start BLE advertising/GATT and Classic profiles normally here.
}

void loop() {
  ble.update();
  classic.update();
}
```

Linking Classic preserves BTDM memory even before Classic is begun, so it costs
more heap than a BLE-only sketch. There is no special dual-host `begin()` and
no build flag.

## 7. Host to Controller: serialize commands safely

### Step 1: authorize the command

In routed mode every observed command opcode has an explicit scope:

| Scope | Examples | Allowed owner |
|---|---|---|
| Shared read | Local version, supported features, BD_ADDR | Either |
| NimBLE radio / connection | LE scan, advertising, encryption | NimBLE |
| Classic radio / connection | Inquiry, scan mode, pairing, SCO | Bluedroid |
| Shared connection | Disconnect, remote version, RSSI | Host owning the handle |
| Controller merged | General, Page 2 and LE event masks | Union of both requests |
| Controller virtual | Reset, Host Buffer Size, flow-control setup | Broker substitutes behavior |
| Host credit | Host Number Of Completed Packets | Broker-owned |

Unknown opcodes, wrong-Host opcodes and commands targeting another Host's
handle fail closed before physical transmission. This affects routed mode only;
single-host pass-through remains unchanged. A new profile that fails only with
both Hosts often needs a newly observed opcode classified and tested.

### Step 2: merge or virtualize Controller-wide commands

The broker caches each Host's General, Page 2 and LE event masks and sends their
bitwise union. Otherwise the last Host to set a mask could silence events the
other requires.

A reattaching Bluedroid normally sends HCI Reset, which would destroy live LE
links. The broker does not send that Reset physically; it returns a successful
Command Complete only to the requesting Host. Host Buffer Size and
Controller-to-Host flow-control setup are likewise replaced by broker-owned
configuration.

### Step 3: copy commands into a bounded FIFO

The full packet is copied into a 16-entry broker-owned FIFO, so caller buffer
lifetime is irrelevant. A full FIFO rejects the command with `ESP_ERR_NO_MEM`
and increments diagnostics.

### Step 4: run one physical transaction at a time

The dispatch task checks `Num_HCI_Command_Packets` credit and VHCI availability.
It conservatively permits one in-flight transaction and records `opcode + owner`.

### Step 5: return the response to its owner

Command Complete or Command Status is matched against the in-flight opcode and
delivered only to the requesting Host. A mismatch increments
`command_response_mismatch`.

## 8. Protect data with connection-handle ownership

An HCI ACL header contains a 12-bit connection handle. The broker records its
owner on successful connection events:

| Controller event | Owner |
|---|---|
| LE Connection Complete / Enhanced Connection Complete | NimBLE |
| BR/EDR Connection Complete | Classic Bluedroid |
| Synchronous Connection Complete (SCO/eSCO) | Classic Bluedroid |

Outgoing ACL is accepted only from the Host owning that handle. Incoming ACL
goes to only that Host. Disconnection Complete is routed before the handle is
removed. SCO is Classic-only and ISO is NimBLE-only. The current routing table
holds up to 16 connections (`ESPBLE_HCI_ROUTER_MAX_CONNECTIONS`).

## 9. Controller to Host: classify events

| Event class | Destination |
|---|---|
| LE Meta Event | NimBLE |
| Classic-specific asynchronous event | Classic Bluedroid |
| Command Complete / Status | Host that issued the command |
| Disconnect, authentication, encryption, remote feature/version, mode change | Handle owner |
| Hardware Error / Data Buffer Overflow | Both |
| Number Of Completed Packets | Filtered and rebuilt per Host by handle |

One Number Of Completed Packets event may contain both LE and Classic handles.
Broadcasting it would corrupt each Host's accounting, so the broker constructs
a separate event containing only the records owned by each recipient. Shared
disconnect and encryption events are why "LE events to NimBLE, everything else
to Classic" is not sufficient.

## 10. ACL flow control in both directions

### Host → Controller

Both Hosts share Controller transmit buffers. Number Of Completed Packets
events announce freed buffers and are split by handle. EspBle does not reserve
or fairly partition those buffers between Hosts, so heavy traffic from one can
temporarily delay the other.

### Controller → Host

For the reverse direction, a Host returns consumed buffers with Host Number Of
Completed Packets. In a shared setup, Classic Bluedroid can credit only Classic
ACL routed to it, while the bundled NimBLE does not return this credit. Because
the Controller pool is shared, LE traffic would eventually drain the entire
pool and stall both radios.

The broker therefore learns buffer geometry from Read Buffer Size, counts ACL
delivered across both Hosts by handle, and generates the credit commands itself.
Credits normally batch at a threshold of four, but any remainder is flushed
when the queue drains. Pending credit for a disconnected handle is discarded
because the Controller frees those buffers itself.

## 11. Startup, shutdown and reattachment

The Controller outlives either individual Host:

1. The Host that starts it transfers shutdown responsibility to the broker.
2. Ending one Host keeps the Controller alive while the other remains.
3. Only removal of the final Host disables and deinitializes the Controller.
4. Unsent commands and event-mask requests from a removed Host are discarded.
5. A session generation prevents old queued commands reaching a restarted Controller.
6. Receive gates prevent delayed delivery into a stopped Host.

NimBLE's receive path remains gated until its Host task is ready. Classic
profile setup is asynchronous, so command state is preserved while registration
changes from one Host to two. Bluedroid can later reattach while a BLE link is
alive because its bootstrap Reset and flow-control commands receive virtual
completion instead of resetting the physical Controller.

## 12. Shared state versus independent state

| Shared | Independent per Host |
|---|---|
| One radio and BTDM Controller | BLE GAP / GATT / SMP state |
| HCI command credit | Classic GAP / SPP / HID / audio state |
| Controller ACL/SCO buffers | BLE bonds and Classic bonds |
| Physical event-mask settings | Profile callbacks and connections |
| Heap, CPU time and radio time | Each object's `update()` event queue |

BLE and Classic bonds are separate key stores; deleting one does not delete the
other. Concurrent support does not mean unlimited parallel throughput.

## 13. Reading diagnostics

`espble_hci_broker_get_diagnostics()` in `EspBleHciBroker.h` returns a snapshot
for the current Controller session. This is an internal experimental boundary.

```cpp
#include <EspBleHciBroker.h>

espble_hci_broker_diagnostics_t d = {};
espble_hci_broker_get_diagnostics(&d);
Serial.printf("qmax=%u qfull=%lu mismatch=%lu unknown=%lu credits=%lu/%lu\n",
  d.command_queue_high_water,
  static_cast<unsigned long>(d.command_queue_full),
  static_cast<unsigned long>(d.command_response_mismatch),
  static_cast<unsigned long>(d.unknown_acl),
  static_cast<unsigned long>(d.acl_credits_returned),
  static_cast<unsigned long>(d.acl_credits_dropped));
```

| Field | Meaning | Normal interpretation |
|---|---|---|
| `command_enqueued[]` / `command_sent[]` | Per-Host queued / physical commands | Equal after traffic settles |
| `command_queue_high_water` | Maximum FIFO occupancy | Below 16 leaves capacity |
| `command_queue_full` | Commands rejected by a full FIFO | Normally 0 |
| `command_response_mismatch` | Unexpected response opcode | Must be 0 |
| `command_unregister_busy` | Host removed with an in-flight response | Normally 0 |
| `unknown_acl` | ACL received for an unowned handle | Must be 0 |
| `tx_acl[]` / `rx_acl[]` / `completed_acl[]` | Per-Host ACL counters | Locate a stalled direction |
| `event_mask_unions` | Mask commands rewritten to a union | Expected in dual-host use |
| `virtual_resets` | Resets completed without physical transmission | Expected on Classic reattach |
| `acl_flow_control_owned` | Broker receive-credit loop active | 1 in a routed session |
| `acl_credits_returned` / `dropped` | Returned / invalidated receive credits | Correlate drops with disconnects |

For both Hosts stalling, inspect queue, mismatch, credit and heap counters. For
one side stalling, inspect unknown ACL and per-Host counts. For a new profile
that fails only in dual-host mode, look for `unclassified` or `wrong host` HCI
opcodes. If ending one Host kills the other link, inspect final-Host detection
and Controller ownership.

## 14. Current limitations

- Original ESP32 only; other supported SoCs have no Bluetooth Classic radio.
- Dual host remains experimental, with less external-device interoperability
  coverage than EspBle-to-EspBle and Core-host testing.
- Host-to-Controller ACL buffers are not partitioned fairly between Hosts.
- HCI policy fails closed until every new opcode is classified and tested.
- Linking Classic preserves BTDM memory and costs more heap than BLE-only use.
- Radio, heap, CPU and application callback queues are shared resources.
- The measured Classic archive Core range is Arduino-ESP32 3.2.0–3.3.11; HFP
  audio requires 3.3.8 or newer.

The first isolation step is to `end()` one Host. If the problem disappears in
single-host mode, prioritize broker policy, shared resources and lifecycle.

## 15. Verification coverage

Hardware peer tests cover Classic HID plus repeated GATT, BLE pairing/bonding
and RPA reconnection, simultaneous commands, FIFO overflow recovery, consecutive
LE/BR-EDR disconnects, arbitrary shutdown/destructor order, Classic reattach,
HFP SLC and bidirectional mSBC SCO during GATT, A2DP/AVRCP during GATT, peer loss,
bad passkeys and failed connections.

The representative suites are `tests/peer/dual_host_smoke/`, `dual_host_rpa/`,
`dual_host_hfp/` and `dual_host_a2dp/`. Long-running results and acceptance
counters are in the [technical validation record](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)
(Japanese).

## 16. Reading the implementation

| Order | File | Responsibility |
|---:|---|---|
| 1 | [`EspBleHciBroker.h`](../src/EspBleHciBroker.h) | Logical Host API and diagnostics |
| 2 | [`EspBleHciRouter.c`](../src/EspBleHciRouter.c) | H4 parsing, responses, handles and event routing |
| 3 | [`EspBleHciCommandScheduler.c`](../src/EspBleHciCommandScheduler.c) | 16-entry FIFO, command credit and in-flight state |
| 4 | [`EspBleHciControllerPolicy.c`](../src/EspBleHciControllerPolicy.c) | Opcode scopes, mask union and virtual commands |
| 5 | [`EspBleHciAclCredits.c`](../src/EspBleHciAclCredits.c) | Controller-to-Host ACL credit accounting |
| 6 | [`EspBleHciBroker.c`](../src/EspBleHciBroker.c) | VHCI, FreeRTOS task and integration |
| 7 | [`EspBleClassic.cpp`](../src/EspBleClassic.cpp) | Namespaced Bluedroid attach and startup order |
| 8 | [`esp_nimble_hci.c`](../src/nimble_esp32/src/esp-idf/esp_nimble_hci.c) | NimBLE-to-broker transport |

Router, scheduler, policy and credit arithmetic are platform-independent C, so
they can be unit tested without a Controller. The broker alone knows ESP-IDF,
VHCI and FreeRTOS; Arduino link state and `begin()` stay in the integration layer.

## 17. The complete mental model

1. Standard Bluedroid alone can run BLE and Classic concurrently.
2. EspBle keeps BLE-only NimBLE to preserve a small, clear BLE API and model.
3. Classic is delegated to a Classic-only, Host-only Bluedroid build.
4. The ESP32 has one physical Controller and radio.
5. The two Hosts share command credit, handles, buffers, settings and lifecycle.
6. The broker is the only physical VHCI owner and centralizes shared state.
7. It authorizes and serializes commands, then returns each response to its owner.
8. It routes data and shared events using connection-handle ownership.
9. It owns receive flow control and Controller lifecycle instead of either Host.
10. The application uses both normal APIs and calls both `update()` methods.

For product-level selection, see [Choosing between BLE and Bluetooth Classic](CLASSIC_VS_BLE.md).
For profile concepts, see the [BLE](GUIDE_BLE_BASICS.md) and
[Classic](GUIDE_CLASSIC_BASICS.md) beginner guides. For broader behavior under
load and failure, see [EspBle in depth](GUIDE_ADVANCED.md).
