# SppClient（Classic）

> English: [README.md](README.md)

SPPの接続する側です。serverは待って見つけられる側ですが、clientはaddressを知り、
その先のRFCOMM channelを解決する必要があります。[SppServer](../SppServer/)では
示せない部分がここにあります。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（client）
- SPP server 1台: [SppServer](../SppServer/)を動かす無印ESP32、またはserial serviceを
  公開するPCやAndroid機器

`serverAddress`にserverのaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- `connect(address)`はSDPで相手のSPP serviceのchannelを問い合わせる
- `connectToChannel(address, channel)`は既に分かっているchannelへdiscoveryを省いて
  接続する。相手が複数serviceを公開している場合はこちらが必要になる
- 接続成功と接続失敗を別々に受け取る。`connect()`の`true`は試行を開始したという
  意味に過ぎないため
- `0x00`を含むbyte列を送る。SPPは他の値と同様に運ぶ

## 主なAPI

- `bluetooth.spp().connect(address)` — channelを解決して接続する
- `bluetooth.spp().connectToChannel(address, channel)` — channelを指定して接続する
- `onConnectionFailed()` — 受理された後に失敗したことを受け取る
- `write(sessionId, data, length)` / `onData()` — message単位ではなくbyte stream

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | SDPでchannelを解決して接続 |
| `k` | channel 1へ直接接続 |
| `w` | 0を含む4 byteを送信 |
| `d` | 切断 |
