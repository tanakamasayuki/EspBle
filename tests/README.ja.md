# Tests

> English: [README.md](README.md)

`pytest-embedded`とArduino CLI backendを利用するEspBleのテストです。方針とカバレッジは[テスト計画](TEST_PLAN.ja.md)を参照してください。

```text
unit/   host上で実行する純粋C++/データ変換テスト（実機不要）
peer/   ESP32-S3標準回帰、およびP4+C6 ESP-Hosted回帰のBLE実機テスト
```

| 構成 | 用途 | 普段の扱い |
|---|---|---|
| ESP32-S3 + ESP32-S3 | 全機能の標準回帰 | 常時接続を推奨。`pytest peer/`の既定構成 |
| ESP32-P4 + ESP32-C6、Peer ESP32-S3 | SDIO/ESP-Hosted/C6 controllerとWi-Fi/BLE共存の回帰 | 1組を必要時に接続。明示的にP4 profileを指定 |

P4向けcompileだけではESP-Hostedの実通信経路を検証できず、S3だけではHosted固有の初期化やSDIOの不具合を検出できません。そのためP4実機は必要ですが、常時接続は不要です。Hosted関連変更ごと、Arduino-ESP32 Core/C6 firmware更新時、リリース候補で実行します。詳しい頻度と合格範囲は[テスト計画](TEST_PLAN.ja.md#p4c6-esp-hosted回帰)、準備は[ESP-Hostedセットアップ](../docs/ESP_HOSTED_SETUP.ja.md)、除外理由は[既知制限](../docs/ESP_HOSTED_LIMITATIONS.ja.md)を参照してください。

examplesのbuild回帰はGitHub Actions（`.github/workflows/compile-examples.yml`）が全exampleをesp32s3 profileでコンパイルして検出します。OSや市販BLE機器との相互運用はmanualテストとして初回リリース前に実施します。

## セットアップ

```sh
cp .env.example .env
uv sync
```

`.env`には既存のEspUsbHost/EspUsbDevice環境と同じprofile由来の変数名を使用します。

```dotenv
TEST_SERIAL_PORT_S3_PEER_HOST=/dev/ttyUSB0
TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE=/dev/ttyUSB1
TEST_SERIAL_PORT_P4_PEER_HOST=/dev/ttyUSB2
TEST_WIFI_SSID=example-test-ssid
TEST_WIFI_PASSWORD=example-test-password
```

`host`と`device`はpytest-embedded-cli上で親側と2台目Peerを識別する既存名です。BLEのCentral/Peripheral roleを意味しません。両方へsketchを転送して実行でき、両方のSerialをpytestから観測・操作できます。初期テストでは親側をCentral、Peer側をPeripheralに固定します。

## 実行

```sh
uv run --env-file .env pytest          # unit + peer全部
uv run --env-file .env pytest unit/
uv run --env-file .env pytest peer/
uv run --env-file .env pytest peer/stack_smoke/
```

P4+C6 fixtureには、C6搭載済みのEspressif ESP32-P4-Function-EV-Board、またはArduino-ESP32の汎用`esp32p4` variantが想定するものと同じ標準SDIO配線を推奨します。これならboard固有のpin上書きなしで共通条件を再現できます。Tab5や独自配線でも実行できますが、正しいvariantまたは`hostedSetPins()`が必要です。pin条件は[SDIO pinの選択と上書き](../docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)を参照してください。

P4を`.env`の`TEST_SERIAL_PORT_P4_PEER_HOST`、S3をPeerとして短い疎通testを実行する例:

```sh
uv run --env-file .env pytest peer/connect_disconnect/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

リリース前の代表suite:

```sh
uv run --env-file .env pytest \
  peer/stack_smoke/ peer/connect_disconnect/ peer/gatt_read_write/ \
  peer/notify_indicate/ peer/mtu/ peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

P4のWi-Fi/BLE共存testはP4 portとWi-Fi情報を`.env`から取得するため、単独では次の形でも実行できます。

```sh
uv run --env-file .env pytest peer/wifi_ble_coexistence/
```

Wi-Fi情報はcompile-time defineとして渡され、verboseなArduino CLI compile commandには
表示され得る。公開されてもよいテスト専用APを使用し、実運用の認証情報は設定しない。

現行Core/ESP-Hostedの既知制限に該当するSecurityと完全な初期化・終了反復は、P4代表suiteの必須合格項目から除外しています。上流versionを更新したときに再実行し、制限が解消したか確認します。

初回の`stack_smoke`は、親側をCentral、`peer_device/`側をPeripheralとしてArduino-ESP32同梱NimBLE backendのBLE APIだけで接続します。EspBle公開API実装前に、2台のポート、書き込み、無線接続、双方のSerial、テストfixtureが動くことを確認する基盤テストです。
