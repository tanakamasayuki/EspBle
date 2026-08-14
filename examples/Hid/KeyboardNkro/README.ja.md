# KeyboardNkro

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

固定6キーReportの代わりに29-byteのNKRO（Nキーロールオーバー）ビットマップInput Reportを使うBLE HID keyboard（HID over GATT / HOGP）です。任意個数のキーを同時に報告でき、HID Service `0x1812` をadvertiseします。キー入力はSerialコマンドで発生させます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード

## 動作

- `configure()` より前に `enableNkro()` を呼び、keyboardを29-byteのNKROビットマップReportへ切り替えます
- preferred MTUに64を要求します（29-byteのNKRO Input ReportにはMTU ≥ 32が必要）
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- 切断のたびにadvertisingを再開します
- `n` で8キー同時押しを**1 report**として送信、`r` で全キーrelease

## 主なAPI

- `ble.hidKeyboard().enableNkro()` — `configure()` より前に呼ぶ必要があります
- `keyboard.configure()` — `begin()` 前にHID Serviceを構成
- `keyboard.ready()` — 購読済みHostが居て今送れるか
- `EspBleHidKeyboardNkroReport::press(usage)` — 送信するReport上で1つのHID usageを押下状態にする
- `keyboard.sendReport(nkroReport)` — NKROの全状態を1 notificationとして送信
- `keyboard.releaseAll()` — 全キーをクリア
- `config.preferredMtu = 64` — Reportに十分なMTUをネゴシエート

## メモ

- NKROビットマップReportのレイアウトはEspUsbDeviceと同じで、同一usageが同じようにマップされます。
- `keys[6]` を持つ通常の `sendReport()` はNKRO有効でも1回に6キーまでです。7キー以上を1回で送るには `EspBleHidKeyboardNkroReport` 版を使います。
- `EspBleHidKeyboardNkroReport` のbitmapが持てるのはusage `0x00`〜`0xDF` です。modifier usage `0xE0`〜`0xE7` は `press()` / `release()` が `modifiers` へ振り分けるため、呼び出し側でusageを区別する必要はありません。

## 期待されるSerial出力

```
Send 'n' for eight simultaneous keys, 'r' to release all.
```

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方
