# NusServer

> 日本語版: [README.ja.md](README.ja.md)

Implements the Nordic UART Service (NUS) UUID layout with the generic GATT server API (peripheral): RX (`6e400002-…`) accepts writes and TX (`6e400003-…`) sends notifications, under service `6e400001-…`. Received RX data is echoed back to subscribed TX clients.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × ESP32-S3 running the [NusClient](../NusClient/) example (or any NUS-compatible central)

## What it does

- Registers the NUS service with a writable RX characteristic (with and without response) and a notifiable TX characteristic before `begin()`
- Prints each RX write, then echoes the same bytes back as a TX notification and prints whether the echo was accepted
- Reports TX subscription changes via `onSubscriptionChanged()`
- Advertises the NUS service UUID under the name `EspBle NUS`

## Key APIs

- `gattServer.addCharacteristic(serviceUuid, uuid, config)` — RX with `writable` + `writableWithoutResponse`, TX with `notifiable`
- `gattServer.onWritten(callback)` — `EspBleGattWrite` filtered to the RX UUID via `characteristicUuid.equalsIgnoreCase(...)`
- `gattServer.notify(serviceUuid, characteristicUuid, value)` — echoes the received bytes on TX; returns whether it was accepted
- `gattServer.onSubscriptionChanged(callback)` — `subscription.notifications` for the TX characteristic

## Notes

- NUS is a packet-oriented GATT convention, not Bluetooth Classic SPP. Payloads must fit the connection's ATT/MTU limit; add your own framing for messages that span multiple packets.

## Expected Serial output

```
TX notifications: 1
RX: hello
Echo accepted: 1
```
