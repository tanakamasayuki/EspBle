# MultiConnection

> English: [README.md](README.md)
> 概念: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md)

1つのCentralが複数のPeripheral接続を同時に保持する例です。接続ごとにIDがあり、
すべての操作は対象の接続を明示します。このlibraryに「現在の接続」という暗黙の対象は
ありません。

## 必要なもの

- このsketchを動かすESP32-S3 1台（Central）
- Battery Service `0x180f`をadvertiseするperipheral 2〜3台。例えば
  [Gatt/Device/BatteryServer](../../Gatt/Device/BatteryServer/)を動かすboard

## 動作

- 接続ごとにscanを再開し、最初の1台で止めない。満杯になったら収集をやめる——
  さもないと満杯のCentralがhostに拒否される接続を試み続ける
- 各peerからbattery levelを読む。共通の結果callbackが、どの接続の応答かを持っている
- 切断時はIDの一致でエントリを外す。IDは1つの接続に一生対応する

## 主なAPI

- `ble.connectionCount()` — 現在保持している数
- `ble.readCharacteristic(connectionId, service, characteristic)` — 接続が
  呼び出しの一部になっている
- `ble.onCharacteristicRead()` — 全接続で共通のcallback。結果が接続IDを持つ

## 制限

無印ESP32の同梱hostでは同時接続3までです。他のtargetはもっと多く扱えます。

## Serialコマンド

| キー | 動作 |
|---|---|
| `r` | 各peerのbattery levelを読む |
| `d` | 最初のpeerを切断 |

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続
