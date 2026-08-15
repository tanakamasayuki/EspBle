# HidMouse（Classic）

> English: [README.md](README.md)
> どちらの無線を使うか: [BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)

Bluetooth Classic（BR/EDR）のHID mouseです。呼び出しはBLE版の
[Hid/Mouse](../../Hid/Mouse/)と同じで、違うのは無線だけです。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID device）
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす
  無印ESP32（`onMouse()`が復号したeventを表示する）

## 動作

- `begin()`前にmouse profileを設定する
- Peripheral / pointing deviceのClass of Deviceを宣言し、Hostが未分類ではなく
  mouseとして扱うようにする
- Serialコマンドで移動、クリック、スクロール、ドラッグを行う

## 主なAPI

- `bluetooth.hidMouse().configure()` — `begin()`前にmouseを加える
- `move(dx, dy)` — 相対・符号付きの移動量
- `click(button)` — 押下と解放を1回の呼び出しで送る
- `wheel(delta)` — pointerを動かさず、押しているbuttonも保持したままスクロール
- `press(button)` / `releaseAll()` — 押下は加算される（ドラッグに必要）。
  `buttons()`で現在押している状態を読む

## Serialコマンド

| キー | 動作 |
|---|---|
| `m` | (+12, -8)移動 |
| `c` | 左クリック |
| `w` | 1段スクロール |
| `p` | 左を押しながら移動（ドラッグ） |
| `r` | すべてのbuttonを解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い
