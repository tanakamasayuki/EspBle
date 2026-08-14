# HidComposite（Classic）

> English: [README.md](README.md)

keyboard、mouse、メディアキーを兼ねる1台のBluetooth Classic（BR/EDR）HID deviceです。
Classicはdevice recordを1つ登録するため、すべてが1つの合成Report Descriptorに入り、
profileごとにreport IDが分かれます。

**合成できるprofile数にはBLEには無い上限があります。**recordはdevice名などと共有する
300 byteのSDP padに収まる必要があり、この3つでdescriptorは144 byte、gamepadを加えると
212 byteで登録できません。`begin()`はそうした組み合わせを拒否します——誰も到達できない
deviceを起動しないためです。gamepadは[HidGamepad](../HidGamepad/)を単独で使ってください。BLE側の
[Hid/CompositeKeyboardMouse](../../Hid/CompositeKeyboardMouse/)と対になります。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- `begin()`前に3つのprofileを設定する。設定した内容がdescriptorを決めるため、後から
  追加してもHostが読み終えたrecordには入らない
- Class of Deviceは1つ選ぶ。複合deviceでも、Hostが提示する姿は1つに決まる
- Serialコマンドでprofileごとに1 reportを送る

## 主なAPI

- `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` — まとめて設定し、個別に送る
- 送信はそれぞれのreport IDで出るため、keyboardしか解釈しないHostは他を無視する

## Serialコマンド

| キー | 動作 |
|---|---|
| `k` | "hi"と入力 |
| `m` | pointerを移動 |
| `v` | 音量を上げる |
| `r` | keyboardとmouseを解放 |
