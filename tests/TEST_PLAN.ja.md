# テスト計画

> English: [TEST_PLAN.md](TEST_PLAN.md)

## 方針

BLEは接続、切断、Discovery、購読、Security、Bondingが複数の非同期イベントにまたがります。このためPeerテストを補助的なsmokeではなく、実装を進めるための主要な自動テストにします。

- unit: keymap変換、HID Report Map parserなどをhost上のg++で検証する（`tests/unit/`）。
- examples_compile: 公開APIと対象SoCのbuild回帰を検出する。`.github/workflows/compile-examples.yml`が全exampleをesp32s3 profileでコンパイルする（push/PRで自動実行。カバレッジ表のbuild列✅はこの検証を指す）。
- peer: ESP32-S3 2台を標準fixtureとし、実際のradio、controller、host stackを通した接続を検証する。ESP32-P4 + ESP32-C6はESP-Hosted固有経路の追加fixtureとして使用する。
- manual: Android/iOS/Windows/Linux/macOSや市販機器との相互運用を検証する。

Peer不要のruntime behaviorを1台で検証する「single」層は現在使用していません。必要になった時点で追加します。

## Peerハードウェア

Peerテストは次の2構成を使い分けます。

| fixture | 親側DUT | 2台目Peer | 目的 | 接続方針 |
|---|---|---|---|---|
| 標準回帰 | ESP32-S3 | ESP32-S3 | EspBleの全機能と通常のNimBLE経路 | 常時接続を推奨 |
| ESP-Hosted回帰 | ESP32-P4 + ESP32-C6 | ESP32-S3 | SDIO、ESP-Hosted、C6 controller、Wi-Fi/BLE共存 | 必要時の接続でよい |

標準回帰にはEspUsbHost/EspUsbDeviceなどで常時接続されているESP32-S3 2台を共用します。BLE通信のためのボード間配線は不要です。各ボードをPCへ接続するSerial/給電だけを使用します。

これに加えてmanual test用ESP32-S3が1台あります。BLEはボード間の有線接続を必要としないため、将来3台が必要なscenarioでは追加のPeerディレクトリとprofile/port設定を用意して、この1台を第3Peerとして利用できます。初期テストの必須環境は常設2台のままとし、3台構成は複数接続やBLE-to-BLE bridgeのE2E testを追加するときに使用します。

pytest-embedded-cliの既存規約に従います。

- 通常の親側profile: `s3_peer_host`
- P4親側profile: `p4_peer_host`
- 2台目profile: `s3_peer_device`
- 2台目directory: `peer_device/`
- Python fixture: `peers["device"]`

これらの`host` / `device`はUSB roleでもBLE roleでもありません。pytest-embedded-cliは両方へsketchを転送して実行し、`dut`と`peers["device"]`の両Serialを観測・操作できます。

初期scenarioは親側sketchをCentral、2台目sketchをPeripheralに固定します。EspBle Centralを検証するときは親側の結果を主にassertし、EspBle Peripheralを検証するときはPeer側の結果を主にassertします。役割交換やコード配置の交換は前提にしません。

## P4/C6 ESP-Hosted回帰

### P4を実機テストする理由

P4向けのコンパイルだけでは、P4とC6の間にあるSDIO transport、ESP-Hostedの初期化・終了、C6側controller、Wi-Fi/BLEの共有を通りません。S3だけのPeerテストでもこの経路は再現できないため、P4対応を維持するにはP4+C6実機による追加回帰が必要です。P4+C6 fixtureは1組で十分で、常時接続する必要はありません。

### 基準fixtureの条件

- ESP32-P4をhost、ESP32-C6をESP-Hosted slave/controllerとして使用する。2台目のESP32-S3は無線Peerであり、P4との信号配線は不要。
- P4-C6間は4-bit SDIOの`CLK`、`CMD`、`D0`〜`D3`、`RESET`と安定した電源/GNDを接続する。
- C6にはArduino-ESP32 Core同梱hostと互換性のあるESP-Hosted Slave firmwareを書き込む。準備とversion条件は[ESP-Hostedセットアップ](../docs/ESP_HOSTED_SETUP.ja.md)を参照する。
- 基準fixtureには、C6を搭載済みのEspressif ESP32-P4-Function-EV-Board、またはそれと同じ標準SDIO配線（P4側`CLK=18`、`CMD=19`、`D0=14`、`D1=15`、`D2=16`、`D3=17`、`RESET=54`）のP4+C6構成を推奨する。この配線はArduino-ESP32の汎用`esp32p4` variantと一致し、board固有設定なしで共通の回帰条件を再現できる。
- M5Stack Tab5など標準配線と異なるboardも使用できる。その場合は正しいboard variantを選ぶか、`ble.begin()`より前に`hostedSetPins()`で上書きする。方法は[SDIO pinの選択と上書き](../docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)を参照し、結果には使用したboard/profileとpin構成を記録する。

独自配線のfixtureは追加検証には使えますが、それだけを唯一の基準機にするとCoreの標準設定に対する回帰を判定しにくくなります。このため、今後C6付きP4を追加するならFunction-EV-Boardまたは上記の標準配線を使う構成を優先します。board出荷時のC6 firmware versionは固定条件にせず、テスト前にCoreとの互換性を確認して必要なら公式Updaterで更新します。

### 実行頻度

| タイミング | P4実機テスト |
|---|---|
| 通常の変更 | S3の全回帰を基本とし、P4の常時実行は不要 |
| `begin()`/`end()`、NimBLE lifecycle、ESP-Hosted分岐、P4 profile、Wi-Fi共存の変更 | 変更ごと、またはmerge前に代表suiteを実行 |
| Arduino-ESP32 CoreまたはC6 firmwareの更新 | 更新直後に代表suiteを実行し、既知制限も再確認 |
| リリース候補 | 最終候補に対して代表suiteとWi-Fi/BLE共存testを必ず実行 |

Hosted関連を連続開発している期間は、まとまった変更ごとに接続して回すのが有効です。Hostedに関係する変更がなければ週次などの暦ベース実行は必須ではなく、リリース前に接続すれば十分です。

### 通常実行とprofileの扱い

無指定の`pytest`および`pytest peer/`は、常設可能なS3 2台だけで完走できることを原則とします。P4/C6のように常時接続しない追加fixtureを`default_profile`にしてはならず、P4実機を検証するときだけ`--profile p4_peer_host`を明示します。

`wifi_ble_coexistence`はESP-Hosted固有scenarioですが、sketch自体の既定profileは`s3_peer_host`です。S3 buildではHosted専用header/APIをcompile対象から外し、Serialで`ESP_HOSTED_CAPABLE 0`を返します。pytestはこのcapabilityを確認して正常終了するため、通常suiteはP4未接続でも失敗しません。P4 buildは`ESP_HOSTED_CAPABLE 1`を返し、その場合だけWi-Fi接続、BLE通信、共有transportのlifecycleを最後まで検証します。

S3での正常終了は「Wi-Fi/BLE共存を確認済み」という意味ではなく、「このfixtureでは対象外であることを明示的に確認した」という意味です。P4の共存回帰を実施したことにするには、必ずP4 profileを指定した実行結果を使用します。

`.env`にP4とPeer S3のportを設定した代表suiteは`tests/`から次のように実行します。

```sh
uv run --env-file .env pytest \
  peer/stack_smoke/ \
  peer/connect_disconnect/ \
  peer/gatt_read_write/ \
  peer/notify_indicate/ \
  peer/mtu/ \
  peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

短時間の疎通確認には`peer/connect_disconnect/`だけを同じprofile指定で実行します。現行Core/ESP-Hostedで既知制限の影響を受けるSecurityおよび完全な初期化・終了反復は、代表suiteの必須合格項目に含めません。CoreまたはC6 firmware更新時には別途再実行し、[ESP-Hostedの既知制限](../docs/ESP_HOSTED_LIMITATIONS.ja.md)が解消したか確認します。

## 無印ESP32回帰

無印ESP32はArduino-ESP32のプリビルドがBluedroidであるため、EspBleが同梱するNimBLE host
（`src/nimble_esp32/`）で動きます。方針と検証記録は[無印ESP32対応計画](../docs/PLAN_ESP32.ja.md)が正本です。

| 役割 | profile | 対象suite |
|---|---|---|
| 親側(Central) | `esp32_peer_host` | EspBleを使う62 suite |
| Peer(Peripheral) | `esp32_peer_device` | EspBleを使う64 suite |

```sh
uv run --env-file .env pytest peer/<suite>/ --profile esp32_peer_host --peer-profile device:s3_peer_device
uv run --env-file .env pytest peer/<suite>/ --profile s3_peer_host --peer-profile device:esp32_peer_device
```

**esp32 profileを持たない側は、profile指定時に自動でskipされます**（除外指定は不要）。
profileを置いていないのは次の2種類だけです。

1. **core同梱`BLE`ラッパで書かれた側** — 各suiteは片側をEspBle、もう片側を独立実装の
   基準側として同梱ラッパで書いています。無印ESP32ではそのラッパがBluedroidになり、
   自前のNimBLE hostと同一controllerを共有できないため実行できません（`#error`で拒否）。
   該当は親側の`stack_smoke`・`advertise_payload`・`hid_keyboard_device`・`midi_device`、
   Peer側の`stack_smoke`・`midi_host`です。**反対側（EspBleで書かれた側）にはprofileがあり、
   無印ESP32で実行・pass済み**なので、MIDIもHIDも無印ESP32のカバレッジがあります。
   両側がラッパの`stack_smoke`だけが無印ESP32では対象外です（同梱ラッパのsmokeであり
   EspBleのテストではないため）。
2. **`phy_update`** — 無印ESP32はBLE 4.2 controllerでLE 2M PHYを持たず、構造上通りません。

役割を入れ替えて同じsuiteを続けて実行するときは、**片方のボードを別suiteのfirmwareで上書きしてから**
実行してください。`local_identity`のようにService UUIDで対象を選ぶsuiteは、前の実行のPeer firmwareが
載ったままのボードが広告していると意図しない側を観測します。

無印ESP32の2台は`/dev/ttyUSB0` / `/dev/ttyUSB1`で常設です。EspBleBluedroidと機材を共用しますが、
両repositoryのpytestを同時に走らせても構いません（ポートの調停はpytestが行います）。

| タイミング | 無印ESP32の実行 |
|---|---|
| 文書だけの変更 | 不要 |
| 通常の`src/`変更 | 代表smoke（`gatt_read_write` / `security_bond` / `hid_keyboard_host` / `mtu` / `connection_parameters`、親側のみで可。約15分） |
| `src/nimble_esp32/`、`EspBleNimbleHost.h`、lifecycle・controller・vendorツールの変更 | 両役割の全掃引（各約1時間） |
| Arduino-ESP32 Coreの更新 | 両役割の全掃引。`tools/vendor_nimble_esp32.py`のpinがそのcoreのesp-idfと一致しているかも確認する |
| リリース候補 | 両役割の全掃引（[リリースチェックリスト](../docs/RELEASE_CHECKLIST.ja.md)参照） |

無印ESP32はcore同梱ではなく**EspBleが持ち込んだhost**で動くため、`src/`の変更はS3では
再現しない形で影響し得ます。一方でhostは他ターゲットと同一スナップショットなので、
毎回の全掃引までは求めず、上表の粒度とします。

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

## 他スタックとの相互接続テスト（予定）

同梱NimBLEどうしの通信だけでは、EspBleが「NimBLEの癖に依存した実装」になっていても気づけません。兄弟ライブラリの**EspBleBluedroid**（ESP32のBluedroidスタック版）を相手にした相互接続テストを追加します。

- 対象: GATTのServer/Client両方向、Notify/Indicate、MTU交換、Pairing/Bonding
- 位置づけ: NimBLE ↔ Bluedroid の組み合わせで、wire形式と手続きが仕様どおりかを確認する
- 実施時期: EspBleBluedroid側のGATTが動作するようになってから

既存の「可能な範囲で一方をArduino-ESP32同梱BLE APIの直接実装にする」という原則の延長ですが、**スタックそのものが異なる**点でより強い検証になります。同梱wrapper由来の制約（同一UUIDの扱いなど）が、相手スタックでどう見えるかの確認にも使えます。

## 3台Peer（manual test）

3台目board前提のscenarioは`tests/manual/`配下に置きます（既定`pytest peer/`には含めず、3台目のport未設定時は自動skip）。実行は`tests/`から`uv run --env-file .env pytest manual/`。boardとportは[tests/manual/README.md](manual/README.md)参照。

- ✅ `manual/multi_connection`: 1台のCentralから2台のPeripheral（A=`peer_device`、B=`peer_device2`）へ同時接続し、各peerのnotifyが正しいconnectionへroutingされることを確認。続いてPeripheral Aを（peripheral側から）想定外に切断し、`setAutoReconnect(true)`によりCentralが同一addressへ自動再接続、persistent subscriptionが無指示で復元してnotifyが再開する一方、Peripheral Bは接続を維持することを確認。接続ごとのdiscovery cache・subscriptionの分離と、一方の切断・再接続が他方へ影響しないことを検証する。

