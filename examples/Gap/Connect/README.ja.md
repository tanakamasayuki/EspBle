# Connect

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

特定のService UUIDをadvertiseするPeripheralを探し、Centralとして接続します。非同期の接続モデルを示すexampleです: `connect()`は要求の受理だけを返し、完了（または失敗）は後から`ble.update()`経由のイベントとして届きます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- 対象Service UUIDをadvertiseするBLE Peripheral × 1 — 例えば2台目のボードで[Gatt/Basics/Server](../../Gatt/Basics/Server/) exampleを動かします（`TARGET_SERVICE_UUID`を合わせて変更してください）

## 動作

- active scanを開始し、各resultから`TARGET_SERVICE_UUID`を探します
- 最初に一致した相手へscanを停止して接続を要求します
- 接続・切断・接続失敗のイベントをlibrary connection IDと一緒に表示します
- 切断・失敗後は次のscan resultで再試行します

sketch冒頭の`TARGET_SERVICE_UUID`を、接続したいPeripheralがadvertiseするUUIDへ書き換えてください。

## 主なAPI

- `scanResult.advertisesService(uuid)` — 16-bit表記（`"1812"`）と128-bit表記のどちらでも一致します
- `ble.connect(scanResult)` — 要求を受理して即座に返ります。接続処理自体は内部taskで実行されます
  - `ble.connect(scanResult, timeoutMilliseconds)` — timeoutは`update()`から強制されます（既定10000ms）
- `ble.connect(address, EspBleAddressType, timeoutMilliseconds)` — 保存済みaddressからScanなしで接続します
- `ble.onConnected(callback)` / `ble.onDisconnected(callback)` — どちらも同じ安定した`connection.id`を持ちます
- `ble.onConnectionFailed(callback)` — 非同期失敗。`failure.detail`で理由を確認できます

## 期待されるSerial出力

```
Connected to 5a:b8:1e:0c:2f:71 (id=1)
Disconnected (id=1)
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Connect.ino](Connect.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

Connect - 特定のService UUIDをadvertiseするPeripheralを探して接続する。 非同期モデルの例: connect() は要求の受理だけを返し、完了/失敗は あとから update() 経由のイベントで届く。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
4. `ble.onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
5. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
6. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

接続前に探索結果の条件で相手を選びます。要求・成立・失敗を区別して確認します。この例はGAP接続までで、アプリ用GATT値の通信は行いません。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
