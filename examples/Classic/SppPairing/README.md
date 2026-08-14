# SppPairing (Classic)

> 日本語版: [README.ja.md](README.ja.md)

Classic pairing under application control, over an SPP server. Without a security
configuration the stack pairs with Just Works and accepts every request, which is
fine for a closed setup and wrong for anything a stranger can reach. **Classic
works on the original ESP32 only**, and Classic pairing produces a link key that
is separate from a BLE bond: removing one does not remove the other.

## Hardware

- 1 × original ESP32 running this sketch
- 1 × peer that pairs: a PC or Android device, or a second original ESP32 running
  [SppClient](../SppClient/)

## What it does

- Enables security with the `DisplayYesNo` IO capability, which is what produces a
  number both sides compare
- Answers the numeric comparison from the application — auto-accepting here, where
  a real product asks the user
- Reports the pairing result, success or failure, with the backend status
- Lists the bonds stored in NVS at startup

## Key APIs

- `EspBleClassicConfig::security` — `enabled` and `ioCapability`, both read during
  `begin()`
- `onNumericComparisonRequested()` / `confirmNumericComparison()` — the question
  and the answer
- `onPasskeyDisplayed()` / `onPasskeyRequested()` / `providePasskey()` — the other
  IO capabilities
- `bondCount()` / `bond(index)` / `deleteBond()` / `deleteAllBonds()`

## Notes

**Without `security.enabled` the IO capability has no effect.** Secure Simple
Pairing only involves the application when a service asks for it, so enabling
security is what makes the SPP service require MITM protection.

Answering nothing rejects the pairing once `responseTimeoutMilliseconds` elapses.
A peer left waiting forever is worse than a refusal.

Legacy PIN pairing is refused. There is no way to answer it here, and a fixed PIN
would be a fixed key.
