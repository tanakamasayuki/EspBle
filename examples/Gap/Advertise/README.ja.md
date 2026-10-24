# Advertise

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

デバイス名と16-bit Service UUID（HID、`1812`）を載せたconnectableなLegacy Advertisingを開始します。Peripheral側の最小例です。汎用BLEスキャナアプリか、2台目のボードで組み合わせる[Scan](../Scan/) exampleで確認できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 動作

- デバイス名`EspBle Advertiser`でスタックを初期化します
- local nameとHID Service UUID `1812`を、サイズごとに単一のComplete List AD構造へまとめてadvertiseします（CSS Part A 1.1）
- リセットするまでadvertiseし続けます。Centralからの接続要求はスタックが受け入れます

## 主なAPI

- `ble.begin(config)` — スタック初期化。`config.deviceName`がGAPデバイス名になります
- `ble.advertising().setName(name)` — advertising payloadへlocal nameを載せます
- `ble.advertising().addServiceUuid(uuid)` — Service UUIDを掲載（サイズごとに単一のComplete Listへまとめます）
- `ble.advertising().setChannelMap(mask)` — 使うadvertisingチャネルを絞ります（`EspBleAdvertisingChannel37/38/39` のビットマスク、0で3チャネルすべて）。Wi-Fiと重なるチャネルを避けられる代わりに、見つけてもらうまでの時間は延びます
- `ble.advertising().start()` — connectableなLegacy Advertisingを開始。payloadが31 bytesを超える場合は`InvalidArgument`で失敗します
- `ble.lastErrorName()` / `ble.lastErrorDetail()` — 要求が拒否された理由

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Advertise.ino](Advertise.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[Scan](../Scan/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

Advertise - デバイス名とService UUIDを載せたconnectableなLegacy Advertisingを開始する。 Peripheral側の最小例。汎用BLEスキャナアプリやGap/Scan exampleで受信できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

広告でこの機器を発見可能にします。広告中のService UUIDは探索の手掛かりで、載せるだけで読み取り可能なGATT Characteristicが作られるわけではありません。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
