# Gamepad

> English: [README.md](README.md)
> 概念: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID」

GATT（HOGP）上のBLE HID gamepadです。符号付き6軸、hat switch、32 buttonを1つの
Input Reportで送ります。Classic側にも同じ呼び出しがあり
（[Classic/HidGamepad](../../Classic/HidGamepad/)）、旧世代のゲーム機やPCには
そちらが必要です（BR/EDR HIDしか受け付けないため）。

## 必要なもの

- このsketchを動かすESP32-S3 1台（HID device / peripheral）
- HID host 1台: PCや携帯、または[KeyboardHost](../KeyboardHost/)を動かす2台目
  （`onGamepad()`がeventを表示する）

## 動作

- `begin()`前に`ble.hidGamepad().configure()`でHID Serviceへ合成する
- bonding付きsecurityを有効にする（HOGPは暗号化linkを要求する）
- 切断ごとにadvertisingを再開する
- Serialコマンドでbutton、stick位置、hat方向を送る

## 主なAPI

- `ble.hidGamepad().configure()` — HID Serviceへgamepadを合成する
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

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方
