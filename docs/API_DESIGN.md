# API Design

> 日本語版: [API_DESIGN.ja.md](API_DESIGN.ja.md)

This document records the **rules** the public API follows. The authority for
class names and signatures is `src/EspBle.h`; how to use them is shown by
[../examples/](../examples/) and [GUIDE_BLE_BASICS.md](GUIDE_BLE_BASICS.md).
**No usage examples belong here** — kept in the header, the examples and this
document, three copies inevitably disagree (the samples that used to live here
had gone stale against their own signatures).

The reasoning behind each choice is in [DECISIONS.ja.md](DECISIONS.ja.md)
(Japanese), and the vocabulary in [TERMINOLOGY.ja.md](TERMINOLOGY.ja.md)
(Japanese).

## Naming

- The library root is `EspBle`. Use the standard vocabulary — Central,
  Peripheral, GATT Client, GATT Server, Connection — and avoid bare `Host` /
  `Device`, which say nothing on their own.
- Public constants and types carry an `EspBle` or `ESP_BLE_` prefix to avoid
  collisions.
- Profile types spell the role out: `HidHost`, `HidDevice`, never an abbreviation
  that drops it.

## Ownership

- The sketch owns `EspBle`.
- The scanner, the advertiser, the GATT server and every profile are
  **non-owning handles** obtained from `EspBle`.
- `EspBle` owns the lifetime of registered services, characteristics and
  descriptors.
- A connection is represented by a library handle that can tell you it has become
  invalid after a disconnect.
- Backend native objects are borrowed references; nothing guarantees they can be
  stored.

## Requests and their results are separate

- **A synchronous error at acceptance** is a `bool` return plus `lastError()` /
  `lastErrorName()` / `lastErrorDetail()` / `clearError()`. `EspBleError` is
  `None` / `InvalidState` / `InvalidArgument` / `BackendFailure` /
  `ResourceExhausted` / `NotFound` / `Timeout`.
- **Asynchronous completion or failure** is reported by each event type
  (`EspBleGattResult` and friends) through its success / error / detail fields,
  in connection and characteristic context.
- `lastError*` is single state, so calls are expected to come from one loop task
  context.
- There are no operation ids and no per-operation forced cancel.
- A backend operation that waits runs on an internal task, so the requesting API
  never blocks the loop.

## Events

- Ordinary callbacks are dispatched in `update()` context. A raw callback that
  has to run in stack context says so in its name and its documentation.
- Core GATT and connection callbacks follow a multi-observer model: **one primary
  (`on*`) plus several listeners** (`add*Listener()`, removed with
  `removeGattListener()` / `removeConnectionListener()` / `removeListener()`).
  `on*` only replaces the primary, so single-observer use is unaffected. The
  listener limit is four per owner kind.
- **A callback that must answer has a primary only** (`onPasskeyDisplayed`,
  `onNumericComparison`). Any number of observers may watch, but if more than one
  party could answer, nobody is responsible for answering.
- An event carries the connection id, the target UUID or attribute handle, the
  result and a payload as needed. Payload lifetime is stated per type.
- State getters exist for users who prefer not to use callbacks.
- Queue overflow is observed through drop counters (`droppedEventCount()`,
  `droppedResultCount()`, `droppedPersistentSubscriptionCount()`,
  `invalidInputReportCount()`) rather than a dedicated event.

## Addressing a target

- A UUID is a *type*, not a *which one*. **Anything whose UUID may legitimately
  repeat can be addressed by attribute handle.**
- On the server side, the handles returned by `addService()` /
  `addCharacteristic()` / `addDescriptor()` are what later calls take.
- On the client side, handle-taking overloads accompany the UUID-taking ones
  (characteristic read / write / subscribe / unsubscribe, descriptor read /
  write). Results report `handle` for a characteristic and `descriptorHandle` for
  a descriptor.
- UUID addressing remains as the convenient path for when that UUID is unique.

## Values and codecs

- The GATT core deals in byte sequences. Public value containers take pointer
  plus length, with a `String` overload for convenience that copies, `0x00`
  included.
- Strings, integers, Bluetooth SIG formats, HID reports, battery level and so on
  are converted by explicit codecs or profile helpers.
- **CPU endianness and C++ struct layout are never used implicitly as a wire
  format.**

## Order of configuration

- GATT server services and characteristics, and each profile's `configure()`,
  happen before `begin()`. Security permissions are fixed at registration too.
- Reconfiguration after `begin()` is not allowed; reconfigure after `end()`.
- Configuration limits are compile-time constants (server: 8 services, 32
  characteristics, 16 descriptors; discovery snapshot: 16 services, 48
  characteristics, 48 descriptors; advertising: 4 service UUIDs, 4 service-data
  blocks). Exceeding one is returned as an explicit resource error.

## Profile APIs

- A profile handle obtained from `EspBle` is `configure()`d before `begin()`, and
  `configured()` reports its state.
- Sending follows the same split: synchronous `bool` for acceptance, asynchronous
  event for the result. Where something is fired as a Write Without Response —
  HID's `setKeyboardLeds()`, for instance — the documentation states that
  acceptance is all you get and delivery is not confirmed.
- Profiles consume the generic GATT callbacks **as listeners**; they never take
  the primary for themselves.
- The details are in [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) and
  [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) (both Japanese). For writing your
  own Report Descriptor, see
  [the HID descriptor guide](GUIDE_HID_DESCRIPTORS.md).

## When extending

A new API keeps the existing boundaries: connection ids, asynchronous events,
`update()` dispatch, byte sequences, and addressing by handle. Record the
decision in [DECISIONS.ja.md](DECISIONS.ja.md), and track unimplemented features
and their priority in [FEATURE_MATRIX.md](FEATURE_MATRIX.md) and
[STATUS.md](STATUS.md).
