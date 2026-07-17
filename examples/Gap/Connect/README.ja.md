# Connect

> English: [README.md](README.md)

特定のService UUIDをadvertiseするPeripheralを探し、Centralとして接続します。非同期の接続モデルを示すexampleです: `connect()`は要求の受理だけを返し、完了（または失敗）は後から`ble.update()`経由のイベントとして届きます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Central）
- 対象Service UUIDをadvertiseするBLE Peripheral × 1 — 例えば2台目のボードで[Gatt/Server](../../Gatt/Server/) exampleを動かします（`TARGET_SERVICE_UUID`を合わせて変更してください）

## 動作内容

- active scanを開始し、各resultから`TARGET_SERVICE_UUID`を探します
- 最初に一致した相手へscanを停止して接続を要求します
- 接続・切断・接続失敗のイベントをlibrary connection IDと一緒に表示します
- 切断・失敗後は次のscan resultで再試行します

sketch冒頭の`TARGET_SERVICE_UUID`を、接続したいPeripheralがadvertiseするUUIDへ書き換えてください。

## 主なAPI

- `scanResult.advertisesService(uuid)` — 16-bit表記（`"1812"`）と128-bit表記のどちらでも一致します
- `ble.connect(scanResult)` — 要求を受理して即座に返ります。接続処理自体は内部taskで実行されます
  - `ble.connect(scanResult, timeoutMilliseconds)` — timeoutは`update()`から強制されます（既定10000ms）
- `ble.onConnected(callback)` / `ble.onDisconnected(callback)` — どちらも同じ安定した`connection.id`を持ちます
- `ble.onConnectionFailed(callback)` — 非同期失敗。`failure.detail`で理由を確認できます

## 期待されるSerial出力

```
Connected to 5a:b8:1e:0c:2f:71 (id=1)
Disconnected (id=1)
```
