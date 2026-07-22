# ProximityServer

> English: [README.md](README.md)

Proximityプロファイルの**Reporter**役。2つの標準Serviceを同時にホストします: Link Loss Service（0x1803、read/writeのAlert Level 0x2A06）と、Tx Power Service（0x1804、read専用のsigned int8 Tx Power Level 0x2A07）。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral / Proximity Reporter）
- 1 × Central: [ProximityClient](../ProximityClient/) example、または Proximity Monitor

## 動作

- `begin()`の前に両Serviceを登録し、0x1803をAdvertise
- 固定のTx Power Level -8 dBmを提示
- 書かれたLink Loss Alert Levelを`onWritten`で保存し、read backできるようにする（実機のReporterはこのlevelに従いlink loss時に鳴動する）

## 主なAPI

- 1つのserverに対する2回の`addService(...)`
- `ble.gattServer().onWritten(...)` — Alert Levelの書き込みを受信

## 期待されるSerial出力

```
Link Loss Alert Level: 2 (High Alert)
```
