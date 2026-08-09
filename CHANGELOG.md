# Changelog / 変更履歴

## Unreleased

- (EN) Added experimental original-ESP32 Bluetooth Classic support backed by a
  separately built, namespaced Classic-only Bluedroid host. SPP, generic HID
  Device/Host, custom reports, reconnect and lifecycle paths are covered on two
  original ESP32 boards. The archive enables Classic HID APIs that are absent
  from Arduino-ESP32's built-in Bluedroid configuration.
- (JA) 無印ESP32向けBluetooth Classic実験対応を追加。独自build・名前空間化した
  Classic-only Bluedroid hostを使い、SPP、generic HID Device/Host、custom report、
  再接続、lifecycleを無印ESP32 2台で検証した。Arduino-ESP32内蔵Bluedroidでは
  無効なClassic HID APIも独自archiveでは有効にしている。
- (EN) Added opt-in experimental NimBLE + Classic dual-host operation on the
  original ESP32. The HCI broker owns command FIFO/credits, event-mask union,
  command-response and ACL-handle routing, controller lifecycle, Classic
  reattachment virtualization, and host-based RPA recovery. Hardware tests cover
  encrypted GATT plus Classic HID, bonding, both shutdown orders, restart,
  contention, full-FIFO backpressure recovery, and a multi-hour soak. Observed
  commands are classified by transport and scope; unknown or wrong-host commands
  fail closed only in dual-host mode. Invalid HID reports are rejected locally,
  and bonded LE plus Classic HID recover after an abrupt peer reboot. Connection
  and pairing failures remain a release gate.
- (JA) 無印ESP32でNimBLE + Classicを同時利用するopt-in実験機能を追加。HCI brokerが
  command FIFO/credit、event mask union、command応答・ACL handle routing、controller
  lifecycle、Classic再attach仮想化、host-based RPA復帰を管理する。暗号化GATTと
  Classic HID、bond、任意停止順、再起動、command競合、FIFO満杯復帰、数時間級soakまで
  実機検証済み。観測commandをtransportとscopeで分類し、dual-host時だけ未知／別host
  commandをfail-closedにした。不正HID reportは接続を維持したまま拒否し、peer突然再起動後も
  bond済みLEとClassic HIDを復旧する。接続失敗とpairing失敗はrelease gateとして残る。
- (EN) Added a reproducible Classic-host archive builder pinned to ESP-IDF
  v5.5.5 and xtensa-esp32 GCC 14.2.0, with link checks, global-symbol
  namespacing, required-symbol validation and SHA-256 reporting.
- (JA) Classic host archive生成をESP-IDF v5.5.5 / xtensa-esp32 GCC 14.2.0へ
  固定し、link check、global symbol名前空間化、必須symbol検査、SHA-256表示を追加した。

## 1.2.0
- (EN) Added support for the original ESP32. Its Arduino-ESP32 prebuilt libraries
  are built with Bluedroid, so EspBle now bundles the NimBLE host for that chip
  only (`src/nimble_esp32/`, vendored by `tools/vendor_nimble_esp32.py` from the
  esp-nimble commit the matching esp-idf pins, with the configuration frozen to
  the values the other targets use). Every bundled source is guarded, so the
  other targets keep using the NimBLE bundled with the core and their binaries
  are unchanged. Central and peripheral roles are verified on hardware. The
  unsupported cases come from the BLE 4.2 controller -- `phy_update` cannot pass because there is no LE
  2M PHY, and extended/periodic advertising and LE Coded PHY are unavailable.
- (JA) 無印ESP32へ対応した。Arduino-ESP32のプリビルドがBluedroidであるため、この
  チップ向けにだけNimBLE hostを同梱する（`src/nimble_esp32/`。
  `tools/vendor_nimble_esp32.py`が、対応するesp-idfがpinするesp-nimbleのcommitから
  取得し、設定値は他ターゲットと同一に固定する）。同梱ソースは全ファイルガード付き
  なので、他ターゲットはcore同梱NimBLEをそのまま使い、生成物も変わらない。実機で
  Central / Peripheralの両役割を検証した。非対応項目はBLE 4.2 controller由来の制約で、
  LE 2M PHYがないため`phy_update`は通らず、
  Extended / Periodic AdvertisingとLE Coded PHYも利用できない。
