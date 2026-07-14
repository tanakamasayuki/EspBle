# Tests

[English](README.md)

`pytest-embedded`とArduino CLI backendを利用するEspBleのテストです。

```text
unit/              host上で実行する純粋C++/データ変換テスト
examples_compile/  examplesのbuild-only test
single/            ESP32-S3 1台のruntime test
peer/              ESP32-S3 2台のBLE接続自動テスト
manual/            OSや市販BLE機器を使う手動相互運用テスト
```

## セットアップ

```sh
cp .env.example .env
uv sync
```

`.env`には既存のEspUsbHost/EspUsbDevice環境と同じprofile由来の変数名を使用します。

```dotenv
TEST_SERIAL_PORT_S3_PEER_HOST=/dev/ttyUSB0
TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE=/dev/ttyUSB1
```

`host`と`device`はpytest-embedded-cli上で親側と2台目Peerを識別する既存名です。BLEのCentral/Peripheral roleを意味しません。両方へsketchを転送して実行でき、両方のSerialをpytestから観測・操作できます。初期テストでは親側をCentral、Peer側をPeripheralに固定します。

## 実行

```sh
uv run --env-file .env pytest
uv run --env-file .env pytest peer/
uv run --env-file .env pytest peer/stack_smoke/
```

初回の`stack_smoke`は、親側をCentral、`peer_device/`側をPeripheralとしてArduino-ESP32同梱NimBLE backendのBLE APIだけで接続します。EspBle公開API実装前に、2台のポート、書き込み、無線接続、双方のSerial、テストfixtureが動くことを確認する基盤テストです。
