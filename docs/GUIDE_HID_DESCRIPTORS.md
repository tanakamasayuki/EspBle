# Writing a HID Report Descriptor

> 日本語版: [GUIDE_HID_DESCRIPTORS.ja.md](GUIDE_HID_DESCRIPTORS.ja.md)

The HID specs in this repository ([device](HID_DEVICE_SPEC.ja.md),
[host](HID_HOST_SPEC.ja.md), both Japanese) describe what EspBle implements. This
guide is the practical companion: **how to write a Report Descriptor of your own,
what the bytes have to line up with, and how to prove it works** before blaming
the Host.

A Report Descriptor is the only thing that tells a Host how to interpret your
reports. Get it wrong and there is no error: a Host either ignores your device,
or reads your fields at the wrong offsets and does something surprising. That is
why the verification section matters as much as the syntax.

## 1. Decide which route you need

EspBle offers three, and the first one that fits is the right one.

| Route | Use it when | What you write |
|---|---|---|
| Fixed profiles — `hidKeyboard()`, `hidMouse()`, `hidConsumerControl()`, `hidSystemControl()`, `hidGamepad()` | The device is one of those things | Nothing. The descriptor is composed from the profiles you configure, and the same module produces it on BLE and Classic |
| `hidVendor()` | You want a private data pipe to your own application, not a device a Host understands | Nothing. A vendor-defined report of `EspBleHidVendorConfig::reportSize` bytes (63 by default) |
| `hidCustom()` | The device is something a Host must understand and it is not one of the fixed profiles — a dial, a pedal, a control panel, an unusual composite | The raw descriptor bytes, plus one `addInputReport()` / `addOutputReport()` / `addFeatureReport()` declaration per report |

`hidCustom()` composes into the same HID service as the fixed profiles, so a
custom report can sit next to a keyboard. Two rules follow from that:

- **Report IDs must be unique**, and while a fixed profile is enabled its
  reserved IDs (1..6) belong to it. With no fixed profile enabled you may use
  them.
- **Up to four custom reports** (`EspBleHidCustom::MaxReports`).

Whatever you declare with `addInputReport()` must match the descriptor. The
declaration is what sizes the characteristic and routes `sendInput()`; the
descriptor is what the Host reads. Nothing cross-checks them for you.

## 2. What the bytes are

A descriptor is a flat sequence of items. Each item starts with a prefix byte
encoding its tag and how many data bytes follow, so a descriptor is read
strictly left to right, and **global items stay in effect until changed**.

The items you will actually use:

