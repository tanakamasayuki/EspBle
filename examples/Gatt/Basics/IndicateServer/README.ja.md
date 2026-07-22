# IndicateServer

> English: [README.md](README.md)

[NotifyServer](../NotifyServer/)のIndication版です。2秒ごとにカウンタ値を送信し、各配信はClientがATT層で確認応答します。確認結果は`onSent()`へ非同期に届きます。[Gatt/IndicateClient](../IndicateClient/)と組み合わせて使います。

値を取りこぼしてはいけない用途（状態遷移の通知など）にはIndicationを、確認応答の往復がスループットを律速するような高頻度ストリームにはNotificationを使います。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [Gatt/IndicateClient](../IndicateClient/) exampleを動かすESP32-S3 × 1

## 動作内容

- `begin()`前にRead + Indicate可能なCharacteristicを登録します
- `onSubscriptionChanged()`でCCCDのIndication購読を追跡し、購読者がいる間だけ送信します
- 2秒ごとに増加するカウンタを送り、Clientが配信を確認したかを表示します

## 主なAPI

- `EspBleGattCharacteristicConfig::indicatable` — Indicate propertyとCCCDを追加します
- `gattServer.indicate(serviceUuid, characteristicUuid, value)` — 同期的に受理され、確認待ちでloopをblockしません
- `gattServer.onSent(callback)` — Indicationでは`result.success`がClientの配信確認を意味します
- `subscription.indications` — 接続ごとのCCCD Indication状態

## 期待されるSerial出力

```
Indication confirmed: 1
Indication confirmed: 2
...
```
