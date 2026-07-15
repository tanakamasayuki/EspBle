# テスト計画

## 方針

BLEは接続、切断、Discovery、購読、Security、Bondingが複数の非同期イベントにまたがります。このためPeerテストを補助的なsmokeではなく、実装を進めるための主要な自動テストにします。

- unit: UUID、Advertising data、codec、状態遷移、error変換をhost上で検証する。
- examples_compile: 公開APIと対象SoCのbuild回帰を検出する。
- single: stack初期化などPeer不要のruntime behaviorをESP32-S3で検証する。
- peer: ESP32-S3 2台で実際のradio、controller、host stackを通した接続を検証する。
- manual: Android/iOS/Windows/Linux/macOSや市販機器との相互運用を検証する。

## Peerハードウェア

EspUsbHost/EspUsbDeviceなどで常時接続されているESP32-S3 2台を共用します。BLE通信のためのボード間配線は不要です。各ボードをPCへ接続するSerial/給電だけを使用します。

これに加えてmanual test用ESP32-S3が1台あります。BLEはボード間の有線接続を必要としないため、将来3台が必要なscenarioでは追加のPeerディレクトリとprofile/port設定を用意して、この1台を第3Peerとして利用できます。初期テストの必須環境は常設2台のままとし、3台構成は複数接続やBLE-to-BLE bridgeのE2E testを追加するときに使用します。

pytest-embedded-cliの既存規約に従います。

- 親側profile: `s3_peer_host`
- 2台目profile: `s3_peer_device`
- 2台目directory: `peer_device/`
- Python fixture: `peers["device"]`

これらの`host` / `device`はUSB roleでもBLE roleでもありません。pytest-embedded-cliは両方へsketchを転送して実行し、`dut`と`peers["device"]`の両Serialを観測・操作できます。

初期scenarioは親側sketchをCentral、2台目sketchをPeripheralに固定します。EspBle Centralを検証するときは親側の結果を主にassertし、EspBle Peripheralを検証するときはPeer側の結果を主にassertします。役割交換やコード配置の交換は前提にしません。

## Peerテスト原則

- テスト専用128-bit Service UUIDで周囲のBLE機器を除外する。
- device nameだけで接続相手を決めない。
- 可能な範囲で一方をArduino-ESP32同梱BLE APIの直接実装にする。
- 親CentralとPeer Peripheralの役割は固定したまま、EspBleを親側、Peer側、または両側へ組み込んで目的別に検証する。
- Serial logだけで合否をassertできるscenarioにする。
- 各テスト終了時にscan、advertising、subscription、connectionを停止する。
- Securityテストは開始時と終了時のBond/NVS状態を明示する。
- radio環境による一時的な遅延にtimeoutは許すが、無制限retryで不具合を隠さない。
- 接続・切断理由、MTU、Security状態を可能な限り両側で照合する。

## 3台Peerへの拡張

次のscenarioは3台構成の候補です。

- 1台のCentralから2台のPeripheralへの同時接続
- 2台のCentralから1台のPeripheralへの接続
- 接続ごとのsubscription、Security、Bond状態の分離
- BLE HID入力Peripheral → Bridge DUT（Central + Peripheral）→ 出力確認Central
- 一方の接続切断や再接続が他方へ影響しないこと

pytest-embedded-cliでは親側と既存`peer_device/`に加えてPeerディレクトリを追加し、それぞれのsketchとSerialを同じテストから操作・assertする構成にします。具体的なディレクトリ名と環境変数は最初の3台scenario追加時に、pytest-embedded-cliの命名規約に合わせて確定します。

## カバレッジ計画

| 領域 | unit | build | peer | manual |
|---|---|---|---|---|
| test fixture / bundled BLE stack | | ✅ | `stack_smoke` | |
| Advertising / Scan parser | 予定 | ✅ | ✅ `advertise_scan` | generic scanner |
| connect / disconnect / timeout | 状態遷移 | ✅ | ✅ `connect_disconnect`（timeoutは予定） | |
| GATT discovery / read / write | codec | ✅ | ✅ `gatt_read_write` | generic GATT app |
| notify / indicate / unsubscribe | queue | 予定 | 予定 | generic GATT app |
| MTU | validation | 予定 | 予定 | |
| Pairing / Bonding | error/state | 予定 | 予定 | Android/Linux |
| encrypted characteristic | permission | 予定 | 予定 | |
| reconnect / peer loss | state | 予定 | 予定 | |
| HID Keyboard | report codec | 予定 | 予定 | OS、市販keyboard |
| HID LED output | report codec | 予定 | 予定 | OS |
| Battery Service | codec | 予定 | 予定 | generic GATT app |
| Central+Peripheral同時動作 | state | 予定 | 予定 | |

## 最初の実装順

1. `stack_smoke`: 同梱NimBLE backendのBLE APIで2台接続、read/writeと双方のSerialを確認する。
2. ✅ `advertise_scan`: EspBle Advertising builderとScanner parser。
3. ✅ `connect_disconnect`: Connection identity、local role、接続と切断のloop context。
4. ✅ `gatt_read_write`: 汎用GATT Server/Client、既知UUID Discovery、Read、Write。
5. `notify_indicate`: subscriptionとevent queue。
6. `security_bond`: Pairing、Bond保存/削除/再接続。
7. `hid_keyboard`: HID Central/PeripheralとOutput Report。

## 合格条件

- test codeがすべての入力を生成し、Serial assertionで結果を判定する。
- timeoutやretryを含む合否条件が固定されている。
- EspBle同士だけでなく、同梱BLE API直接実装との組み合わせがある。
- 手動確認が必要な項目を自動テスト合格条件へ混ぜない。
