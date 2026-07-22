# NusServer

> English: [README.md](README.md)

Nordic UART Service（NUS）のUUID構成を汎用GATT Server APIで実装します（Peripheral）。Service `6e400001-…` のもとで、RX（`6e400002-…`）はWriteを受け取り、TX（`6e400003-…`）はNotificationを送ります。受信したRXデータは購読中のTX Clientへechoします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [NusClient](../NusClient/) exampleを動かすESP32-S3 × 1（またはNUS互換のCentral）

## 動作

- `begin()`前にNUS Serviceを登録し、RX Characteristicを応答あり/なしWrite可、TX CharacteristicをNotify可として構成します
- RX Writeを表示し、同じバイト列をTX Notificationとしてecho、echoが受理されたかを表示します
- TXの購読状態変化を`onSubscriptionChanged()`で報告します
- `EspBle NUS`の名前でNUS Service UUIDをadvertiseします

## 主なAPI

- `gattServer.addCharacteristic(serviceUuid, uuid, config)` — RXは`writable` + `writableWithoutResponse`、TXは`notifiable`
- `gattServer.onWritten(callback)` — `characteristicUuid.equalsIgnoreCase(...)`でRX UUIDに絞り込む`EspBleGattWrite`
- `gattServer.notify(serviceUuid, characteristicUuid, value)` — 受信バイトをTXでechoし、受理されたかを返します
- `gattServer.onSubscriptionChanged(callback)` — TX Characteristicの`subscription.notifications`

## メモ

- NUSはpacket指向のGATT慣例であり、Bluetooth Classic SPPではありません。payloadは接続のATT/MTU上限内に収め、複数packetへ分割する場合はapplication側でframingします。

## 期待されるSerial出力

```
TX notifications: 1
RX: hello
Echo accepted: 1
```
