# CompositeKeyboardMouse

> English: [README.md](README.md)

keyboardとmouseを兼ねる複合BLE HID device（HID over GATT / HOGP）です。両profileを `begin()` 前に構成すると、固定Report ID 1（keyboard）と2（mouse）を持つ単一のHID Service（`0x1812`）へ合成されます。入力はSerialコマンドで発生させます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード

## 動作

- `begin()` 前に `ble.hidKeyboard().configure()` と `ble.hidMouse().configure()` を呼び、Report ID 1と2を持つ1つのHID Serviceへ合成します
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- 切断のたびにadvertisingを再開します
- `h` で "hello" と入力、`m` でポインタを移動（+10, +10）

## 主なAPI

- `ble.hidKeyboard().configure()` / `ble.hidMouse().configure()` — 複合serviceを作るため両方を `begin()` 前に呼ぶ
- `keyboard.write("hello")` — ASCII文字列を入力
- `mouse.move(dx, dy)` — 相対移動Reportを送信

## 期待されるSerial出力

```
Send 'h' to type hello, 'm' to move.
```
