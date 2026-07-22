# Mouse

> English: [README.md](README.md)

標準的な相対移動ポインタとボタンを公開するBLE HID mouse（HID over GATT / HOGP）です。HID Service `0x1812` をadvertiseし、移動やクリックはSerialコマンドで発生させます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード（`onMouse()` ハンドラでイベントを表示します）

## 動作

- `begin()` 前に `ble.hidMouse().configure()` を呼びHID Serviceを構成します
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- 切断のたびにadvertisingを再開します
- `m` でポインタを移動（+12, -8）、`c` で左ボタンをクリック

## 主なAPI

- `ble.hidMouse().configure()` — `begin()` 前にHID mouse serviceを構成
- `mouse.move(dx, dy)` — 相対移動Reportを送信
- `mouse.click(ESP_BLE_HID_MOUSE_LEFT)` — ボタンをpress/release

## 期待されるSerial出力

```
Send 'm' to move, 'c' to click.
```
