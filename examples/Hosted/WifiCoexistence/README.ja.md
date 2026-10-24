# WifiCoexistence

> English: [README.md](README.md)
> 準備: [ESP-Hostedセットアップ](../../../docs/ESP_HOSTED_SETUP.ja.md)

ESP32-P4でWi-FiとBLEを同時に使う例です。P4は自前の無線を持たず、ESP-Hosted経由で
ESP32-C6を通して両方を使います。1つのtransportを共有するため、内蔵controllerを持つ
SoCとは違い、開始と停止の順序が意味を持ちます。

ESP-Hosted構成でないtargetでは、buildを失敗させずにそう伝えるsketchになります——
そこには示すものがありません。

## 必要なもの

- SDIOでESP32-C6を繋いだESP32-P4 1台（このsketchを動かす）
- Wi-Fiネットワーク1つと、scanで見つかるBLE機器（何でもよい）

`WIFI_SSID`と`WIFI_PASSWORD`を設定するか、compiler defineで渡します。

## 動作

- Wi-Fiを先に開始し、それが共有transportも立ち上げる。順序はどちらでもよく、
  transportはどちらか一方の専有物ではない
- Wi-Fiが通信している最中にBLEをscanし、結果ごとにWi-Fiが接続を維持しているかを表示する
- `ble.end()`はBLEが持っている分だけを解放し、Wi-Fiとtransportは動かしたままにする
- transportは最後の利用者が居なくなった時点で解放される。だからWi-Fiを最後に止めることで
  初めて解放される

## Serialコマンド

| キー | 動作 |
|---|---|
| `b` | BLEだけ停止（Wi-Fiは継続） |
| `w` | BLEとWi-Fiを停止し、transportを解放 |

## 関連するガイド

- [ESP-Hostedセットアップ](../../../docs/ESP_HOSTED_SETUP.ja.md) — 配線・対応version・C6 firmware
- [ESP-Hostedの実機確認済み制限](../../../docs/ESP_HOSTED_LIMITATIONS.ja.md) — いま動かないものとその理由

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[WifiCoexistence.ino](WifiCoexistence.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

ESP32-P4でWi-FiとBLEを同時に使う例。P4は自前の無線を持たず、ESP-Hosted経由で ESP32-C6を通して両方を使う。1つのtransportを共有するため、内蔵controllerを 持つSoCとは違い、開始と停止の順序が意味を持つ。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。

## 確認と変更のポイント

Wi-Fiの接続情報をローカルで設定し、接続を維持したままBLEを探索します。まずBLE単独の停止、次に共有Transportの両利用者の停止を試します。無線側ファームウェアはHostedセットアップ資料に従います。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