| Bytes | Item | Role |
|---|---|---|
| `05 xx` / `06 xx xx` | Usage Page (1-byte / 2-byte) | Which vocabulary the following usages come from. `01` generic desktop, `07` keyboard, `0C` consumer, `FF00`+ vendor-defined |
| `09 xx` / `0A xx xx` | Usage | What this control is |
| `19 xx` / `29 xx` | Usage Minimum / Maximum | A contiguous range of usages, for array fields and for bitfields like modifier keys |
| `A1 01` … `C0` | Collection (Application) … End Collection | The wrapper every top-level device needs. `A1 00` is a Physical collection, `A1 02` a Logical one |
| `85 xx` | Report ID | Everything after it belongs to that report, until the next Report ID |
| `15 xx` / `25 xx` (`16`/`26` for 2-byte) | Logical Minimum / Maximum | The numeric range of one field. **A negative minimum makes the field signed** (two's complement in `Report Size` bits) |
| `75 xx` | Report Size | Bits per field |
| `95 xx` | Report Count | How many fields of that size |
| `81 xx` | Input | Emit the fields declared so far. Data bits: `02` = Data,Variable,Absolute (a value); `00`/`01` with the constant bit = padding; `Array` for a list of pressed usages |
| `91 xx` | Output | Same, Host to device |
| `B1 xx` | Feature | Same, configuration rather than data |

Four things must be in effect before every `Input` / `Output` / `Feature` item:
**a usage page, a usage (or usage range), a logical range, and a size and count.**
Anything you leave unset keeps its previous value — which is convenient and is
also the most common source of a descriptor that decodes as nonsense.

## 3. How the bytes end up in a report

- **Fields pack in declaration order, LSB first** within each byte. A field may
  cross a byte boundary; padding does not appear on its own.
- **Pad every report to a whole number of bytes** with a constant Input item.
  Eight buttons and one 4-bit hat is 12 bits, so add 4 constant bits.
- **A signed field is two's complement of exactly `Report Size` bits.** A mouse
  delta declared as Logical Minimum -127, Maximum 127, size 8 sends `0xFF` for
  -1. Widen the size, not just the logical range, if you need more.
- **A bitfield is `Report Count` fields of `Report Size` 1**, one per usage in
  the range — that is how modifier keys and gamepad buttons work.
- **An array is `Report Count` fields of `Report Size` 8, each holding a usage
  code** — that is how a keyboard reports up to six simultaneous keys, and why
  the order in the array carries no meaning.
- **A hat switch** is a single field whose logical range covers centre plus the
  directions. EspBle's gamepad declares it as one 8-bit field with Logical
  Minimum 0, Maximum 8: `0` is centred and `1`..`8` are the eight directions
  clockwise from up (`ESP_BLE_HID_GAMEPAD_HAT_*`). It also declares a physical
  range of 0..315 with the unit set to degrees, which is what lets a Host map the
  value to a direction rather than treat it as a number.

Where the report ID sits differs by transport, and this catches everyone once:

| | BLE (HOGP) | Classic (HID over BR/EDR) |
|---|---|---|
| Report ID on the air | Not in the payload — each report has its own characteristic | **First byte of the payload** |
| What `sendInput()` takes | The payload only | The payload only; the library prepends the ID |
| What a Host callback receives | The payload | The payload, with the ID as byte 0 in the raw view |
| Field offsets in the descriptor | Relative to the payload | Relative to the payload, i.e. **after** the ID byte |

That mismatch is a real defect this library once had: the Classic HID host
dropped every report from a device using report IDs, because the transport puts
the ID in front while the descriptor's offsets do not count it. If you parse raw
reports yourself, mind the same byte.

## 4. A worked example

[`Hid/CustomDevice`](../examples/Hid/CustomDevice/) is a vendor-defined control
panel: a signed dial delta plus a button bitfield in, an LED state out. Read it
as a sequence of decisions rather than a block of hex.

1. `06 00 FF` — vendor-defined usage page, because no standard page describes
   "my control panel". A Host will not act on it by itself; your application
   will.
2. `09 01` / `A1 01` — a usage for the collection, then Application collection.
   Every top-level device needs one.
3. `85 01` — Report ID 1. Everything until the next Report ID belongs to it.
4. `15 81 25 7F` — logical range -127..127. **This is what makes the field
   signed.**
5. `75 08 95 02` — two 8-bit fields.
6. `09 02 81 02` — a usage, then Input(Data,Variable,Absolute): emit those two
   bytes.
7. `15 00 25 01 75 08 95 01` — then a 0..1 range, one 8-bit field …
8. `09 03 91 02` — … as an Output: one byte the Host writes.
9. `C0` — End Collection.

Then, in the sketch, `custom.setReportMap(...)`, `custom.addInputReport(1, 2)`
and `custom.addOutputReport(1, 1)`: the same report ID, the same sizes. If you
change the descriptor and forget the declarations, the characteristic is the
wrong size and the Host reads short.

For a standard-usage example that a Host acts on by itself, read the gamepad
descriptor the fixed profile emits (`EspBleHidGamepadDescriptor`), which the peer
tests pin byte for byte. It is worth tracing because it uses every technique
above in 39 fields and 11 payload bytes:

- Six signed 8-bit axes (X, Y, Z, Rz, Rx, Ry) sharing one Logical Minimum -127 /
  Maximum 127 and one size and count — six usages, then a single Input item.
- One 8-bit hat switch with its own logical range, physical range and unit, then
  the unit reset to zero afterwards so it does not leak into the next field.
- Thirty-two buttons as a usage range (`Usage Minimum 1`, `Usage Maximum 0x20`)
  of 1-bit fields — four bytes, no padding needed because 32 bits is byte-aligned.

Six plus one plus four is the 11-byte payload, and with report ID 3 in front on
Classic that is the `id=3 len=12` the gamepad peer test asserts.

## 5. Classic: the budget that decides what fits

On Classic the descriptor travels in an SDP record, and that record has a hard
budget: **214 bytes for the descriptor plus the `name`, `description` and
`provider` strings together** (the record's standard attributes take 86 of the
300-byte pad). `begin()` refuses an overflowing configuration with
`ResourceExhausted` — the backend used to log the overflow and report success,
which brought up a device no Host could find.

Consequences when you are designing a Classic device:

- **Shorten the strings to buy descriptor bytes**, or the other way round. They
  come from the same 214.
- **A composite has to be chosen deliberately.** With the default strings,
  keyboard + mouse + consumer fits and adding a gamepad does not.
- **Only keyboard and mouse are decoded by the Classic HID host.** Everything
  else arrives raw at `onInputReport()`, which is exactly where a hand-written
  descriptor should be verified.
- BLE has no equivalent limit: the Report Map is a characteristic and is read
  like any other value.

## 6. Verifying it

Do not trust a descriptor you have only read. Three levels, cheapest first.

**Decode it offline.** Paste the bytes into any HID descriptor decoder (the
USB-IF HID Descriptor Tool, or one of the web decoders) and check that the report
sizes it computes match what you declared with `addInputReport()`. A decoder that
reports an unexpected total size means the descriptor and your declaration have
already diverged.

**Watch the raw bytes with a second board.** This is what the repository does,
and it is the most useful bench you can build: one board is the device, the other
runs `hidHost()` (BLE) or the Classic HID host, prints the raw report as hex, and
you assert the offsets. On Classic that is the only way to see anything but a
keyboard or mouse. `peer/classic_hid_gamepad` is the pattern to copy — it checks
that axis negatives stay signed, that the hat and the button bitfield sit where
the descriptor declared, that report IDs are not confused between profiles, and
that a release returns every byte to zero.

**Then try a real Host.** Linux exposes a connected HID device through hidraw, so
`hid-tools`' `hid-recorder` shows both the descriptor the Host received and the
reports as they arrive; a phone or a PC tells you whether your usages were chosen
sensibly, which no amount of byte checking can.

**Re-pair after every descriptor change.** Hosts cache the Report Map per bond —
Windows, Android, macOS, iOS and BlueZ all parse it once at pairing and keep
using that parse. After changing the descriptor, deleting the bond on the device
is not enough: remove (unpair, "forget") the device on the Host as well, then
pair again, or the Host keeps reading the new reports with the old layout. A
device that worked until you fixed the descriptor is this, almost always.

What failure looks like:

| Symptom | Likely cause |
|---|---|
| The Host ignores the device entirely | Missing Application collection, or a usage page/usage a Host does not accept for that device class |
| Fields are read at the wrong offsets | Padding missing, or the report ID byte counted (Classic) or not counted (BLE) |
| Values jump between extremes | Field declared unsigned (Logical Minimum 0) while you send negatives, or `Report Size` too small |
| A button does nothing | Bitfield count and usage range disagree, so your bit belongs to another usage |
| Only the first report ever arrives | The Host is not subscribed to that report's characteristic (BLE), or your declaration and the descriptor disagree on the size |
| It worked until the descriptor changed, now fields are garbled | The Host is still using the Report Map it cached at pairing — unpair on the Host and pair again |
| `begin()` returns `ResourceExhausted` (Classic) | The 214-byte SDP budget |

## 7. Checklist

- [ ] Usage page, usage, logical range, size and count are all in effect before
      every Input / Output / Feature item.
- [ ] Every report is padded to a whole number of bytes.
- [ ] Signed fields have a negative Logical Minimum and a `Report Size` wide
      enough for the range.
- [ ] Report IDs are unique, and do not take 1..6 while a fixed profile is
      enabled.
- [ ] `addInputReport()` / `addOutputReport()` / `addFeatureReport()` match the
      descriptor's IDs and byte sizes.
- [ ] On Classic, descriptor plus the three strings is at most 214 bytes.
- [ ] The raw bytes were observed on a second board, not only reasoned about.

Related: [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md),
[HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md),
[the HID chapter of the BLE guide](GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse),
and [EspBle in depth](GUIDE_ADVANCED.md) for the report-length and composition
limits.
