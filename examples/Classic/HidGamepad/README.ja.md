# HidGamepad（Classic）

> English: [README.md](README.md)
> どちらの無線を使うか: [BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)

Bluetooth Classic（BR/EDR）のHID gamepadです。**Classicが必要になる代表例**で、
旧世代のゲーム機やPCはClassic HIDしか受け付けず、BLEでは代替できません。呼び出しは
BLE版の[Hid/Gamepad](../../Hid/Gamepad/)と同じで、違うのは無線とHostからの見つけ方だけです。

**Classicは無印ESP32のみ**で動きます。ESP32-S3/C3/C6/H2/P4はBR/EDRの無線を持ちません。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID device）
- Classic HID host 1台: ゲーム機、PC、または
  [HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32（raw reportの表示で確認できる）

## 動作

- gamepad profileを`begin()`より前に設定する。合成したReport Descriptorは、Hostが
  pairing時に読むdevice recordの一部になるため
- Peripheral / gamepadのClass of Deviceを宣言する。Hostがiconを選び、機種によっては
  接続を提案するかどうかを決める値
- Serialコマンドでbutton、stick、hatのreportを送る

## 主なAPI

- `bluetooth.hidGamepad().configure()` — `begin()`前に合成HID deviceへgamepadを加える
- `send(x, y, z, rz, rx, ry, hat, buttons)` — 軸の値から1 reportを送る
- `sendReport(report)` — `EspBleHidGamepadReport`を埋めて送る同等の呼び出し
- `releaseAll()` — 全軸中央・全button解放。Hostは送った最後のreportを保持し続けるため、
  入力を止めるにはこれを送る

## Serialコマンド

| キー | 動作 |
|---|---|
| `a` | button 1 |
| `b` | button 2 |
| `d` | 左stickを右上、hatは上、button 1 |
| `r` | すべて解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い
