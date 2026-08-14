# ScanResponse

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Splits the advertised data across two payloads: the **advertising payload** and the **scan response payload**.

A legacy advertising payload holds only 31 bytes. When a scanner performs an **active scan** it sends a Scan Request to the advertiser, which answers with a **Scan Response** — a second 31 bytes. That brings the total to 62.

| | Advertising payload | Scan response payload |
|---|---|---|
| Who receives it | **Everyone** nearby (visible to passive scans too) | Only scanners that ask for it with an **active scan** |
| What belongs there | The minimum needed to identify the peer (service UUIDs) | Descriptive fields (name, appearance, manufacturer data) |
| Flags | Added automatically | **Not allowed** (the spec reserves them for the advertising payload) |

In EspBle, `advertising().data()` and `advertising().scanResponse()` return the same kind of builder, so you decide which field goes on which side.

## Relationship to the default behaviour

When the scan response is left empty, EspBle **places the device name there automatically** so the name does not eat into the 31-byte advertising payload.

Setting anything on the scan response turns that automatic placement off. If you still want the name, set it explicitly with `scanResponse().setName(...)` as this example does.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- A receiver — the [Info/ScanDump](../../Info/ScanDump/) example on a second board, or a scanner app such as nRF Connect

## What it does

- Puts a 128-bit service UUID, the appearance, and Tx Power in the advertising payload (28 of 31 bytes including flags)
- Puts the name (22 bytes) and manufacturer data (7 bytes) in the scan response (29 of 31 bytes)
- A passive scan sees only the former; an active scan sees both, merged into one result

Every AD structure costs 2 bytes (length + type) on top of its value. The sketch spells out the budget for each side in comments; use it as a starting point when rearranging fields.

## Key APIs

- `ble.advertising().data()` — builder for the advertising payload; the existing setters such as `setName()` forward to it
- `ble.advertising().scanResponse()` — builder for the scan response payload
- `EspBleAdvertisingData::setName()` / `addServiceUuid()` / `setManufacturerData()` / `addServiceData()` / `setAppearance()` / `setTxPowerIncluded()`
- `ble.advertising().setScanResponseEnabled(false)` — disable the scan response entirely (for a pure broadcaster; see [Beacon](../Beacon/))

## Notes

- **The Tx Power value is filled in by the controller.** The sketch only chooses whether to include it; the radio writes the actual power. A receiver estimates distance from the gap between `txPowerLevel` and `rssi` (the path loss).
- Appearance is what a phone uses to pick an icon. On the receiving side it is available as `appearance` / `hasAppearance()` on `EspBleScanResult`.
- If either payload exceeds 31 bytes, `start()` fails with `InvalidArgument` and `lastErrorDetail()` **names the field that did not fit**.

  ```
  Advertising failed: INVALID_ARGUMENT (name does not fit in the 31-byte scan response payload)
  ```
- **Flags cannot be placed in a scan response.** The Bluetooth Core Specification (CSS Part A) defines the Flags AD type for the advertising payload only, so putting it in a scan response violates the spec. EspBle adds it to the advertising payload automatically.

## Expected Serial output

```
Advertising. Passive scanners see only the service UUID.
```

On the [Info/ScanDump](../../Info/ScanDump/) side (active scan):

```
d0:cf:13:58:fd:95 type=0 rssi=-38 connectable scannable name="EspBle Scan Response" uuid=5266f727-49d7-4eaf-a6f1-7363616e7270 manufacturer[5]=ffff010203
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting
