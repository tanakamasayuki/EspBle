# ServiceData

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

Service Data（AD type 0x16）を載せたadvertisingを行う例です。Service Dataは「どのserviceの値か」をUUIDで示したpayloadで、**センサーが接続させずに値を配る**ときの標準的な方法です。

[Beacon](../Beacon/)が使うManufacturer Dataとの違いは次の点です。

| | Service Data | Manufacturer Data |
|---|---|---|
| 意味づけ | UUIDが示すserviceの仕様に従う | ベンダー独自。解釈にはcompany IDの知識が必要 |
| 必要な割り当て | SIG割り当てUUID（または自分の128bit UUID） | Bluetooth SIGから割り当てられたcompany ID |
| 向いている用途 | 標準serviceの値の放送 | 独自フォーマット、iBeaconなどのベンダー定義 |

このexampleは Environmental Sensing Service（`0x181A`）のUUIDで温度を放送します。payloadはGATT characteristicと同じwire形式（0.01度単位の符号付き16bit・little-endian）なので、受け取る側は接続したときと同じデコードが使えます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Broadcaster）
- 受信側 — [Info/ScanDump](../../Info/ScanDump/)を動かす2台目のボード、またはnRF Connect等のスキャナアプリ

## 動作

- Environmental Sensing Service（`0x181A`）のService Dataとして温度を放送します
- 5秒ごとに値を更新します。Legacy advertisingにはpayloadをその場で書き換える手段がないため、`stop()` → `addServiceData()` → `start()` で載せ直します。同じservice UUIDへ再度addするとそのブロックが**置き換わる**（2つ目が足されるのではない）ため、payloadは増えていきません
- non-connectable・non-scannableのbroadcasterとして動作します

## 主なAPI

- `ble.advertising().addServiceData(uuid, data, length)` — Service Dataブロックを追加する。UUIDのサイズに応じてAD type 0x16 / 0x20 / 0x21が選ばれる。UUIDを変えて最大4ブロックまで載せられ、同じUUIDで呼び直すと差し替えになる。データを渡さなければそのブロックを削除する
- `ble.advertising().addServiceUuid(uuid)` — service-UUID一覧にも同じUUIDを載せ、受信側の`advertisesService()`で絞り込めるようにする
- `ble.advertising().setConnectable(false)` / `setScanResponseEnabled(false)` — 純粋なbroadcasterにする
- 受信側: `scanResult.serviceData[]` / `serviceDataCount` / `hasServiceData()`、UUIDで引く `scanResult.serviceDataFor(uuid, data)`

## 注意

- Service Dataは31byteのlegacy advertising payloadを消費します。UUIDが128bitだとそれだけで16byte使うため、独自UUIDで大きなpayloadを載せることはできません。
- 1つのadvertisementに複数のService Dataブロックを載せられます（送受信とも最大4ブロック）。順序に依存せず取り出すには、添字ではなく `serviceDataFor()` でUUIDから引いてください。
- 受信側の `uuid` は、送信側が16bit表記（`181A`）で指定していても**128bitのフル形**で返ります（`0000181a-0000-1000-8000-00805f9b34fb`）。自分で文字列比較すると一致しないので、値として比較する `serviceDataFor()` を使ってください。
- 値の更新のたびにadvertisingを止めて再開するため、その瞬間だけ放送が途切れます。数百ミリ秒ごとの更新には向きません。
- **受信側は重複報告を有効にしないと最初の値しか見えません。** スキャナは既定で1つの機器につき1回しか報告しないためです（`EspBleScanConfig::wantDuplicates = true`）。[Info/ScanDump](../../Info/ScanDump/)なら `d` で切り替えられます。

## 期待されるSerial出力

```
Broadcasting 23.50 degC
Broadcasting 23.75 degC
Broadcasting 24.00 degC
```

[Info/ScanDump](../../Info/ScanDump/)側では次のように見えます。

```
d0:cf:13:58:fd:95 type=0 rssi=-13 uuid=0000181a-0000-1000-8000-00805f9b34fb servicedata[0000181a-0000-1000-8000-00805f9b34fb][2]=c409
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ServiceData.ino](ServiceData.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ScanDump](../../Info/ScanDump/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

ServiceData - Service Dataブロックを放送する。どのserviceの値かをUUIDで示した payloadで、センサーが「接続させずに値を配る」ときの標準的な方法。Manufacturer Data と違い、割り当て済みcompany IDが不要。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

Service UUIDはデータの種類を示しますが、独自項目の並びまで自動的に決めるものではありません。温度の倍率とバイト順を合わせ、変化を見るスキャナでは重複結果を有効にします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
