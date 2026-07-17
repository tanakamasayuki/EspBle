# Client

> English: [README.md](README.md)

[Gatt/Server](../Server/) exampleへ接続し、CentralのGATT Clientフローを一通り実行します: 既知UUIDのDiscovery → Read → Write with Response。各ステップは`bool`を返す要求APIで受理され、完了は`ble.update()`からのイベントとして後から届きます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/Server](../Server/) exampleを動かすESP32-S3 × 1

## 動作内容

- ServerのService UUIDをscanして接続します
- 既知のCharacteristicをDiscoveryし、Read → 値を表示 → `hello from Central`をWrite → Write結果を表示、と連鎖します
- Central GATT操作が同時1件であることを示します — 次の操作は前の操作の完了callbackから発行します

## 主なAPI

- `ble.discoverCharacteristic(connectionId, serviceUuid, characteristicUuid)` — 既知UUIDのDiscovery（Service/Characteristicの一覧列挙は未実装です）
- `ble.onCharacteristicDiscovered(callback)` — `success`、property、`detail`を持つ`EspBleGattResult`
- `ble.readCharacteristic(...)` / `ble.onCharacteristicRead(callback)` — `result.value`が値を保持します（binary-safe）
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, withResponse)` / `ble.onCharacteristicWritten(callback)`
- Central GATT操作は排他です: 実行中に2つ目を要求すると`InvalidState`で同期的に失敗します

## 期待されるSerial出力

```
Read: ready
Write complete
```
