# Peer Tests

> English: [README.md](README.md)

ESP32-S3 2台を標準fixtureとしてBLEで接続する自動テストです。加えて、ESP32-P4 + ESP32-C6を親側DUT、ESP32-S3を無線PeerとしてESP-Hosted固有経路を回帰できます。S3と無線Peerの間に信号配線は不要です。

```sh
uv run --env-file .env pytest peer/
```

profileと環境変数はEspUsbHost/EspUsbDeviceの既存環境を再利用します。

| pytest上の位置 | profile | 環境変数 |
|---|---|---|
| 通常の親側 | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` |
| ESP-Hosted親側 | `p4_peer_host` | `TEST_SERIAL_PORT_P4_PEER_HOST` |
| 無印ESP32の親側 | `esp32_peer_host` | `TEST_SERIAL_PORT_ESP32_PEER_HOST` |
| 2台目Peer | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` |
| 2台目Peer（無印ESP32） | `esp32_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_ESP32_PEER_DEVICE` |

profile名はBLE roleを表しません。両側へsketchを転送・実行でき、両側のSerialをpytestから観測できます。1つのsuite内では親側sketchがCentral、`peer_device/`側sketchがPeripheralで固定です。どちらの物理boardをどちらへ置くかは`--profile` / `--peer-profile`で決めます。

`pytest peer/`の既定はS3 2台です。P4は常時接続せず、Hosted関連変更、Core/C6 firmware更新、リリース候補でprofileを明示して代表suiteを実行します。P4+C6の推奨標準配線、実行コマンド、頻度、既知制限による対象外は[tests README](../README.ja.md)と[テスト計画](../TEST_PLAN.ja.md#p4c6-esp-hosted回帰)を参照してください。

## 無印ESP32

無印ESP32はEspBleが同梱するNimBLE hostで動きます（[PLAN_ESP32.ja.md](../../docs/PLAN_ESP32.ja.md)）。機材は常設2台です。portはpytestが排他で掴むので、別のpytestを同時に走らせても待ち合わせになります（`arduino-cli upload`や`esptool`を直接使うと待機せずに失敗するので使わないでください）。無印ESP32はBluetooth Classicを持つ唯一のtargetでもあるため、Classicとdual-hostのsuiteもこの2台で動きます。

```sh
# 無印ESP32を親側(Central)、S3をPeerに
uv run --env-file .env pytest peer/gatt_read_write/ \
  --profile esp32_peer_host \
  --peer-profile device:s3_peer_device

# 無印ESP32をPeer(Peripheral)、S3を親側に
uv run --env-file .env pytest peer/hid_keyboard_device/ \
  --profile s3_peer_host \
  --peer-profile device:esp32_peer_device
```

esp32 profileは、その側のsketchがEspBleで書かれているsuiteすべてに置いてあります。profileを置いていない側は`--profile`指定時に自動でskipされ、該当するのは次の2つだけです。

- **core同梱`BLE`ラッパで書かれた側** — 無印ESP32ではラッパがBluedroidになり、EspBleが持ち込むNimBLE hostと同一controllerを共有できないため`#error`で拒否されます。親側は`stack_smoke`・`advertise_payload`・`hid_keyboard_device`・`midi_device`、Peer側は`stack_smoke`・`midi_host`です。**反対側はEspBleで書かれておりprofileがあります。**
- **`phy_update`** — 無印ESP32はBLE 4.2 controllerでLE 2M PHYを持ちません。

逆に`rpa_bond`とClassic・dual-hostのsuiteは無印ESP32専用で、S3のprofileを持ちません。

## suiteの構成

1 suite = 1 directoryです。

```text
peer/<suite>/
  <suite>.ino        親側sketch
  peer_device/       2台目sketch（1台で完結するsuiteには無い）
  sketch.yaml        buildするprofile（そのsuiteが対応するboard）
  test_<suite>.py    入力の生成と判定
```

**全suiteの目的と合格条件は[テスト計画](../TEST_PLAN.ja.md)が正本です。**ここでは重複させません。
suiteを追加するときは、両言語のテスト計画へも追記します（`tests/peer`の全suiteが両方に載っている
状態を保ちます）。

新しいsuiteを書くときの規則は、テスト計画の「起動banner待ちを避ける」と
「末尾の可変長fieldは行末で止める」を先に読んでください。どちらも実機で実際に落ちた形です。
