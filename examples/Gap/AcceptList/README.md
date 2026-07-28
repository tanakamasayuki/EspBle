# AcceptList

> 日本語版: [README.ja.md](README.ja.md)

A peripheral-side example that **restricts who may connect**.

BLE has **no** "a connection request arrived, inspect the peer and approve or reject it" callback. The controller decides by matching the peer against the **Filter Accept List** (formerly "white list"), and a rejected peer never reaches the application at all. That leaves three options:

| Approach | Description |
|---|---|
| **Filter Accept List** | This example. The controller rejects, so it is the most reliable and costs the application nothing |
| Disconnect after connecting | Inspect the peer in `onConnected` and call `disconnect()`. The connection does get established once |
| Protect the attributes | Require encryption/authentication on characteristics ([Security/*](../../Security/)). Anyone may connect, but the values stay protected |

Combine them as needed: use the accept list when a peer should not connect at all, and encryption when it may connect but must not read the values.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- A central that tries to connect — the [Gap/Connect](../Connect/) example on a second board, or a phone app

Replace `ALLOWED_CENTRAL` in the sketch with **the address of the central you want to allow** before using it. Left unchanged, nobody can connect — which does at least demonstrate that the filter works.

## What it does

- Adds the allowed address to the accept list and advertises with the `ConnectionFromAcceptList` policy
- Connection requests from peers not on the list are silently dropped by the controller; the peer sees a connection timeout
- Sending `o` returns the policy to `Any` so anyone may connect; `r` restricts it again

## Key APIs

- `ble.addToAcceptList(address, addressType)` — add an entry (up to 8)
- `ble.removeFromAcceptList(address, addressType)` / `ble.clearAcceptList()`
- `ble.acceptListCount()` / `ble.acceptListEntry(index, entry)`
- `ble.advertising().setFilterPolicy(policy)` — `Any` / `ScanRequestFromAcceptList` / `ConnectionFromAcceptList` / `Both`

## Notes

- **A policy change takes effect when advertising starts.** To change it while running, do `stop()` → `setFilterPolicy()` → `start()` as this example does.
- **Matching is by address.** A peer that rotates an RPA cannot be listed usefully until it is bonded and its identity address applies (see [Gap/PrivateAddress](../PrivateAddress/)).
- **A restrictive policy with an empty accept list rejects everyone.** That is usable as a deliberate lock, but easy to hit by accident.
- A rejected peer is not told it was rejected. The Link Layer has no PDU for refusing a connection, so the controller simply drops the request; from the peer's side it looks like a connection that timed out with no answer.

## Expected Serial output

```
Advertising. Only aa:bb:cc:dd:ee:ff may connect.
Policy: open (accept list has 1 entries)
Connected id=1 from d0:cf:13:58:fd:95
Disconnected id=1
```
