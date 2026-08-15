# Inquiry（Classic）

> English: [README.md](README.md)

Bluetooth Classicのdevice discoveryです。他のClassic profileはaddressを指定して接続する
ため、addressを知らない場合の入手経路がここになります。**Classicは無印ESP32のみ**で動き、
見つかるのはClassic機器だけです——BLEしか持たないperipheralはinquiryに応答せず、BLEのscan
（`EspBle::scanner()`）ではClassic専用機器は見つかりません。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- 発見可能なClassic機器: Bluetooth設定画面を開いた電話機、pairing modeのheadset、または
  [SppServer](../SppServer/)を動かす無印ESP32

## 何をするか

- 約10秒間のinquiryを1回実行し、結果を表示する
- name、Class of Device、RSSIは応答に含まれていたときだけ表示する。inquiry結果に名前が
  入らないことは珍しくない
- 終了は別途通知し、`stop()`による中断と時間切れを区別する

## 主なAPI

- `bluetooth.inquiry().start(config)` — 探索開始。`true`は開始したという意味で、完了では
  ない
- `onResult()` — 応答1件につき1回。同じpeerが2回応答すれば2回届くので、必要ならaddressで
  重複を除く
- `onComplete()` — `cancelled`で`stop()`と時間切れを区別する
- `bluetooth.inquiry().requestName(address)` / `requestServices(address)` — 相手へ直接
  問い合わせる。名前が無い場合やservice一覧が欲しい場合に使う。どちらもinquiry実行中は
  応答が来ない

## 補足

`durationSeconds`は切り上げられます。controllerは1.28 s単位で数えるため、10は10.24 sに
なります。

接続できる機器が必ずinquiryで見つかるとは限りません。`ConnectableOnly`の相手は結果に
出ませんが、addressを知っている側からは接続できます。

## 関連するガイド

- [Classic入門ガイド §3 Inquiry](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#3-inquiry) — scanが返すもの、addressを取る照会
