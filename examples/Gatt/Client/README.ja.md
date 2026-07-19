# Client

> English: [README.md](README.md)

[Gatt/Server](../Server/) exampleへ接続し、CentralのGATT Clientフローを一通り実行します: database一覧Discovery → 既知UUIDのDiscovery → Read → 応答あり/なしWrite → Descriptor Read/Write。各要求は直ちに`bool`を返し、完了は`ble.update()`からのイベントとして後から届きます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/Server](../Server/) exampleを動かすESP32-S3 × 1

## 動作内容

- ServerのService UUIDをscanして接続します
- Service、Characteristic、Descriptorを接続単位のsnapshotへ一覧Discoveryします
- 既知CharacteristicのDiscovery後、Read、応答あり/なしWrite、Descriptor Read/Writeを連鎖します
- Central GATT操作が同時1件であることを示します — 次の操作は前の操作の完了callbackから発行します

## 主なAPI

- `ble.discoverServices()` / `onServicesDiscovered()` — peer databaseの一覧Discovery
- `discoveredService*()` / `discoveredCharacteristic*()` / `discoveredDescriptor*()` — 切断または次の一覧Discoveryまでsnapshotを照会
- `ble.discoverCharacteristic(connectionId, serviceUuid, characteristicUuid)` — 既知UUIDのDiscovery
- `ble.onCharacteristicDiscovered(callback)` — `success`、property、`detail`を持つ`EspBleGattResult`
- `ble.readCharacteristic(...)` / `ble.onCharacteristicRead(callback)` — `result.value`が値を保持します（binary-safe）
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, withResponse)` / `ble.onCharacteristicWritten(callback)`
- `ble.readDescriptor()` / `writeDescriptor()`と各完了callback
- 各操作の末尾の`timeoutMilliseconds`（既定10000、0は無効）— timeoutは`EspBleError::Timeout`で完了します
- Central GATT操作は排他です: 実行中に2つ目を要求すると`InvalidState`で同期的に失敗します

## 期待されるSerial出力

```
Read: ready
Descriptor: EspBle value
Descriptor write complete
```
