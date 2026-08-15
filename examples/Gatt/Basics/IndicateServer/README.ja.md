# IndicateServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

[NotifyServer](../NotifyServer/)のIndication版です。2秒ごとにカウンタ値を送信し、各配信はClientがATT層で確認応答します。確認結果は`onSent()`へ非同期に届きます。[Gatt/IndicateClient](../IndicateClient/)と組み合わせて使います。

値を取りこぼしてはいけない用途（状態遷移の通知など）にはIndicationを、確認応答の往復がスループットを律速するような高頻度ストリームにはNotificationを使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [Gatt/IndicateClient](../IndicateClient/) exampleを動かすESP32-S3 × 1

## 動作

- `begin()`前にRead + Indicate可能なCharacteristicを登録します
- `onSubscriptionChanged()`でCCCDのIndication購読を追跡し、購読者がいる間だけ送信します
- 2秒ごとに増加するカウンタを送り、Clientが配信を確認したかを表示します

## 主なAPI

- `EspBleGattCharacteristicConfig::indicatable` — Indicate propertyとCCCDを追加します
- `gattServer.indicate(characteristic, value)` — 同期的に受理され、確認待ちでloopをblockしません
- `gattServer.onSent(callback)` — Indicationでは`result.success`がClientの配信確認を意味します
- `subscription.indications` — 接続ごとのCCCD Indication状態

## 期待されるSerial出力

```
Indication confirmed: 1
Indication confirmed: 2
...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
