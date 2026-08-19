# A2dpSinkAvrcp（Classic）

> English: [README.md](README.md)

A2DP SinkとAVRCPの組み合わせです。speakerやheadsetに必要な対で、A2DPが音楽、AVRCPが
ボタンと音量を運びます。**Classicは無印ESP32のみ**で動き、どちらのprofileも届く相手は
Classic機器——電話機、tablet、PCです。この側はTargetで、電話機が再生を押すとこの基板が
それを知ります。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- AVRCPを持つA2DP source 1台: 電話機・tablet・PC

## 何をするか

- backendの要求どおり、A2DPより先にAVRCPを開始する
- sourceが送るpassthrough keyを、押下と解放で分けて表示する
- 音量変化を表示し、remoteからの指示か自分側の変化かを区別する
- A2DPの接続を表示する

## 主なAPI

- `bluetooth.avrcp().begin()` — `a2dpSink().begin()`より**前に**呼ぶ
- `onPassthrough()` — 再生・停止・次曲などをkeyの押下／解放として受け取る
- `onVolumeChanged()` — `remoteCommand`で指示と報告を区別する

## 補足

**AVRCPはA2DPより先に開始します。**逆順にすると、sourceがaudio profileを確立する時点で
制御チャネルが使えません。

**このbuildでTargetが宣言できるnotificationは音量変化だけです。**同梱Classic hostが他を
許さず、`supportedNotifications()`が許可集合を返し、許可外の宣言は理由付きで拒否されます。
Targetとしてmetadataやplay statusを送るAPIはbackendに存在しません。この機器自身の音量を
sourceへ報告するには`bluetooth.avrcp().setAbsoluteVolume(value)`を呼びます——このsketchは
音量指示を受けるだけで、自分からの報告は行いません。

keyを受けるのではなく送る側になるなら、この機器はControllerです——
[AvrcpController](../AvrcpController/)を参照してください。

## 関連するガイド

- [Classic入門ガイド §7 A2DPとAVRCP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#7-a2dpとavrcp) — encode済みmedia、codec設定、操作