今後の3台候補: 2台のCentralから1台のPeripheralへの接続、BLE HID入力Peripheral → Bridge DUT（Central + Peripheral）→ 出力確認Central。追加時は同じ`tests/manual/`配下にPeerディレクトリとprofile/port設定を用意する。

## カバレッジ計画

| 領域 | unit | build | peer | manual |
|---|---|---|---|---|
| test fixture / bundled BLE stack | | ✅ | `stack_smoke` | |
| Advertising / Scan parser | 予定 | ✅ | ✅ `advertise_scan` / `advertise_payload`（raw AD構造） | generic scanner |
| connect / disconnect / timeout | 状態遷移 | ✅ | ✅ `connect_disconnect`（address直接再接続） / `lifecycle_stress`（接続timeoutの非同期失敗） | |
| 接続lifecycle / event queue / leak | | ✅ | ✅ `lifecycle_stress`（flood中切断、再接続heap、GATT中`end()`、connect中`end()`、scanner flush、GATT操作の自動キュー） | |
| GATT一覧/既知UUID discovery、Characteristic/Descriptor read/write | codec | ✅ | ✅ `gatt_read_write`（Server Descriptor event、操作timeout・遅延完了抑止を含む） | generic GATT app |
| notify / indicate / unsubscribe | queue | ✅ | ✅ `notify_indicate` / `persistent_subscribe`（再接続時の自動再購読） | generic GATT app |
| MTU | validation | ✅ | ✅ `mtu` | |
| Pairing / Bonding | error/state | ✅ | ✅ `security_bond` | Android/Linux |
| static passkey / MITM | validation | ✅ | ✅ `security_passkey` | Android/Linux |
| encrypted characteristic | permission | ✅ | ✅ `security_bond` | |
| authenticated characteristic | permission | ✅ | ✅ `security_passkey` | |
| HID over GATT security | | ✅ | ✅ `hid_security`（未暗号化linkの拒否） | OS |
| reconnect / peer loss | state | ✅ | ✅ Bond再接続 / `lifecycle_stress`（radio消失をsupervision timeoutで検出） / `persistent_subscribe`（再購読） | `manual/multi_connection`（auto-reconnect） |
| 複数同時接続 / 接続分離 | | | | `manual/multi_connection`（2 Peripheral同時、notify routing、一方切断が他方に非影響） |
| Address privacy（own address type） | | ✅ | ✅ `address_privacy`（random static advertising） | RPA回転（900秒周期のため自動試験対象外）はbonded peer解決を手動確認 |
| iBeacon（broadcast / decode） | ✅ `unit/ibeacon` | ✅ | ✅ `ibeacon`（broadcast→decode、全フィールド） | iBeaconアプリ |
| Advertising Service Data（AD 0x16） | | ✅ | ✅ `service_data`（`addServiceData`複数ブロック / `serviceDataFor()`によるUUID検索） | generic scanner |
| Scan Response payload分割 / Appearance / Tx Power受信 | | ✅ | ✅ `scan_response`（passiveで届かない項目とactiveで届く項目の切り分け） | generic scanner |
| Filter Accept List / connect timeout | | ✅ | ✅ `accept_list`（制限policyで接続不成立→開放で成立、要求timeoutでの失敗通知） | |
| 自分のアドレス / 送信電力 / 切断理由指定 | | ✅ | ✅ `local_identity`（`localAddress()`と観測値の一致、`setTxPower()`の電波への反映、理由コードの往復） | |
| HID Keyboard Device | report codec（予定） | ✅ | ✅ `hid_keyboard_device` / `hid_robustness`（購読gate、queue満杯） | OS、市販HID Host |
| HID NKRO Device / Host | ✅ `unit/report_map` | ✅ | ✅ `hid_keyboard_nkro`（8キー、高usage、個別release、LED） | OS、市販HID Host |
| HID LED output | report codec（予定） | ✅ | ✅ `hid_keyboard_device` / `hid_keyboard_host`（WWR非block） | OS |
| Battery Service Server / Client | 1-byte codec | ✅ | ✅ HIDへの組込み / `battery_service` standalone | generic GATT app |
| Device Information Service | PnP ID 7-byte codec | ✅ | ✅ HIDへの組込み / `device_information` standalone | generic GATT app |
| Current Time Service | Current Time 10-byte codec | ✅ | ✅ `current_time` Read / Notify | generic GATT app |
| Heart Rate Service | flags / variable-length codec | ✅ | ✅ `heart_rate` Location Read / Measurement Notify | generic GATT app |
| Environmental Sensing Service | signed / scaled integer codec | ✅ | ✅ `environmental_sensing` Read / Temperature Notify | generic GATT app |
| BLE MIDI Device / Host | ✅ `unit/midi`（timestamp、running status、SysEx、builder、複数パケットencoder） | ✅ | ✅ `midi_device`（wire形式＋複数パケットSysExを同梱API Centralで検証） / `midi_host`（running status decode＋複数パケットSysEx送信を同梱API Peripheralで検証） | 市販BLE MIDI楽器 / DAW |
| Health Thermometer Service | ✅ `unit/medical_float`（IEEE-11073 FLOAT/SFLOAT） | ✅ | ✅ `health_thermometer`（Type Read＋FLOAT Measurement Indicate/decode） | generic GATT app / 市販体温計 |
| Blood Pressure Service | ✅ `unit/medical_float`（SFLOAT） | ✅ | ✅ `blood_pressure`（Feature Read＋SFLOAT Measurement Indicate/decode） | generic GATT app / 市販血圧計 |
| Weight Scale Service | 固定分解能uint16 | ✅ | ✅ `weight_scale`（Feature Read＋uint16 Measurement Indicate/decode） | generic GATT app / 市販体重計 |
| Body Composition Service | uint16 flags＋任意フィールド | ✅ | ✅ `body_composition`（Feature Read＋Body Fat Percentage/Weight Measurement Indicate/decode） | generic GATT app / 市販体組成計 |
| Cycling Speed and Cadence Service | 多フィールド整数レイアウト | ✅ | ✅ `cycling_speed_cadence`（Location Read＋多フィールドMeasurement Notify/decode） | generic GATT app / 市販CSCセンサー |
| Running Speed and Cadence Service | 混在幅整数レイアウト | ✅ | ✅ `running_speed_cadence`（Location Read＋混在幅Measurement Notify/decode） | generic GATT app / 市販RSCセンサー |
| Cycling Power Service | 符号付き16bit + 16bit flags | ✅ | ✅ `cycling_power`（Location Read＋符号付きpower Measurement Notify/decode） | generic GATT app / 市販パワーメーター |
| Fitness Machine Service（FTMS） | flags駆動offset walk | ✅ | ✅ `fitness_machine`（Feature/Indoor Bike Data＋Control Point/Status） | Zwift等 / 市販スマートトレーナー |
| Pulse Oximeter Service（PLX） | SFLOAT | ✅ | ✅ `pulse_oximeter`（Features Read＋SFLOAT Spot-Check Indicate/decode） | generic GATT app / 市販パルスオキシメーター |
| Glucose Service（RACP手続き） | SFLOAT / date_time | ✅ | ✅ `glucose`（RACP write→Measurement notify→RACP応答indicate） | generic GATT app / 市販血糖値計 |
| Location and Navigation Service | flags駆動可変長 + sint32 | ✅ | ✅ `location_navigation`（LN Feature Read＋Location and Speed Notify/decode） | generic GATT app / 市販GPSセンサー |
| User Data Service | 書き込み可能char + notify | ✅ | ✅ `user_data`（Age/First Name write→onWritten→Database Change Increment notify、再read） | generic GATT app |
| Alert Notification Service | bitmask read + Control Point write + notify | ✅ | ✅ `alert_notification`（category bitmask Read＋Control Point write→New Alert notify/decode） | generic GATT app / 市販通知機器 |
| Immediate Alert Service（Find Me） | Write Without Responseのみ | ✅ | ✅ `immediate_alert`（Alert Level Write Without Response→onWritten） | generic GATT app / 市販Find Meタグ |
| Phone Alert Status Service | read/notify + Write Without Response Control Point | ✅ | ✅ `phone_alert_status`（Alert Status Read＋Ringer Control Point write→Ringer Setting notify/decode） | generic GATT app / 市販電話proxy |
| Proximity（Link Loss + Tx Power） | 2 Service同居 + signed int8 | ✅ | ✅ `proximity`（Tx Power Level signed read＋Alert Level read/write/再read） | generic GATT app / 市販Proximityタグ |
| Reference Time Update Service | Write Without Response Control Point + read state | ✅ | ✅ `reference_time_update`（Control Point write→Time Update State遷移/再read） | generic GATT app |
| Bond Management Service | Feature bitmask read + Control Point write | ✅ | ✅ `bond_management`（Feature uint24 Read＋Control Point op code write/onWritten） | generic GATT app |
| Continuous Glucose Monitoring Service | ✅ `unit/cgm_crc`（CRC-16/MCRF4XX） | ✅ | ✅ `continuous_glucose_monitoring`（E2E-CRC付きFeature Read＋Measurement Notify/verify/decode） | generic GATT app / 市販CGM |
| HID Keyboard Host | ✅ `unit/report_map`（Vendor含む） | ✅ | ✅ `hid_keyboard_host`（全6 profile、Vendor双方向） / `hid_boot_keyboard`（Report IDなし、長さ異常） / `hid_robustness`（rollover、Discovery中disconnect拒否） | 市販keyboard |
| HID keyboard event / layout | ✅ `unit/keymap`（Unicode 4-plane、AltGr、CapsLock） | ✅ | ✅ EN-US / JA-JP / en-GB / de-DE / fr-FR、modifier | 残りの各言語実機 |
| ESP32KeyBridge input adapter | bridge core | ✅ | ·（ESP32KeyBridge側で検証する） | BLE-to-USB E2E |
| Central+Peripheral同時動作 | state | 予定 | 予定 | |

## 実装済みscenario

1. ✅ `stack_smoke`: 同梱NimBLE backendのBLE APIで2台接続、read/writeと双方のSerialを確認する。
2. ✅ `advertise_scan`: EspBle Advertising builderとScanner parser。
3. ✅ `connect_disconnect`: Connection identity、local role、接続と切断のloop context。
4. ✅ `gatt_read_write`: 汎用GATT Server/Client、一覧/既知UUID Discovery、応答あり/なしWrite、Descriptor Read/Write、操作timeout。
5. ✅ `notify_indicate`: subscription、unsubscribe、CCCD、送信結果、受信event queue。
6. ✅ `mtu`: 接続時MTU交換、両側snapshot/callback、最大payloadと超過拒否。
7. ✅ `security_bond`: Pairing、Bond保存/削除/再接続、暗号化Characteristic。
8. ✅ `security_passkey`: 静的passkey、MITM、認証必須Characteristic。
9. ✅ `hid_keyboard_device`: HID Keyboard Device、Battery Read、Input Notification、Output Report。
10. ✅ `hid_keyboard_host`: HID Host、全6 profileのReport Map解析、Vendor Input / Output / Feature、state、LED返送。
11. ✅ `lifecycle_stress`: event flood中の切断保持、再接続heap、GATT/connect実行中の`end()`、scanner flush、接続timeoutの非同期失敗、GATT操作の自動キュー、peer loss（supervision timeout）。
12. ✅ `hid_robustness`: CCCD購読gate、rollover無視、queue満杯時の全release保持、Discovery中disconnect拒否、config違いの再`begin()`拒否。
13. ✅ `hid_security`: security有効HID Deviceが未暗号化linkのRead/Discovery/Inputを拒否。
14. ✅ `hid_boot_keyboard`: Report IDなしboot keyboardのDiscoveryと入力、長さ異常reportのカウント。
15. ✅ `advertise_payload`: raw advertisementのAD構造検証（単一Complete List、type重複なし）。
16. ✅ host unit test（`tests/unit/`）: keymap変換（Unicode 4-plane、AltGr、文字ペアCapsLock）、HID Report Map parser、各codec、HCI router、HCI command scheduler。
17. ✅ `battery_service`: standalone Battery LevelのRead、CCCD購読、Notification、解除。
18. ✅ `device_information`: standalone DISの文字列Readと7-byte little-endian PnP ID decode。
19. ✅ `current_time`: standalone Current Timeの10-byte decode、CCCD購読、Notification、解除。
20. ✅ `heart_rate`: Body Sensor Location Readとflags付き可変長Measurementの購読、decode、解除。
21. ✅ `environmental_sensing`: Temperature / Humidity / Pressureのscale値Read、温度Notification、解除。
22. ✅ `hid_keyboard_nkro`: 29-byte bitmap Report、8キー同時押し、高usage、個別release、LED Output。あわせて**NKROのMTU下限拒否**を確認する（`test_nkro_requires_mtu_32`）: 29byteのreportは`preferredMtu >= 32`（29＋ATTヘッダ3）を要するため、`begin()`が23と31を`InvalidArgument`で拒否し32を受理する。無言失敗にすると「キーボードが何も送らない」ようにしか見えず原因を指せないため明示エラーにしており、**MTUを黙って引き上げることもしない**（アプリが選んだ設定を隠すことになる）。拒否後もkeyboard構成が残っていることも確認する。
23. ✅ `midi_device`: 同梱API CentralによるEspBleMidiDeviceのwire形式（header＋timestamp＋Note On/Off）、空Read、複数パケットSysExの独立reassemble、Centralからの書込みdecode検証。
24. ✅ `midi_host`: 同梱API Peripheralからのrunning statusパケットをEspBleMidiHostが2メッセージへdecode、Host送信Note Onと複数パケットSysExのPeripheral到達検証。
25. ✅ `health_thermometer`: Health Thermometer ServerのIEEE-11073 FLOAT Temperature Measurement Indicate、ClientのType Read・Indication購読・FLOAT decode検証。
26. ✅ `blood_pressure`: Blood Pressure ServerのIEEE-11073 SFLOAT systolic/diastolic/mean Measurement Indicate、ClientのFeature Read・Indication購読・SFLOAT decode検証。
27. ✅ `weight_scale`: Weight Scale Serverの0.005 kg分解能uint16 Weight Measurement Indicate、ClientのFeature Read・Indication購読・decode検証。
28. ✅ `cycling_speed_cadence`: CSC Serverの多フィールドMeasurement Notify、ClientのSensor Location Read・Notification購読・wheel/crank全フィールドdecode検証。
29. ✅ `running_speed_cadence`: RSC Serverの混在幅Measurement Notify、ClientのSensor Location Read・Notification購読・speed/cadence/stride/distance decode検証。
30. ✅ `cycling_power`: Cycling Power Serverの16bit flags＋符号付き16bit power Measurement Notify、ClientのSensor Location Read・Notification購読・負のpower decode検証。
31. ✅ `pulse_oximeter`: Pulse Oximeter ServerのSFLOAT SpO2/pulse rate Spot-Check Indicate、ClientのPLX Features Read・Indication購読・SFLOAT decode検証。
32. ✅ `glucose`: RACP手続き。ClientのRACP write→ServerのMeasurement notify→RACP応答indicateの一連の振る舞いと、sequence/base time/SFLOAT濃度のdecode検証。
33. ✅ `body_composition`: Body Composition Serverのuint16 flags＋必須Body Fat Percentage（0.1 %/LSB）＋任意Weight（flag bit 10）Measurement Indicate、ClientのFeature Read・Indication購読・全フィールドdecode検証。
34. ✅ `location_navigation`: Location and Navigation Serverのflags駆動Location and Speed Notify（Instantaneous Speed＋sint32緯度経度）、ClientのLN Feature Read・Notification購読・speed/lat/lon decode検証。
35. ✅ `user_data`: User Data ServiceでClientがFirst NameとAgeをWrite→Serverの`onWritten`受信→Database Change Increment（uint32）をNotify→ClientがincrementをdecodeしAgeを再readして書き込み保存を確認。書き込み→onWritten→notifyパス検証。
36. ✅ `alert_notification`: Alert Notification ServiceでClientがSupported New Alert Category bitmask（0x0022）をRead・New Alert購読・Control Pointへ「Notify New Alert Immediately」をWrite→Serverがcategory/count/text（"Bob"）付きNew AlertをNotify→Clientがdecode。Control Point→notifyパス検証。
37. ✅ `immediate_alert`: Immediate Alert Service（Find Me）でClientがAlert Level（0x2A06）へHigh Alert（2）→No Alert（0）をWrite Without Responseで書き込み→Serverが`onWritten`で各levelをloop contextで受信。Write Without Responseのみの標準Service検証。
38. ✅ `phone_alert_status`: Phone Alert Status ServiceでClientがAlert Status（0x2A3F）をRead・Ringer Setting（0x2A41）購読・初期値read・Ringer Control Point（0x2A40）へSet Silent（1）/Cancel Silent（3）をWrite Without Response→Serverが状態変更しRinger SettingをNotify（0/1）→Clientがdecode。Control Point→状態変更notifyパス検証。
39. ✅ `proximity`: Proximityプロファイル。1 serverにLink Loss Service（0x1803）とTx Power Service（0x1804）を同居させ、ClientがTx Power Level（0x2A07、signed int8 = -8 dBm）をRead・Link Loss Alert Level（0x2A06）を初期read・High Alert（2）を応答ありWrite・再readして保存を確認。2 Service同居とsigned int8 read検証。
40. ✅ `reference_time_update`: Reference Time Update ServiceでClientがTime Update State（0x2A17、2バイト）を初期read（0,0）・Time Update Control Point（0x2A16）へGet（1）/Cancel（2）をWrite Without Response→Serverがread専用stateをUpdate Pending（1,0）/Idle・Canceled（0,1）へ遷移→Clientが再readで確認。Control Point→state遷移パス検証。
41. ✅ `bond_management`: Bond Management ServiceでClientがBond Management Feature（0x2AA5、uint24 = 0x000011）をRead・Bond Management Control Point（0x2AA4）へop code 0x03（Delete bond of requesting device, LE）を応答ありWrite→Serverが`onWritten`でop codeを受信。Feature read＋Control Point op codeのGATT choreography検証（実bond削除ではない）。
42. ✅ `continuous_glucose_monitoring`: CGM ServiceでClientがCGM Feature（0x2AA8）をReadしてE2E-CRC検証・feature（0x001000）とtype/location（0x11）をdecode・CGM Measurement（0x2AA7）購読→ServerがSFLOAT血糖値（100）・time offset（5）・末尾E2E-CRC付きMeasurementをNotify→ClientがE2E-CRC検証しdecode。両基板が共有`EspBleCgmCrc.h`でCRC生成・検証。
43. ✅ `disconnect_reason`: `EspBleConnection::disconnectReason`検証。Peripheralが切断を開始し、開始側（Peripheral）はlocal終了、remote側（Central DUT）はremote終了の理由コードをonDisconnectedで受け取る。両者が非0かつ相異なることを確認し、切断理由がServer/Client両パスで捕捉されることを検証。
44. ✅ `connection_parameters`: 接続パラメータ検証。`EspBleConnection`のinterval/latency/timeout公開と、Central（DUT）の`updateConnectionParameters()`要求→両peerの`onConnectionParametersUpdated()`が交渉後interval（80）を報告することを確認。
45. ✅ `phy_update`: LE PHY検証。`EspBleConnection`のtx/rxPhy公開と、Central（DUT）の`updatePhy()`で2M PHY要求→両peerの`onPhyUpdated()`が交渉後PHY（tx/rx=2）を報告することを確認。
46. ✅ `service_changed`: GATT Service Changed検証。ClientがGeneric Attribute Service（0x1801）のService Changed（0x2A05）を購読→Serverの`notifyServicesChanged(0x0001,0xFFFF)`→Clientがindicationで変更range（start=1/end=65535）をdecodeすることを確認。
47. ✅ `runtime_passkey`: 対話型の実行時Passkey Entry検証。Peripheral（DisplayOnly・MITM・静的passkeyなし）が動的passkeyを生成し`onPasskeyDisplayed`で提示、Central（KeyboardOnly・MITM・静的passkeyなし）は接続後にbackendのpasskey要求がブロックし、テストが表示passkeyを`providePasskey()`で中継→両側authenticated+bondedでpairing完了することを確認。
48. ✅ `numeric_comparison`: LE Secure Connections Numeric Comparison検証。両側（DisplayYesNo・MITM）が同一の6桁比較値を`onNumericComparison`で提示、テストが両値の一致を確認して両側`confirmNumericComparison(true)`→両側authenticated+bondedでpairing完了することを確認。
49. ✅ `hid_boot_protocol`: HID over GATTのBoot Protocol検証。generic GATT clientがkeyboard peripheralのProtocol Mode（0x2A4E、初期値Report=1）をRead、Boot Keyboard Input Report（0x2A22）を購読、Boot Protocol Modeへ切替（deviceは`onProtocolMode()`でmode=0を観測）、Shift+'a'の8-byte Boot ReportをNotify受信、Boot Keyboard Output Report（0x2A32）のCaps Lock LED書き込みをdeviceが`onOutputReport()`で受信することを確認。
50. ✅ `hid_custom`: 任意Report DescriptorのCustom HID＋handle指定GATT操作の検証。`ble.hidCustom()`でvendor定義descriptor（Report ID 1に2byte入力＋1byte出力＋2byte feature）をHID Serviceへ合成（3 Reportが同一UUID 0x2A4D）。generic GATT clientがdiscover後に各Reportを個別handleへ解決し、Report Map（0x2A4B）長を確認、入力Reportをhandleで購読して2byte（差分+5・ボタン0x01）をデコード、出力Reportをhandleで書き込みdeviceが`onOutputReport()`で受信することを確認。**各Reportの役割は、HIDが本来宣言している方法——Report Reference（0x2908）のtype byte（1=Input / 2=Output / 3=Feature）——で決める。** Report Referenceは「0x2A4Dのcharacteristicの下にある0x2908」なので`readDescriptor()`の**handle指定でしか名指しできない**（UUIDの組では3つのcharacteristicを選び分けられない）。結果の`descriptorHandle`が対象descriptor、`handle`がそれを持つcharacteristicであることを、discovery時の対応付けと照合する。あわせて**宣言されたtypeとcharacteristicのflagsが一致すること**（Inputはnotifiable、OutputだけがWrite Without Responseを持つ、Featureは応答付き書き込みのみ）を確認する——featureは設定なのでfire-and-forgetになってはいけない。`addFeatureReport()`が独立したhandleを得て、そこへの書き込みがdeviceの`onFeatureReport()`へ2byteそのまま届くことも確認。handle指定の異常系（ゼロハンドルは`INVALID_ARGUMENT`で受理前に拒否、存在しないハンドルは受理後に`NotFound`）も含む。
51. ✅ `beacon`: non-connectable Beaconの検証。Peer側が`setConnectable(false)`＋`setScanResponseEnabled(false)`＋`setInterval(100, 150)`でmarker Service UUIDとmanufacturer dataをbroadcastし、親側Scannerがconnectable=0・scannable=0・manufacturer payload（`ffff01020304`）を捕捉することを確認。
52. ✅ `persistent_subscribe`: persistent subscription（再接続時の自動再購読）の検証。Centralが初回接続でnotify characteristicを購読し1件受信、切断（非bondなのでPeripheral側は購読を忘れ再advertise）、Centralが再接続する。再接続では`subscribe()`を呼ばないが`EspBleConfig::persistentSubscriptions`（既定on）が購読を自動復元するため`onSubscribed`がconnect=2で自発的に発火し、Peripheralからのnotifyをcount=2で受信することを確認。
53. ✅ `address_privacy`: address privacyの検証。Peripheralを`EspBleConfig::ownAddressType = RandomStatic`で構成しmarker Serviceをadvertiseし、Central ScannerがそのpeerをaddressType=Random（=1）かつ先頭octetの上位2bit=0b11（static random）で観測することを確認（public addressではないこと）。
54. ✅ `ibeacon`: iBeaconのbroadcast/decodeの検証。Peripheralが`EspBleIBeacon.h` codecで組んだiBeacon（UUID 0102..10、major 0x1234、minor 0xABCD、measured power -59）をnon-connectable・non-scannable manufacturer dataとしてbroadcastし、Central Scannerが`espBleDecodeIBeacon`で全フィールドをdecode（connectable=0・scannable=0）することを確認。
55. ✅ `service_data`: Advertising Service Data（AD 0x16）の送受信と複数ブロックの検証。Peripheralが`addServiceData("FEAB", {AB CD EF 12})`と`addServiceData("181A", {2E 09})`の2ブロックをnon-connectable・non-scannable broadcastし、Central Scannerが`serviceDataCount == 2`と各ブロックのUUID・payloadを読めること、さらに`serviceDataFor("181A", data)`が16-bit表記と128-bitフル形を値比較して一致させることを確認。
56. ✅ `fitness_machine`: 標準Fitness Machine Service（0x1826）のdata＋control検証。ServerがFitness Machine Feature（0x2ACC）をRead提供し、Indoor Bike Data（0x2AD2）をflags 0x0044でNotify。ClientがFeature Read（features=6）、購読、Indoor Bike Dataをflag順にdecodeしてspeed=3000・cadence=90 rpm・power=250 Wを復元。続いてControl Point（0x2AD9）とStatus（0x2ADA）を購読し、Set Target Power（0x05,250）→応答indication [0x80,05,01]を確認、Serverの'g'で"Target Power Changed"（0x08,250）status notifyを確認（indicationは1接続同時1件のみのため単一indicationを検証）。最後にIndoor Bike Data購読解除と切断まで確認。
57. ✅ `scan_response`: advertising payloadとscan responseの分割、およびAppearance / Tx Power Levelの受信の検証。PeripheralがService UUID・Appearance（0x0341）・Tx Power Levelをadvertising payloadへ、name・Manufacturer Dataをscan responseへ載せてbroadcast。CentralがPassive Scanでname・Manufacturer Dataが**届かない**ことと、Appearance・Tx Powerが届くことを確認し、続くActive Scanで4項目すべてが1件の結果へマージされることを確認。Tx Powerの値はcontrollerが埋めるため範囲と、両scan modeで同値であることを判定。
58. ✅ `accept_list`: Filter Accept ListによるPeripheral側の接続制限の検証。Peripheralが到達不能アドレスのみをaccept listへ登録し`ConnectionFromAcceptList` policyでadvertise。Centralの接続が成立せず、要求timeout（4秒）で`onConnectionFailed`が返ること（backendは約30秒戻らないため、EspBleが試行を放棄して報告する経路の検証）を確認。続いてpolicyを`Any`へ戻すと同一Centralが接続できることを確認。さらに同じリストをscan側で使う`EspBleScanConfig::acceptListOnly`について、空リストでは対象を報告せず、対象アドレスを登録すると報告することを確認。加えて`acceptListEntry()`で登録内容が読み戻せること、`removeFromAcceptList()`で外すと絞り込みが再び一致しなくなること（追加方向だけでなく解除方向）を確認。

