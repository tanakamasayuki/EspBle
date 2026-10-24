# Server

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Read/Write可能なCharacteristicとDescriptorを1つずつ持つ独自GATT Serviceを登録し、advertiseします。Characteristicは応答あり/なし両方のWriteに対応します。あわせて、**読まれた瞬間に値を作る**Characteristicも1つ持ちます。

2台目のボードで[Gatt/Client](../Client/) example（同じUUIDを対象にしています）を動かすか、nRF Connectなどの汎用GATT Clientアプリから操作できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- GATT Client × 1（Gatt/Clientを動かす2台目のボード、またはスマートフォンアプリ）

## 動作

- `begin()`前にService `10da4dd0-…`、Characteristic `10da4dd1-…`、Descriptor `10da4dd2-…`、Read専用の`10da4dd3-…`を登録します
- 初期値を`ready`に設定します
- Clientからの書込みをConnection IDと一緒に表示します
- `10da4dd3-…`が読まれたときは、その場で `millis()` を値にして返します
- ClientがみつけられるようにService UUIDをadvertiseします

## ハンドルで組み立てる

登録は**3段のハンドル連鎖**になります。`addService()` が返すハンドルを `addCharacteristic()` に渡し、それが返すハンドルを `addDescriptor()` に渡す、という形です。

```cpp
const EspBleGattService service = gattServer.addService(SERVICE_UUID);
characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
descriptor = gattServer.addDescriptor(characteristic, DESCRIPTOR_UUID, descriptorConfig);
```

以降の値設定・送信・イベント判定はすべてこのハンドルで行い、UUIDでは指定しません。**UUIDは「型」であって「どれか」を表さない**ためです。仕様上、1台が同じUUIDのServiceを複数持てますし、Client側から見れば同じUUIDのCharacteristicが並ぶ相手（HIDのReportなど）も普通にあります。

ハンドルはグローバル変数などに保持してください。失敗すると無効なハンドルが返るので、`valid()` で判定できます。

## 読まれた瞬間に値を作る

`setValue()` で先に値を置いておく形は、値が変わったときにこちらが更新できる場合に向きます。センサーのように「読まれた時点の値」を返したい場合は、`onRead()` を使います。

```cpp
gattServer.onRead([](const EspBleGattReadRequest &request) {
  if (request.characteristic != liveCharacteristic) return;
  ble.gattServer().setValue(liveCharacteristic, String(millis()));
});
```

コールバックの中で `setValue()` した値が、そのまま相手へ返ります。定期的に `setValue()` を呼び続ける必要がなくなり、**誰も読まないなら値を作る処理自体が走りません**。

**このコールバックだけは `update()` ではなくBLEスタックのタスクで走ります。** ATTの読み取り応答を返す前に値が必要で、後回しにできる場所がないためです。したがって次の2点に注意してください。

- **短く保つこと。** ここで待たせるとスタック全体が止まり、相手からは読み取りのタイムアウトに見えます。Serial出力もこの中では避けてください
- **`loop()` と同時に走ります。** 共有変数を触るなら、他のコールバックと違って排他制御が必要です

## 主なAPI

- `ble.gattServer().addService(uuid)` — Serviceを登録してハンドルを返す。`begin()`前に呼ぶ必要があります
- `addCharacteristic(service, uuid, config)` — Serviceのハンドルを渡してCharacteristicを登録し、そのハンドルを返します
- `EspBleGattCharacteristicConfig` — `readable`、`writable`のほか`notifiable`、`indicatable`、暗号化/認証permission
- `addDescriptor(characteristic, uuid, config)` / `EspBleGattDescriptorConfig` / `setDescriptorValue(descriptor, value)` — Descriptor定義、permission、binary-safeな値
- `gattServer.setValue(characteristic, value)` / `gattServer.value(characteristic, out)` — 保持値（binary-safeな`String`。pointer+length overloadもあります）
- `gattServer.onWritten(callback)` — `connectionId`、書き込まれたCharacteristicのハンドル、値を持つ`EspBleGattWrite`
- `gattServer.onRead(callback)` — 読み取り要求。`EspBleGattReadRequest`は`connectionId`と対象のハンドルを持ちます
- `gattServer.onDescriptorWritten(callback)` — Descriptorのハンドルと値を持つ`EspBleGattDescriptorWrite`

## 注意

- **コールバックは全Characteristic共通です。** 複数登録している場合は `write.characteristic == myHandle` で対象を判定してください。イベントにはUUID文字列も入っていますが、同じUUIDが複数あると区別できないため、ハンドルで比べるのが確実です。
- **1つのServiceの中に同じUUIDのCharacteristicを複数置けます。** 仕様が認めている重複で、HIDのReportがその典型です。属性テーブルをBLEスタックのAPIで直接組み立て、操作もイベントもハンドルで対象を名指しするため取り違えません。ただし**再接続時の購読自動復元だけはUUIDを手掛かりにする**ため、同じUUIDが複数ある場合は復元されません（再接続後にハンドルを指定して購読し直してください）。
- **`onRead()` は1つだけです。** 他のイベントのように `add*Listener()` で複数登録することはできません。「返す値を作る」責任を持てるのは1箇所だけだからです。
- **MTUを超える値は分割して読まれます。** ATTの1回の応答に収まらない場合、Clientは続きを要求します（Read Long）。Server側は値を置くだけで、分割はスタックが扱います。
- 登録はすべて `begin()` より前に行う必要があります。`begin()` 後の `addService()` は `InvalidState` で失敗します。

## 期待されるSerial出力

```
Connection 1 wrote: hello from Central
Descriptor 10da4dd2-8eaa-4c69-9003-676174747277 wrote: descriptor value
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Server.ino](Server.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[Client](../Client/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

Server - Read/Write可能なCharacteristicを1つ持つ独自GATT Serviceを公開する。 GATT Server構成はすべて begin() 前に登録する（backendはserver開始後の Service追加ができないため）。Gatt/Client example や汎用GATTアプリから操作できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `gattServer.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
5. `gattServer.onRead(...)` — この同期GATT読み取りコールバックはBLEスタックのタスクで動きます。setValueで素早く値を用意し、待機、大量の表示、遅いデバイス入出力を行わないようにします。
6. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
7. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

書き換え可能な保存値と、読まれた時点で作る値では処理経路が異なります。onReadはスタック側で短く処理し、相手の書き込みに伴うアプリ処理にはupdate側のonWrittenを使います。

`ble.update()` をloopで継続して呼びます。上記のスタック／Host側コールバックは例外なので、updateから呼ばれると考えず、その場で必要なコピーや応答だけを短く処理します。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
