# NumericComparisonClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

The central side of **Numeric Comparison** pairing. Its counterpart is [NumericComparisonServer](../NumericComparisonServer/), and the configuration is **exactly the same as the server's** (`DisplayYesNo` plus MITM required) — both sides declaring the same thing is what makes this method get chosen.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × ESP32-S3 running [NumericComparisonServer](../NumericComparisonServer/)

Keep both serial monitors visible at once.

## What it does

- Active-scans for the server's service UUID and connects to the first match
- `pairOnConnect` (on by default) starts pairing as soon as the connection comes up
- The 6 digits to compare arrive at `onNumericComparison` — they should equal what the server shows
- `y` accepts, `n` rejects. **Pairing is stopped until it is answered**
- Once both sides accept, it discovers and reads a characteristic requiring `authenticatedRead`
- `c` deletes all bonds (only while disconnected)

## Key APIs

- `EspBleSecurityConfig::ioCapability = DisplayYesNo`
- `ble.onNumericComparison(cb)` / `ble.confirmNumericComparison(accept)`
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — access after pairing

## Notes

- **A rejection from either side fails the pairing.** One `n` ends it for both.
- **Answer within 30 seconds**, or the stack stops waiting.
- On later connections the bond applies and no pairing happens, so nothing is asked. Send `c` on both sides to try again.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Does the peer show 052913? Send 'y' to accept, 'n' to reject.
Answer accept: sent
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities
