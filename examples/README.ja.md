# EspBle Examples

> English: [README.md](README.md)

各exampleには検証済みArduino-ESP32バージョンを固定した`sketch.yaml` profileが同梱されているため、IDEのボード設定なしでビルドできます:

```sh
arduino-cli compile --profile esp32s3 examples/<path>
```

相手側が必要なexampleは、それぞれのREADMEに組み合わせを記載しています。どのペアもnRF Connectなどのスマートフォンアプリを相手にして試すこともできます。

| Example | Role | 説明 |
|---|---|---|
| [CompileSmoke](CompileSmoke/) | — | 最小のビルド確認。ライブラリバージョンを表示 |
| [Gap/Advertise](Gap/Advertise/) | Peripheral | 名前+Service UUIDつきのconnectable Legacy Advertising |
| [Gap/Scan](Gap/Scan/) | Central | address / RSSI / nameを表示する継続active scan |
| [Gap/Connect](Gap/Connect/) | Central | Service UUIDをscanして接続。非同期の接続/切断/失敗イベント |
| [Gap/Mtu](Gap/Mtu/) | Central | 希望MTUの交換とNotification payload上限 |
| [Gatt/Server](Gatt/Server/) | Peripheral | Read/Write可能なCharacteristicを持つ独自Service |
| [Gatt/Client](Gatt/Client/) | Central | Gatt/Serverに対する既知UUID Discovery → Read → Writeの連鎖 |
| [Gatt/NotifyServer](Gatt/NotifyServer/) | Peripheral | 購読状態でgateした周期Notification |
| [Gatt/SubscribeClient](Gatt/SubscribeClient/) | Central | NotifyServerを購読してNotificationを表示 |
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Just Works Pairing + Bondingと暗号化Characteristic |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | 静的passkeyによるMITM認証Characteristic |
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | SerialコマンドでキーをタイプするBLE keyboard。LED report受信 |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | BLE keyboardへ接続してキー表示、LED書込み |

2台のボードでの推奨ペア:

- Gap/Advertise ↔ Gap/Scan
- Gatt/Server ↔ Gatt/Client
- Gatt/NotifyServer ↔ Gatt/SubscribeClient（およびGap/Mtu）
- Hid/KeyboardDevice ↔ Hid/KeyboardHost
