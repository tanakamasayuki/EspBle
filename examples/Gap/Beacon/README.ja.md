# Beacon

> English: [README.md](README.md)

manufacturer dataを載せたnon-connectable・non-scannableなbeaconをbroadcastします。connectableなPeripheralである[Advertise](../Advertise/)と違い、これは純粋なbroadcasterです。Centralから接続もscanもされず、設定した間隔でadvertising payloadを送信するだけです。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（broadcaster）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 動作

- manufacturer data（ここではcompany ID `0xFFFF` ＋小さなpayload）をnon-connectable・non-scannableなadvertisementとしてbroadcastします
- 設定した間隔で送信するだけです。スキャナからは`connectable = false`・`scannable = false`として見えます

## 主なAPI

- `ble.advertising().setConnectable(false)` — non-connectableモード（GATT接続不可）
- `ble.advertising().setScanResponseEnabled(false)` — non-scannable（純粋なbroadcaster。scan responseなし）
- `ble.advertising().setManufacturerData(data, length)` — broadcastするpayload
- `ble.advertising().setInterval(minMs, maxMs)` — advertising間隔（ミリ秒。20〜10240。non-connectableでは仕様上100 ms以上が必要）
- `ble.advertising().start()` — broadcast開始

## メモ

- manufacturer dataは必要に応じて自社の割当company IDやiBeaconレイアウトに置き換える。31 byteのlegacy advertising制限が適用される。
- 通常のconnectableなPeripheralにするには`setConnectable`を既定値（`true`）のままにする。

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
