# Client

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

[Gatt/Server](../Server/) exampleへ接続し、CentralのGATT Clientフローを一通り実行します: database一覧Discovery → 既知UUIDのDiscovery → Read → 応答あり/なしWrite → Descriptor Read/Write → 要求時に作られる値のRead。各要求は直ちに`bool`を返し、完了は`ble.update()`からのイベントとして後から届きます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/Server](../Server/) exampleを動かすESP32-S3 × 1

## 動作

- ServerのService UUIDをscanして接続します
- Service、Characteristic、Descriptorを接続単位のsnapshotへ一覧Discoveryします
- 既知CharacteristicのDiscovery後、Read、応答あり/なしWrite、Descriptor Read/Writeを連鎖します
- 最後に、Server側が `onRead()` でその場で作る値（`10da4dd3-…`）を読み、`Live:` として表示します
- Central GATT操作が同時1件であることを示します — 次の操作は前の操作の完了callbackから発行します

## 連鎖として書く

GATT操作はすべて非同期で、しかも**Central側の操作は同時に1件しか実行できません**。実行中に2つ目を要求すると、その場で `InvalidState` として同期的に失敗します。つまり手続きを上から並べて書くことはできず、**「頼む → 完了イベントの中で次を頼む」の連鎖**になります。

このexampleはその連鎖がそのまま形に出ています。

```
onConnected        → discoverServices()
onServicesDiscovered → discoverCharacteristic()
onCharacteristicDiscovered → readCharacteristic()
onCharacteristicRead → writeCharacteristic()
onCharacteristicWritten → （応答なしWrite）→ readDescriptor()
onDescriptorRead   → writeDescriptor()
onDescriptorWritten → discoverCharacteristic(live) → readCharacteristic(live)
```

**イベントは操作の種類ごとに1つで、対象ごとには分かれません。** 複数のCharacteristicを扱うと同じcallbackへ順番に結果が届くので、`result.characteristicUuid` か、区別できない場合はハンドルで対象を判定します。このexampleは2つのCharacteristicを読むため、`onCharacteristicRead` の中で分岐しています。

一覧Discoveryの結果は**接続ごとのsnapshot**として保持され、切断するか次の一覧Discoveryを行うまで有効です。`discoveredService*()` などはそのsnapshotへの照会で、無線を使いません。

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

## 注意

- **MTUを超える値は自動的に分割して読まれます。** ATTの1回の応答に収まらない値は、Clientが続きを要求して結合します（Read Long）。`result.value` には結合後の全体が入るので、アプリケーション側で組み立てる必要はありません。逆にこれを行わないと長い値が途中で切れるため、EspBleは常にこの読み方をします。
- **Writeは分割されません。** 書き込みはATTの1回の要求として送られ、Long Write（複数回に分けて書く手続き）は行いません。読み取りと非対称なのは、分割の可否が相手側の実装にも依存するためです。1回に載る上限はMTU − 3バイトで、`maximumNotificationPayload()` と同じ値です。

## 期待されるSerial出力

先頭の一覧Discoveryの件数は相手のGATT database（backendが用意する標準Serviceを含む）によって変わり、`Live:` の値は読み取り時点の `millis()` です。

```
Services: ..., characteristics: ..., descriptors: ...
Read: ready
Descriptor: EspBle value
Descriptor write complete
Live: 8421
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Client.ino](Client.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[Server](../Server/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

Client - Gatt/Server example へ接続し、CentralのGATT Clientフローを一通り実行する: Database一覧Discovery → 既知UUIDのDiscovery → Read → Write → Descriptor操作。 各ステップは要求APIで受理され、 完了は update() 経由のイベントとして届く。次の操作は前の操作の完了callbackから連鎖する （Central GATT操作は同時1件のため）。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.discoverServices(...)` — 相手のデータベース探索を開始します。要求直後にhandleがそろったと考えず、完了イベントで結果を使います。
4. `ble.onServicesDiscovered(...)` — 探索の成否と取得したデータベースを確認してから、Service・Characteristic・Descriptorのhandleを使います。
5. `ble.discoverCharacteristic(...)` — 対象のリモートCharacteristicを探し、対応する探索完了イベントから読み書きを続けます。
6. `ble.onCharacteristicDiscovered(...)` — 探索成功を確認してから進みます。接続しただけでは対象UUIDの存在まで保証されません。
7. `ble.readCharacteristic(...)` — 相手の値のReadをキューへ登録します。値は即時のboolではなくonCharacteristicReadへ届きます。
8. `ble.onCharacteristicRead(...)` — 成功・UUID・値の長さを確認してからデコードします。バイナリのStringには0バイトも含まれるので、長さを使って処理します。
9. `ble.writeCharacteristic(...)` — 相手へのWriteをキューへ登録し、完了イベントを確認します。Write Without Responseには、相手アプリの処理を保証するATT応答はありません。
10. `ble.onCharacteristicWritten(...)` — 指定したWrite方式での結果をresult.successで確認します。コマンドの実行確認が必要なら、アプリ側の応答も用意します。

## 確認と変更のポイント

汎用GATT Clientは探索してからCharacteristicやDescriptorを操作します。Descriptorは特定のCharacteristicに属するため、取得したhandleを接続と所有する値に対応付けて扱います。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
