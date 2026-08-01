# ESP-Hosted対応 実行計画

## 目的

ESP32-P4上のNimBLE HostからESP-Hosted経由でESP32-C6のBLE Controllerを利用し、
EspBleの既存公開APIをESP32-S3などの内蔵BLE構成と同じ形で利用できるようにする。

初期対応の対象はArduino-ESP32 3.3.11とする。検証機は次の構成を使用する。

| 役割 | 構成 | Serial port |
| --- | --- | --- |
| Hosted DUT | ESP32-P4 + ESP32-C6 | `/dev/ttyUSB2` |
| 基準Peer | ESP32-S3 | 既存の`.env`設定 |

P4/C6のfirmware準備と更新手順は
[ESP32-P4 / ESP-Hosted セットアップ](ESP_HOSTED_SETUP.ja.md)に分離する。
実機で残った制限と回避策の評価は
[ESP32-P4 / ESP-Hostedの既知制限](ESP_HOSTED_LIMITATIONS.ja.md)に分離する。

## 技術方針

ESP-Hosted専用ライブラリは作らず、EspBle内のNimBLE backendへHostedの
lifecycle処理を追加する。GAP、GATT、Security、HID、MIDIはP4上で動く同じ
NimBLE Host APIを利用し、transport差を公開APIへ露出させない。

Hosted固有差は次の範囲へ限定する。

1. `esp_bt.h`とローカルController用APIを`SOC_BLE_SUPPORTED`で分岐する。
2. Hosted構成では`nimble_port_init()`の前に`hostedInitBLE()`を実行する。
3. 終了時はNimBLE Host停止後に`hostedDeinitBLE()`を実行する。
4. Hostedで利用できない送信電力設定・取得は明示的なunsupported結果にする。
5. 既存のS3/C3/C6/H2向け処理と公開APIを変更しない。

## 実装手順

### Phase 1: compile対応

- `EspBle.cpp`でSoC capabilityを読み込む。
- `esp_bt.h`、`esp_power_level_t`、`esp_ble_tx_power_*()`の利用をローカル
  BLE Controller搭載SoCだけに限定する。
- Hosted構成でも全translation unitがcompile/linkできるようにする。
- ESP32-P4とESP32-S3の`CompileSmoke`をbuildし、S3側の回帰がないことを確認する。

### Phase 2: Hosted lifecycle

- `CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`時だけArduino coreのHosted HALを利用する。
- Hosted transport/Controllerを初期化してからNimBLE Host taskを起動する。
- 初期化途中の失敗を含め、確保済みresourceを逆順に解放する。
- `end()`後の再`begin()`を実機で確認する。

### Phase 3: capability差

- `setTxPower()`はHostedで`false`を返し、`BackendFailure`と理由を設定する。
- `txPower()`はHostedで取得不能値`INT8_MIN`を返す。
- 他の機能差が実機で判明した場合はSoC名ではなくcompile-time capabilityで分岐する。

### Phase 4: Peer test

P4をpytestの親側DUT、S3を`peer_device`側として使用する。既存の
`s3_peer_host` profileは維持し、Hosted検証対象へ`p4_peer_host` profileを追加する。

実行順は次の通りとする。

1. `stack_smoke`または`connect_disconnect`: flash、Serial、scan、接続、切断
2. `gatt_read_write`、`notify_indicate`: GATT Client/Serverの双方向通信
3. `mtu`: ATT MTU交換
4. `security_bond`: pairing、bond保存、再接続
5. `hid_keyboard_host`または`hid_convenience`: HID profile
6. `lifecycle_stress`: peer消失、再接続、lifecycle
7. 問題がなければ残りの`tests/peer/`へP4 profileを展開する

基本コマンドは`tests/`から実行する。

```sh
uv run --env-file .env pytest unit/
uv run --env-file .env pytest peer/connect_disconnect/ \
  --profile p4_peer_host --port /dev/ttyUSB2
```

Peer側は各`peer_device/sketch.yaml`の既存`default_profile`と`.env`のS3 portを使う。
必要に応じて`--peer-profile device:s3_peer_device`を明示する。

## 合格条件

- ESP32-P4とESP32-S3の`CompileSmoke`がcompile/linkに成功する。
- P4上で`begin()`が成功し、S3とのscan、接続、切断が完了する。
- GATT read/write/notify/indicateとMTU交換が成功する。
- bonding後の再接続と、`end()`後の再初期化が成功する。
- Hosted非対応APIがcrashや未定義動作ではなく明示的なerrorを返す。
- 既存unit testがすべて成功する。
- S3の代表buildまたはPeer testに回帰がない。

## 実機で重点確認するリスク

- P4とC6のESP-Hosted firmware互換性
- SDIO pin/transport初期化とC6未応答時のtimeout
- NimBLE初期化失敗時のHosted resource解放
- Wi-FiとBLEが同じESP-Hosted transportを共有する場合の終了処理
- bonding情報、identity address、RPAのHost/Controller間連携
- PHY変更、accept listなどHCI Controller capabilityに依存する操作

Wi-Fi併用と任意SDIO配線は基本BLE動作の合格後に追加検証する。初期実装では
Arduino board variantのESP-Hosted pin設定を使用する。

## 工数見積もり