59. ✅ `local_identity`: 自分のアドレス取得・送信電力・切断理由指定の検証。Peripheralが`localAddress()`/`localAddressType()`で報告した値が、Centralがスキャンで観測したaddress/addressTypeと一致することを確認。`setTxPower(-12)`と`setTxPower(9)`で、無線が適用した値（`txPower()`）と、advertisingのTx Power Levelとして電波に出る値の両方が追従することを確認。最後に`disconnect(id, 0x16)`の理由コードが相手の`disconnectReason`へ0x16のまま届くこと（backendの0x200オフセットを正規化していること）を確認。

60. ✅ `duplicate_uuid`: 同一UUIDの重複登録の契約を検証。**仕様が認めている重複がどちらの役割でも扱えること**を確認する。Peripheralは同一UUIDのServiceを2つ、同一Service内に同一UUIDのCharacteristicを2つ登録し、いずれも別々のハンドルを得る（属性テーブルをNimBLEホストAPIで直接組むため）。Centralは一覧Discoveryで`services=2 characteristics=3`を列挙し、3つすべてを属性ハンドル指定でRead・購読でき、Notificationが正しいハンドルへ対応付くことを確認する。

61. ✅ `directed_advertising`: Directed Advertisingとadvertisingチャネル選択の検証。Peripheralが`setDirectedTarget(address, type)`でCentralを名指しし、Centralがそのadvertisement（ペイロード無し・connectable=1・scannable=0）だけを受け取って接続できること、`clearDirectedTarget()`で通常のadvertisingへ戻ることを確認。あわせて`setChannelMap(EspBleAdvertisingChannel39)`でch39のみへ絞った状態でも接続が成立することを確認。

