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
