# RadioSettings（Classic）

> English: [README.md](README.md)

Classicのsketchが選べる無線・linkの設定3つ——送信電力、page timeout、暗号鍵の最小長
——を扱います。どれもprofileの動作を変えないため、接続試行を通して見せます。page
timeoutを短くすると、居ない相手に対して`connect()`が諦めるまでの時間が短くなります。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台

相手は不要です。`absentAddress`は誰も応答しないaddressで、それがpage timeoutを
見えるようにします。実在のaddressに変えれば接続が成功する側を見られます。

## 何をするか

- BR/EDRの送信電力を範囲で設定する。電力制御はその範囲の中からpacketごとにlevelを
  選ぶためである。1つだけ渡す形は上下限を同じ値に固定する
- 接続の前にpage timeoutを1000 msへ短くする。居ない相手への試行が既定の5120 msでは
  なく約1秒で終わる
- 16 byteより短い暗号鍵を拒否する
- 無線が適用した値——対応するlevelへ丸めた後の値——を表示する

## 主なAPI

- `bluetooth.setTxPower(minimum, maximum)` / `setTxPower(dBm)` — -12〜+9 dBmの3 dB刻み。
  BLEの送信電力を設定する`EspBle::setTxPower()`とは別物
- `bluetooth.txPower(minimum, maximum)` — 無線が適用した範囲
- `bluetooth.setPageTimeout(milliseconds)` — 14〜40959 ms、既定5120 ms。次のpageから効く
- `bluetooth.pageTimeout()` — 確定した値。確定するまでは0
- `bluetooth.setMinimumEncryptionKeySize(bytes)` — 7〜16。以降に確立するlinkに効く

## Serialコマンド

| キー | 動作 |
|---|---|
| `p` | 送信電力の範囲とpage timeoutを表示 |
| `c` | `absentAddress`へ接続を試み、失敗までの時間を測る |

## 補足

`setPageTimeout()`の`true`は要求が受理されたという意味です。backendは自分のtaskで
確定させるため、確定が届くまで`pageTimeout()`は0を返します。controllerの既定値を知る
ためにlibraryが起動時に出す照会も同じです。

page timeoutを短くすると応答が遅れただけの相手を諦め、送信電力を下げると距離が
縮みます。どちらも改善ではなくtrade-offです。
