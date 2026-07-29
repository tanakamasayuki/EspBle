# NumericComparisonServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.ja.md) — chapter 3, "Security" (Japanese)

The peripheral side of **Numeric Comparison** pairing. Its counterpart is [NumericComparisonClient](../NumericComparisonClient/), and **both sides must be configured the same way** (`DisplayYesNo` plus MITM required).

The difference from Passkey Entry is that **neither side types anything**. LE Secure Connections shows the same 6 digits on both devices and the user only answers whether they match. This is the method used by devices that have a screen but no keyboard — a phone and a pair of earbuds, for instance.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × ESP32-S3 running [NumericComparisonClient](../NumericComparisonClient/)

Keep both serial monitors visible at once: **comparing the two numbers is the whole point** of this method.

## What it does

- Advertises as a GATT server whose characteristic requires `authenticatedRead`
- When pairing starts, the 6 digits to compare arrive at `onNumericComparison`
- `y` accepts the match, `n` rejects it. **Pairing is stopped until it is answered**
- Encryption only completes when both sides accept, and `onSecurityChanged` then reports `authenticated=1`
- `c` deletes all bonds (only while disconnected)

## Key APIs

- `EspBleSecurityConfig::ioCapability = DisplayYesNo` — can display and answer yes/no
- `ble.onNumericComparison(cb)` — the 6 digits to compare arrive in `event.passkey`
- `ble.confirmNumericComparison(accept)` — answer whether they matched

## Notes

- **Numeric Comparison is only chosen when both sides declare DisplayYesNo and both require MITM.** With `DisplayOnly` on one side you get Passkey Entry; with `None` you get Just Works. BLE has no API for selecting the method directly.
- **It requires LE Secure Connections**, so it cannot be used with peers older than BLE 4.2.
- **Answer within 30 seconds.** Past that the stack stops waiting and pairing fails.
- If the numbers differ, always send `n`. A mismatch is precisely the sign of a man-in-the-middle.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Does the peer show 052913? Send 'y' to accept, 'n' to reject.
Answer accept: sent
Security established: encrypted=1 authenticated=1 bonded=1
```