62. ✅ `multi_listener`: 1イベントに対する複数observerの配送・解除・上限の検証。`on*()`のprimaryに加え`add*Listener()`で2件を登録し、1回の書き込みで3者すべてが呼ばれることを、`EspBleGattServer`側（`onWritten` + `addWrittenListener`）と`EspBle`側（`onCharacteristicWritten` + `addCharacteristicWrittenListener`）の両方で確認。続いて`removeListener()` / `removeGattListener()`が**指定した1件だけ**を外し、primaryと残りのlistenerには影響しないことを確認。未登録idの削除が`false`を返すこと、listenerが上限4件で頭打ちになり、それ以上のaddが既存を追い出さずに拒否されることも確認。

63. ✅ `hid_convenience`: HID Device側の**便利入力API**が実際に電波へ出す内容の検証。raw `sendReport()`は51・52で押さえているため、このscenarioは一切呼ばない。Deviceはkeyboard/mouse/consumer/system/gamepadを合成し、HostがDiscoveryしてイベントを報告する。文字レベル（`pressKey('a')`→usage 4・修飾子なし、`pressKey('A')`→同じusageにShiftを**呼び出し側が指定せずに**付けること、`tapKey()`が押下と解放の両方を1回で出すこと、`write("hi")`が文字順にtapすること、どのキーでも出せない文字は`INVALID_ARGUMENT`で電波に出さないこと）、usageレベル（`tapUsage()` / `pressUsage()` / `releaseUsage()`でEscape・Ctrl+F1のように`pressKey()`から到達できないキーを送れること、NKRO無効時の`releaseUsage()`は全解放になること）、`setLayout()` / `layout()`（`"`がen-USではShift+`'`（usage 0x34）、ja-JPではShift+`2`（usage 0x1f）になり、**Host側のlayoutはen-USのまま**なのでusageの違いがそのままdeviceの選択を示すこと）、mouse（`wheel()`はx/yを動かさずbuttonsを保つ・`click()`が押下と解放の両エッジを出す・`press()`が置き換えではなく**加算**され`buttons()`が3になる・`releaseAll()`で0へ戻る）、consumer/system（`sendUsage()`が`usage()`へ記録され、`click()`は0へ戻る）、gamepad（`send()`が39個のフィールドへ展開されhat=7が届く・`releaseAll()`で全0）を確認。あわせて`EspBleHidHost`のmulti listener（`addKeyboardListener()`2件＋`addMouseListener()`、1イベントがprimaryと両listenerへ配送されること、`removeListener()`が指定1件だけを外し未登録idでは`false`、上限4件で`RESOURCE_EXHAUSTED`となり既存を追い出さないこと）を確認。

