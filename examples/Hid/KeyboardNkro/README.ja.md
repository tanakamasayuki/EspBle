# KeyboardNkro

> English: [README.md](README.md)

固定6キーReportの代わりに29-byteのNKRO（Nキーロールオーバー）ビットマップInput Reportを使うBLE HID keyboard（HID over GATT / HOGP）です。任意個数のキーを同時に報告でき、HID Service `0x1812` をadvertiseします。キー入力はSerialコマンドで発生させます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード

## 動作

- `configure()` より前に `enableNkro()` を呼び、keyboardを29-byteのNKROビットマップReportへ切り替えます
- preferred MTUに64を要求します（29-byteのNKRO Input ReportにはMTU ≥ 32が必要）
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- 切断のたびにadvertisingを再開します
- `n` で8キー同時押し、`r` で全キーrelease

## 主なAPI

- `ble.hidKeyboard().enableNkro()` — `configure()` より前に呼ぶ必要があります
- `keyboard.configure()` — `begin()` 前にHID Serviceを構成
- `keyboard.pressUsage(usage)` — NKROビットマップ上で1つのHID usageを押下状態にする
- `keyboard.releaseAll()` — 全キーをクリア
- `config.preferredMtu = 64` — Reportに十分なMTUをネゴシエート

## メモ

- NKROビットマップReportのレイアウトはEspUsbDeviceと同じで、同一usageが同じようにマップされます。

## 期待されるSerial出力

```
Send 'n' for eight simultaneous keys, 'r' to release all.
```
