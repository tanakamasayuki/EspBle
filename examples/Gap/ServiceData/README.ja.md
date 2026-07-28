# ServiceData

> English: [README.md](README.md)

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
- 5秒ごとに値を更新します。Legacy advertisingにはpayloadをその場で書き換える手段がないため、`stop()` → `setServiceData()` → `start()` で載せ直します
- non-connectable・non-scannableのbroadcasterとして動作します

## 主なAPI

- `ble.advertising().setServiceData(uuid, data, length)` — Service Dataブロックを設定する。UUIDのサイズに応じてAD type 0x16 / 0x20 / 0x21が選ばれる
- `ble.advertising().addServiceUuid(uuid)` — service-UUID一覧にも同じUUIDを載せ、受信側の`advertisesService()`で絞り込めるようにする
- `ble.advertising().setConnectable(false)` / `setScanResponseEnabled(false)` — 純粋なbroadcasterにする
- 受信側: `scanResult.serviceData` / `scanResult.serviceDataUuid` / `scanResult.hasServiceData()`

## 注意

- Service Dataは31byteのlegacy advertising payloadを消費します。UUIDが128bitだとそれだけで16byte使うため、独自UUIDで大きなpayloadを載せることはできません。
- EspBleが受信時に取り出すのは**最初のService Dataブロック1つ**です。`EspBleScanResult` がService Dataを1組（`serviceData` と `serviceDataUuid`）しか持たないためで、複数ブロックを載せた相手では2つ目以降が読めません。
- 受信側の `serviceDataUuid` は、送信側が16bit表記（`181A`）で指定していても**128bitのフル形**で返ります（`0000181a-0000-1000-8000-00805f9b34fb`）。文字列比較ではなく `advertisesService()` のような値比較を使ってください。
- 値の更新のたびにadvertisingを止めて再開するため、その瞬間だけ放送が途切れます。数百ミリ秒ごとの更新には向きません。

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