64. ✅ `persistent_subscription_overflow`: persistent subscription registryの上限超過が**黙って捨てられず数えられる**ことの検証。レコードが失われるとその購読は再接続時に復元されず、他に知る手段が無いため、カウンタが唯一の手がかりになる。**1つのアドレスからは埋められない**——centralのアクティブ購読表も16件で先に埋まり、そこで拒否された`subscribe()`はCCCD書き込みへ進まないのでレコードも作られない。同じアドレスへ再接続しても自動復元がアクティブ表を埋め直すので同じ。そこでPeripheralが2バッチの間に`end()`＋`begin()`でownAddressTypeをPublicから`RandomStatic`へ変えて別peerとして見せる。手順は「12件購読（レコード1〜12・dropped=0）→ 切断（アクティブ表は解放、レコードは残る）→ peer再init → 再接続して5件購読（レコード13〜16が入り17件目が溢れる）」で、`droppedPersistentSubscriptionCount()`が1になることを確認。**17件目の`subscribe()`自体は成功する**（失われるのはレコードだけ）ことも同時に確認しており、これがカウンタが必要な理由そのものになっている。なお購読は1件ずつ直列に発行する必要がある——GATT操作queueは実行中1件＋8件なので、12件を一度に投げると9件目以降が`ResourceExhausted`で電波に出る前に拒否され、テストがregistryではなくqueueの話になってしまう。

