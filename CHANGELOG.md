# Changelog

## Unreleased

- Establish the initial project, specification, and test structure.
- Add the first EspBle stack, legacy advertising, scanning, deferred scan-result callback, examples, and two-board peer test.
- Add central/peripheral connections with library connection IDs, disconnect, connection snapshots, and MTU exchange with `onMtuChanged()`.
- Add generic GATT server/client: known-UUID discovery, read, write with response, notify/indicate, subscription, and CCCD events.
- Add security: Just Works and static-passkey pairing with LE Secure Connections, bonding, encrypted/authenticated characteristic permissions, and bond management.
- Add the HID Keyboard Device profile: fixed 6KRO report protocol keyboard with HID/Device Information/Battery services, LED output reports, and battery level updates.
- Add the HID Keyboard Host profile: HID discovery, input report subscription, 256-bit usage snapshots, `onKeyboard()` press/release events, 19 EspUsbHost-compatible keyboard layouts, LED output writes, and battery reads.
- Add fixed-capacity event listeners (`add*Listener()` / `removeListener()`) to the HID Keyboard Host for ESP32KeyBridge adapter coexistence.
