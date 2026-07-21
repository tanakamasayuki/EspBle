# Beacon

> English: [README.md](README.md)

manufacturer dataを載せた**non-connectable・non-scannableなbeacon**をbroadcastします。connectableなperipheralである [Gap/Advertise](../Advertise/) と違い、これは純粋なbroadcasterです。centralから接続もscanもされず、設定した間隔でadvertising payloadを送信するだけです。

## ハードウェア

- ESP32-S3 × 1（このスケッチ。broadcaster）
- BLEスキャナ × 1（2枚目のボードで [Gap/Scan](../Scan/) を動かす、またはnRF Connect等のスキャナアプリ）

## 動作

- `setConnectable(false)` — non-connectableモードでadvertise（GATT接続不可）
- `setScanResponseEnabled(false)` — non-scannable（純粋なbroadcaster。scan responseなし）
- `setManufacturerData(...)` — broadcastするpayload（ここではcompany ID `0xFFFF` ＋小さなpayload）
- `setInterval(100, 150)` — advertising間隔（ミリ秒。20〜10240 ms。non-connectableではBLE仕様上 100 ms 以上が必要）

## 補足

- manufacturer dataは必要に応じて自社の割当company IDや iBeacon / Eddystone のレイアウトに置き換える。31 byteのlegacy advertising制限が適用される。
- スキャナからは `connectable = false`・`scannable = false` として見える。
- 通常のconnectableなperipheralにするには `setConnectable` を既定値（`true`）のままにする。
