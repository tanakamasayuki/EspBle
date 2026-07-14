# Peer Tests

ESP32-S3 2台をBLEで接続する自動テストです。ボード間の信号配線は不要です。

```sh
uv run --env-file .env pytest peer/
```

profileと環境変数はEspUsbHost/EspUsbDeviceの既存環境を再利用します。

| pytest上の位置 | profile | 環境変数 |
|---|---|---|
| 親側 | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` |
| 2台目Peer | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` |

profile名はBLE roleを表しません。両側へsketchを転送・実行でき、両側のSerialをpytestから観測できます。初期テストでは親側sketchをCentral、`peer_device/`側sketchをPeripheralに固定し、役割交換は行いません。

## 追加済み

- `stack_smoke`: Arduino-ESP32同梱NimBLE backendのBLE APIだけを使い、Advertising/Scan、接続、GATT read/write、双方のSerialと2台fixtureを確認する。