65. ✅ `gatt_queue_purge`: 接続が終わるときに未処理のGATT操作がどうなるかの検証。どちらも壊れても静かなので、症状が出るまで気づけない経路。**(a) GATT op実行中の`disconnect()`はrejectされず遅延実行される**——rejectしてfalseを返すと、切断を要求した直後のアプリには「まだ繋がっている」と読めてしまう。**(b) queue済みで未開始のopは落とされ、それぞれに失敗完了が届く**——黙って捨てると、アプリは永遠に来ないcallbackを待つことになり、生存接続の前に詰まったままになる。1つの手順で両方を見る: 4件のreadを積み（1件が電波に出て3件がキューに残る）、直後に`disconnect()`を呼ぶ。3件は要求時点で落とされ`InvalidState`＋"connection closed before the queued GATT operation started"が届き、電波に出ていた1件は**打ち切られず正常完了**する。その成功が切断より**先に**届くことが遅延実行の証拠になる（workerの下で接続を壊していたら成立しない）。`droppedEventCount()`が0であること（イベントキュー溢れで見落としていないこと）と、その後の再接続・Discoveryが通ること（ATT slotが握られたままになっていないこと）も確認。

66. ✅ `wifi_ble_coexistence`: P4/C6 ESP-Hosted固有のWi-Fi/BLE共存と共有transport lifecycleを検証。P4でWi-Fiを先に開始してDHCP取得後、同じHosted transportへBLEを追加し、S3 Peerとのscan、接続、GATT read/write、subscribe、notificationがWi-Fi接続を維持したまま成功することを確認する。接続中の`EspBle::end()`はBLE所有分だけを解放してWi-Fiとtransportを維持し、最後の`WiFi.STA.end()`でtransportが解放されることも確認する。資格情報はgit管理外の`.env`からcompile-time defineへ渡す。

67. ✅ `rpa_bond`: 無印ESP32のhost-based privacyを両側で有効化し、scan・接続でOTA RPA（random型、上位bit `01`）を確認する。初回pairingと暗号化GATT read/write後に両端を再起動し、NVSから復元したIRK/LTKで再暗号化する。続いて復元済みbondを削除し、stale resolving-list entryを残さず新規pairingできることまで確認する。
68. ✅ `dual_host_rpa`: 無印ESP32 2台で同梱NimBLE hostと独自Classic hostを同時起動する。Classic HID ACLを維持したままhost生成RPAでpairingし、scanと双方connectionのOTA RPA型を検査する。LE切断後もClassic接続を残し、保存bondから暗号化GATTを再確立する。両roleのhost timeoutを2秒へ短縮し、3周期連続のRPA rotationごとにClassic HID reportを双方向送信しながら、preemptされたadvertising/scanが毎回再開することを確認する。有限8秒のadvertising/scanは3秒後に稼働、元deadlineを越えた9秒後に停止すること、HCI `LE Set Random Address`がNimBLEへ配送されbrokerの未知event・queue overflow・応答不一致が0であることも検査する。変化後のRPAからbond済みLEへ再接続し、最後に両dual-host stackを再起動してClassic再接続、新しいRPAの観測、永続IRK/LTKからのLE暗号化復元を行い、両transportが同時接続中であることを確認する。

69. ✅ `classic_core_host_spp`: EspBleの独自Classic hostと、Arduino-ESP32同梱Bluedroid hostの
    **相互接続**を確認する。peer sketchはEspBleを一切linkせず`BluetoothSerial`だけを使うため、
    2台とも同じstackという条件が外れ、SDP service record、RFCOMM channel、双方向payloadが
    すべてstack境界を越える。peerはchannel 0で接続してEspBle server側のservice recordから
    channelを解決させ、両方向で0 byteを含む4 byte payloadを送ってbinary透過性を確認する。
    peerからの切断でserver側sessionが残らないこと、同じserver instanceへの再接続で新しい
    session idが振られること、EspBle側stackを`end()`+`begin()`し直しても同じaddressで
    再びdial-inできheapが減らないことも確認する。外部機器が無くても相互運用の一部を
    継続的に検証できる位置づけで、対象はSPPのみ——core同梱sdkconfigは`CONFIG_BT_HID_ENABLED`が
    無効なのでClassic HIDはこの構成では試験できない。

実験用 `dual_host_smoke` は、Classic HIDと暗号化LE GATTを接続した両基板で、別taskのClassic scan mode切替とNimBLE `Read RSSI`を同時発行する。FIFO投入数＝物理送信数、最終RSSI成功、broker error 0を確認し、各競合サイクル直後に暗号化GATT readとHID双方向通信を再検証する。`ESPBLE_DUAL_CONTENTION_CYCLES`で反復数を変更できる。さらにtest-onlyのdispatch holdでFIFO満杯と超過拒否を作り、未送信command破棄後のGATT/HID/lifecycle復帰を確認する。偽commandはcontrollerへ送らず、hostへ偽応答も返さない。接続中と両transport切断後にinventoryを取得し、条件付きcleanup commandを含む全opcodeが明示policy内であることも検証する。未知／別host opcodeはdual-host時だけ物理送信前に拒否する。nullと上限超過のHID Input / Output reportを送信前に`InvalidArgument`で拒否し、両接続と直後の通常通信が維持されることも確認する。BLE pairingは最初に誤passkeyを入力して双方の失敗、未暗号化、bond 0、保護GATT拒否とClassic継続を確認し、LE再接続後の正しいpasskeyで暗号化・bond・GATTを復旧する。続いてClassicだけを切断し、最終OPEN失敗の非同期`onConnectionFailed`通知、暗号化LE GATTの継続、正しいpeerへのClassic再接続とHID双方向復旧を確認する。Bluedroid公開HID APIではpage中の接続試行を取り消せないため、独自timeoutによる疑似cancelは試験契約にせずbackendの最終OPEN結果を境界とする。その後peerをsoftware resetで突然消失させ、生存側でLE / BR-EDR双方の切断を検出し、保存bondからBLE暗号化とClassic HIDを再接続してGATT/HID通信を復旧する。lifecycle部はcallback targetの参照寿命barrierを有効にした状態でClassic先行／BLE先行停止、Classic再attach、停止・再登録、両destructor順を通し、panic、watchdog、heap低下がないことを確認する。永続NVDSへ触れる`Write Local Name`はcontroller assertionを起こすため、負荷刺激には使わない。

## 合格条件

- test codeがすべての入力を生成し、Serial assertionで結果を判定する。
- timeoutやretryを含む合否条件が固定されている。
- EspBle同士だけでなく、同梱BLE API直接実装との組み合わせがある。
- 手動確認が必要な項目を自動テスト合格条件へ混ぜない。