| 範囲 | 目安 | 内容 |
| --- | ---: | --- |
| compile + 基本BLE | 0.5〜1日 | capability分岐、Hosted lifecycle、P4/S3 build、接続確認 |
| 代表機能の実機検証 | 1〜2日 | GATT、notify/indicate、MTU、Security、HID、再初期化 |
| firmware互換性の解消 | 0.5〜2日 | C6 firmware更新、bond消去、Security/HID再試験 |
| release品質までの追加検証 | 2〜4日 | 全Peer test、Wi-Fi共存、異常系、長時間・繰り返し試験 |

したがって、基本対応は1〜2日、既知問題を解消してrelease品質まで確認する場合は
合計4〜7日程度を見込む。ESP-HostedまたはArduino core自体の修正が必要になった場合は
この見積もりに含めない。

## 同一ライブラリに実装する判断

ESP-Hosted対応はEspBle内へ実装する方がよい。P4でもGAP/GATT/Securityの処理は
同じNimBLE Host APIであり、差分は主に起動・終了とController固有APIに限られる。
今回の変更も`EspBle.cpp`内のcompile-time分岐へ閉じており、公開APIの分岐は不要だった。

別ライブラリにすると、EspBleの大部分とテストを複製し、通常版とHosted版で修正漏れや
機能差が生じる。別ライブラリが妥当なのは、将来NimBLEとは異なるHost stackを採用する、
またはHosted transport自体をEspBleとは独立した製品として公開する場合に限る。

## 2026-08-01 実装・検証結果

対象環境はArduino-ESP32 3.3.11、P4側ESP-Hosted Host 2.12.11である。検証開始時の
C6側Slave 2.3.2は、検証中に2.12.11へ更新した。

| 検証 | 結果 | 備考 |
| --- | --- | --- |
| P4 `CompileSmoke` | 成功 | 317,952 bytes、global 22,532 bytes |
| S3 `CompileSmoke` | 成功 | 274,253 bytes、global 21,920 bytes |
| host unit test | 7 passed | `uv run --env-file .env pytest unit/` |
| P4/S3 connect/disconnect | 1 passed | scan、接続、切断を確認 |
| P4/S3 GATT・notify・MTU | 4 passed | read/write、notify/indicate、MTU交換を確認 |
| P4/S3 Security・HID・lifecycle | 7 passed / 3 failed | 下記既知制限を検出 |
| S3/S3 Security回帰 | 1 passed | 通常Controller構成ではbond成功 |

### 確認できた既知制限

1. Securityとbonding
   - P4側はbackend status `1291` (`0x50b`)、S3側は`1035` (`0x40b`)で失敗した。
   - いずれもSecurity ManagerのDHKey check failure (`0x0b`)を示す。
   - C6 Slaveを2.3.2から2.12.11へ更新し、Host/Slaveがともに2.12.11であることを
     確認した後も同じDHKey check failureが再現した。firmware version不一致が原因ではない。
   - P4側だけLE Secure Connectionsを無効にする切り分けでは、初回のLegacy pairing、
     bond保存、暗号化GATTが成功した。このためESP-Hosted経由のSecure Connections
     またはDHKey処理に問題があると判断する。
   - Legacy pairingでbond再接続するとS3側は暗号化成功したが、P4側にSecurity eventが
     通知されずtimeoutした。接続状態のpollも試したが、P4側では`encrypted=1`に対して
     `bonded=0`、`key_size=0`しか得られず、正しいSecurity状態を復元できなかった。
     暗黙のLegacy downgradeやbond状態の推測は行わず、既知制限とする。
2. `end()` / `begin()`の繰り返し
   - 1回目の再初期化は成功したが、2回目にESP-Hosted SDIO driverの
     `sdio_mempool_create`がメモリ確保失敗でassertした。
   - Slave 2.12.11への更新後は対象testの単独実行が1回成功したが、lifecycle suite
     全体では7 passed / 1 failedとなり、同じassertが再現した。解消済みとは扱わない。
   - Arduino coreにもsecond initが未修正である旨のコメントがあり、EspBle外の
     Hosted transport再初期化制限と判断する。
   - deinit後に250 ms待機してもlifecycle suiteは`7 passed / 1 failed`で同じassertが
     再現した。非同期cleanup待ちでは回避できない。
   - `end()`でHostedを解放しない切り分けではlifecycle suiteが`8 passed`となったが、
     電力・SDIO resource・Wi-Fi共有時の意味を変えるため初期実装には入れない。
     通常の1回だけの`begin()`運用は成功している。
   - ESP-Hosted-MCU 2.12.12には、各init/deinit cycleでshared channel mempoolが漏れる
     問題の修正commit `d0f4646`が含まれる。Arduino Core 3.3.11は2.12.11を同梱するため、
     Core更新後に実機で再評価する。

### 現時点の判定

compile、接続、GATT、notify/indicate、MTUまでを初期対応済みとする。Security、HIDの
bonding経路と複数回の完全再初期化は未合格であり、ESP-Hosted対応全体を完全合格とは
しない。Securityはversionを揃えても再現するためESP-Hosted/Arduino coreの上流issue
候補とし、再初期化と合わせてcoreの修正状況を追跡する。

## 完了時の記録

実装完了後、この文書へ実行したcore version、対象test、成功・失敗結果、既知の
制限を追記する。実機未確認項目を「対応済み」とは扱わない。
