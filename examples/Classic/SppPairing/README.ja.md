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

## 関連するガイド

- [Classic入門ガイド §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#4-spp) — RFCOMM channel、複数service、byte stream
- [EspBleを深く使う](../../../docs/GUIDE_ADVANCED.ja.md) — 990 byteのpacket、8件の送信queue、backpressure

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[SppPairing.ino](SppPairing.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

applicationがClassic pairingを制御する例。security未設定ではJust Worksで 全要求を自動承諾する。閉じた環境なら十分だが、第三者が届く場所では不適切。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.confirmNumericComparison(...)` — 両側の表示値を比較してから承認します。不一致は拒否し、イベントを自動承認しないようにします。
2. `bluetooth.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
3. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。

## 確認と変更のポイント

表示された数値を比較してから承認し、セキュリティ結果を確認します。Classicのbondが保存されていると、再起動後に新たな確認が出ない場合があります。再試験ではbond管理を意図して行います。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
