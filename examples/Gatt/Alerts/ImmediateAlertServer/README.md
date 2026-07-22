# ImmediateAlertServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Immediate Alert Service (0x1802) peripheral — the Find Me profile **target** role. Alert Level (0x2A06) is a single **Write Without Response** uint8 (0 = No Alert, 1 = Mild, 2 = High).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / Find Me target)
- 1 × central: the [ImmediateAlertClient](../ImmediateAlertClient/) example, or any Find Me locator

## What it does

- Registers Alert Level as a write / write-without-response characteristic before `begin()` and advertises 0x1802
- Reacts to each written Alert Level in `onWritten` (a real target would beep or vibrate)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .writable = true, .writableWithoutResponse = true })`
- `ble.gattServer().onWritten(...)` — receive Alert Level writes in loop context

## Expected Serial output

```
Alert Level: 2 (High Alert)
Alert Level: 0 (No Alert)
```
