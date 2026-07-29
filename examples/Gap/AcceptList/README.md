# AcceptList

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

One **Filter Accept List** (formerly "white list") put to **two uses**: restricting who may connect (the advertising side) and filtering which advertisers a scan reports (the scanning side), both from the same list.

BLE has **no** "a connection request arrived, inspect the peer and approve or reject it" callback. The controller decides by matching the peer against the **Filter Accept List** (formerly "white list"), and a rejected peer never reaches the application at all. That leaves three options:

| Approach | Description |
|---|---|
| **Filter Accept List** | This example. The controller rejects, so it is the most reliable and costs the application nothing |
| Disconnect after connecting | Inspect the peer in `onConnected` and call `disconnect()`. The connection does get established once |
| Protect the attributes | Require encryption/authentication on characteristics ([Security/*](../../Security/)). Anyone may connect, but the values stay protected |

Combine them as needed: use the accept list when a peer should not connect at all, and encryption when it may connect but must not read the values.

The accept list is a single list held by the controller and is **shared by advertising and scanning**. On the scanning side, setting `EspBleScanConfig::acceptListOnly` makes the controller drop advertisements from peers that are not listed, so they never reach `onResult`. In a "talk to this one device only" setup, the same list covers both directions.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × peer board — a central that tries to connect ([Gap/Connect](../Connect/)) or a peripheral that advertises ([Gap/Advertise](../Advertise/)). A phone app works too

Replace `ALLOWED_PEER` in the sketch with **the address of the peer you want to allow** before using it. That board can report its own address with `ble.localAddress()`. Left unchanged, nobody can connect and the filtered scan reports nothing — which does at least demonstrate that the filter works.

## What it does

- Adds the allowed address to the accept list and advertises with the `ConnectionFromAcceptList` policy
- Connection requests from peers not on the list are silently dropped by the controller; the peer sees a connection timeout
- Sending `o` returns the policy to `Any` so anyone may connect; `r` restricts it again
- `f` starts a 5-second scan with `acceptListOnly` set, so only the allowed address is reported
- `a` runs the same scan unfiltered. Every advertiser nearby shows up, and the difference from `f` is exactly what the filter removes

## Key APIs

- `ble.addToAcceptList(address, addressType)` — add an entry (up to 8)
- `ble.removeFromAcceptList(address, addressType)` / `ble.clearAcceptList()`
- `ble.acceptListCount()` / `ble.acceptListEntry(index, entry)`
- `ble.advertising().setFilterPolicy(policy)` — `Any` / `ScanRequestFromAcceptList` / `ConnectionFromAcceptList` / `Both` (advertising side)
- `EspBleScanConfig::acceptListOnly` — apply the same list to the scanning side

## Notes

- **The list is shared by advertising and scanning.** An entry added for one applies to the other; separate lists are not possible, because the controller holds only one.
- **A policy change takes effect when advertising starts.** To change it while running, do `stop()` → `setFilterPolicy()` → `start()` as this example does.
- **Matching is by address.** A peer that rotates an RPA cannot be listed usefully until it is bonded and its identity address applies (see [Gap/PrivateAddress](../PrivateAddress/)).
- **A restrictive policy with an empty accept list rejects everyone.** That is usable as a deliberate lock, but easy to hit by accident.
- A rejected peer is not told it was rejected. The Link Layer has no PDU for refusing a connection, so the controller simply drops the request; from the peer's side it looks like a connection that timed out with no answer.

## Expected Serial output

```
Advertising. Only aa:bb:cc:dd:ee:ff may connect.
Commands: 'o' open policy, 'r' restrict, 'f' filtered scan, 'a' scan everyone
Scanning for 5 s (accept list only)
Advertiser aa:bb:cc:dd:ee:ff rssi=-41 (filtered scan)
Policy: open (accept list has 1 entries)
Connected id=1 from aa:bb:cc:dd:ee:ff
Disconnected id=1
```
