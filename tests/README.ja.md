# Tests

> English: [README.md](README.md)

`pytest-embedded`とArduino CLI backendを利用するEspBleのテストです。方針とカバレッジは[テスト計画](TEST_PLAN.ja.md)を参照してください。

```text
unit/   host上で実行する純粋C++/データ変換テスト（実機不要）
peer/   ESP32-S3 2台のBLE接続自動テスト
```

ESP32-P4 + ESP32-C6 (ESP-Hosted) は、P4を親側DUT、S3をPeer側として代表testを
実行できる。対応状況と既知制限は
[ESP-Hosted対応 実行計画](../docs/PLAN_ESP_HOSTED.ja.md)を参照する。

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

P4を`/dev/ttyUSB2`、`.env`で設定済みのS3をPeerとして実行する例:

```sh
uv run --env-file .env pytest peer/connect_disconnect/ \
  --profile p4_peer_host --port /dev/ttyUSB2 \
  --peer-profile device:s3_peer_device
```

P4のWi-Fi/BLE共存testはP4 portとWi-Fi情報を`.env`から取得するため、通常の形で実行できる。

```sh
uv run --env-file .env pytest peer/wifi_ble_coexistence/
```

Wi-Fi情報はcompile-time defineとして渡され、verboseなArduino CLI compile commandには
表示され得る。公開されてもよいテスト専用APを使用し、実運用の認証情報は設定しない。

初回の`stack_smoke`は、親側をCentral、`peer_device/`側をPeripheralとしてArduino-ESP32同梱NimBLE backendのBLE APIだけで接続します。EspBle公開API実装前に、2台のポート、書き込み、無線接続、双方のSerial、テストfixtureが動くことを確認する基盤テストです。
