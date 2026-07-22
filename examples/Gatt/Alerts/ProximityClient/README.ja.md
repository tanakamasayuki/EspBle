# ProximityClient

> English: [README.md](README.md)

Proximityプロファイルの**Monitor**役。Proximity Reporterへ接続し、Tx Power Level（0x2A07、signed int8）をRead、Link Loss Alert Level（0x2A06）をRead、そしてLink Loss Alert Levelを書き込んで、後でリンクが切れたときReporterが鳴動するようにします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central / Proximity Monitor）
- 1 × Proximity Reporter: [ProximityServer](../ProximityServer/) example

## 動作

- 0x1803（Link Loss）をAdvertiseする機器をscanして接続
- Tx Power Level（0x2A07）をsigned int8（dBm）としてRead
- 現在のLink Loss Alert Level（0x2A06）をRead
- Link Loss Alert Level = 2（High Alert）を**応答ありWrite**して、link loss時の鳴動をarm

## 主なAPI

- 異なる2つのServiceにまたがる`ble.readCharacteristic(...)`
- `ble.writeCharacteristic(..., true)` — 応答ありWrite

## 期待されるSerial出力

```
Tx Power Level: -8 dBm
Link Loss Alert Level: 0
Armed Link Loss High Alert
```
