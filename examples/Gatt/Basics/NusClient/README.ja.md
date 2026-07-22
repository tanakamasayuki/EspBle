# NusClient

> English: [README.md](README.md)

汎用GATT Client APIの組合せでNordic UART Service（NUS）Serverと通信するCentralです。Service `6e400001-…` のもとで、TX Notification（`6e400003-…`）を購読し、Serialへ入力された各行をWrite Without ResponseでRX（`6e400002-…`）へ送ります。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [NusServer](../NusServer/) exampleを動かすESP32-S3 × 1（またはNUS互換のPeripheral）

## 動作

- NUS Service UUIDをscanして接続します
- 接続後にTX Notificationを購読し、`NUS ready`を表示します
- Serialから空でない各行を読み取り、RXへWrite Without Responseで送ります
- Serverから返るTX Notificationを`RX: …`として表示します

## 主なAPI

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid)` — TX Notificationを購読
- `ble.onSubscribed(callback)` — 購読完了（`result.success`）
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, false)` — RXへWrite Without Response
- `ble.onCharacteristicWritten(callback)` — Write受理の結果
- `ble.onNotification(callback)` — `characteristicUuid.equalsIgnoreCase(...)`で絞り込むTX payload

## メモ

- NUSが汎用GATT Client APIの組合せで構築できることを示します。stream意味論や自動packet framingは提供しません。Serialモニタに行を入力すると送信されます。

## 期待されるSerial出力

```
NUS ready: 1
TX accepted: 1
RX: hello
```
