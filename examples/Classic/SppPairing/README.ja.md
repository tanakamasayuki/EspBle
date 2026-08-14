# SppPairing（Classic）

> English: [README.md](README.md)

SPP serverを使って、applicationがClassic pairingを制御する例です。security未設定では
Just Worksで全要求を自動承諾します。閉じた環境なら十分ですが、第三者が届く場所では
不適切です。**Classicは無印ESP32のみ**で動きます。Classic pairingが作るのはlink keyで、
BLEのbondとは別物です——片方を削除しても他方は残ります。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- pairingする相手 1台: PCやAndroid、または[SppClient](../SppClient/)を動かす無印ESP32

## 何をするか

- IO capabilityを`DisplayYesNo`にしてsecurityを有効にする。両者が同じ数字を比較する構成
- numeric comparisonをapplicationで応答する（ここでは自動承諾。実製品ではユーザーへ確認する）
- pairingの成否をbackendのstatus付きで表示する
- 起動時にNVSへ保存されているbondを一覧する

## 主なAPI

- `EspBleClassicConfig::security` — `enabled`と`ioCapability`。どちらも`begin()`で読まれる
- `onNumericComparisonRequested()` / `confirmNumericComparison()` — 問い合わせと応答
- `onPasskeyDisplayed()` / `onPasskeyRequested()` / `providePasskey()` — 他のIO capability
- `bondCount()` / `bond(index)` / `deleteBond()` / `deleteAllBonds()`

## 補足

**`security.enabled`が無効だとIO capabilityは効きません。**Secure Simple Pairingは
serviceが要求したときだけapplicationを介するため、securityを有効にすることでSPP service
がMITM保護を要求するようになります。

無応答は`responseTimeoutMilliseconds`経過で拒否になります。相手を永久に待たせるより
断る方がよいという判断です。

legacy PIN pairingは拒否します。応答経路が無く、固定PINは固定鍵になるためです。