- (EN) Removed the `ble_keybridge_keyboard` peer suite: the ESP32KeyBridge input
  adapter is verified in that library's own repository.
- (JA) `ble_keybridge_keyboard` peer suiteを削除した。ESP32KeyBridge input adapterは
  当該ライブラリ側で検証する。
- (EN) Fixed `local_identity` to disconnect with reason `0x13`. Only the reasons
  the Core specification lists for HCI_Disconnect may be sent by a host; `0x16`
  is reported by the controller, and the original ESP32's controller rejects it.
- (JA) `local_identity`の切断理由を`0x13`に修正した。HCI_Disconnectへ指定できるのは
  仕様が列挙する理由コードだけで、`0x16`はcontrollerが報告する値であり、無印ESP32の
  controllerはこれを拒否する。
- (EN) Fixed the `EspBleGattServer::addCharacteristic()` comment, which still
  said two characteristics in one service may not share a UUID. The
  implementation registers both and each call returns its own handle, which is
  what `docs/FEATURE_MATRIX.md`, `docs/GUIDE_BLE_BASICS.md` and the
  `duplicate_uuid` peer test already describe. Also documented the restriction
  that does exist: one characteristic may not carry the same descriptor UUID
  twice.
- (JA) `EspBleGattServer::addCharacteristic()`のコメントを修正。「同一Service内で
  同一UUIDのCharacteristicは置けない」と書かれたままだったが、実装は両方を登録し
  それぞれのhandleを返す（`docs/FEATURE_MATRIX.md`・`docs/GUIDE_BLE_BASICS.md`・
  `duplicate_uuid` peerテストが記述しているとおり）。実際に存在する制約（1つの
  Characteristic内でDescriptor UUIDは重複できない）も明記した。

## 1.1.0
- (EN) Added limited ESP32-P4 + ESP32-C6 ESP-Hosted support, including the
  Hosted BLE lifecycle, GAP/GATT peer coverage, and Wi-Fi/BLE shared-transport
  coexistence. LE Secure Connections and repeated full reinitialization remain
  limited by upstream Arduino-ESP32/ESP-IDF and ESP-Hosted 2.12.11 issues.
- (JA) ESP32-P4 + ESP32-C6のESP-Hosted構成へ制限付きで対応。Hosted BLEの
  lifecycle、GAP/GATT Peer検証、Wi-Fi/BLE共有transportの共存検証を追加した。
  LE Secure Connectionsと複数回の完全再初期化は、Arduino-ESP32/ESP-IDFおよび
  ESP-Hosted 2.12.11の上流問題による既知制限として残る。
- (EN) Documented ESP-Hosted SDIO board-pin selection and the pre-initialization
  runtime override, with a Tab5/custom-board example.
- (JA) ESP-Hosted SDIOのboard pin選択と初期化前の実行時上書きを文書化し、
  Tab5・独自基板向けexampleを追加した。
- (EN) Added explicit ESP32-C3, ESP32-C6, ESP32-H2, and ESP32-P4 Arduino CLI
  profiles to every example so cross-board build jobs no longer skip examples
  because their `sketch.yaml` lacks the requested profile. The core compatibility
  workflow now tests S3, C3, C6, H2, and P4 by default.
- (JA) 全exampleの`sketch.yaml`へESP32-C3、ESP32-C6、ESP32-H2、ESP32-P4の
  Arduino CLI profileを明示追加し、profile不在によるcross-board buildのskipを解消した。
  Core互換性workflowの既定targetもS3、C3、C6、H2、P4へ拡張した。

## 1.0.0
- (EN) Initial release
- (JA) 初期リリース
