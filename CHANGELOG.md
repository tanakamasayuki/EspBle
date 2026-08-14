# Changelog / 変更履歴

## Unreleased

- (EN) Added randomized fault injection for the HCI router, command scheduler
  and controller policy. It builds them with AddressSanitizer and
  UndefinedBehaviorSanitizer and feeds truncated, oversized and mutated H4
  packets from exact-size heap buffers, plus deterministic probes for the table
  exhaustion and null-argument paths a random walk rarely reaches. The three
  modules reach full line coverage from this test alone.
- (JA) HCI router、command scheduler、controller policyへrandomized fault
  injectionを追加した。AddressSanitizer / UndefinedBehaviorSanitizer付きでbuildし、
  切り詰め・過長・変異させたH4 packetを実サイズのheap bufferから与える。table満杯と
  NULL引数のように乱択では踏みにくい経路は決定的な検査として併記した。この試験だけで
  3モジュールとも行カバレッジ100%に到達する。

- (EN) Fixed a crash in the bundled NimBLE port: removing an event from an
  event queue sampled the queue depth under a function-local spinlock, so a
  concurrent dequeue by the host task on the other core made the following
  receive fail and abort the firmware. Hardware runs hit it through GATT
  discovery issued from EspBle's operation task. Removal is now best effort and
  stops when the queue drains early.
- (JA) 同梱NimBLE portのcrashを修正した。event queueからのevent削除がqueue長を
  function-localなspinlock下で読んでいたため、別coreのhost taskが先にdequeueすると
  続く受信が失敗してfirmwareがabortしていた。実機ではEspBleのoperation taskから出す
  GATT discoveryで発生した。削除をbest effortにし、queueが先に空になったら中断する。

- (EN) Removed the `ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL` build flag on the
  original ESP32. The HCI broker now follows the hosts a sketch actually
  starts: one registered host stays a pass-through, and starting both `EspBle`
  and `EspBleClassic` routes HCI between them. Whichever host starts the
  controller hands its shutdown to the broker, and a linked Classic host keeps
  the controller in dual mode so BLE and Classic can be started, stopped and
  restarted in either order. Dual-host stays experimental; stop one host to fall
  back to a single-host path.
- (JA) 無印ESP32の`ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL` build flagを廃止した。
  HCI brokerはsketchが実際に開始したhostに従い、1 hostならpass-through、
  `EspBle`と`EspBleClassic`の両方を開始した場合だけHCIをroutingする。controllerを
  起動したhostが停止責任をbrokerへ委譲し、Classic hostがlinkされていればcontrollerを
  dual modeで起動するため、BLEとClassicを任意の順で開始・停止・再開できる。
  dual-hostは実験扱いのままで、一方を停止すれば単一host経路へ戻る。

- (EN) Fixed the example compile workflow, which built every sketch with the
  `esp32s3` profile and therefore always failed on the original-ESP32-only
  Classic examples. Each sketch now builds with the profile it actually ships.
- (JA) example compile workflowが全sketchを`esp32s3` profileでbuildしていたため、
  無印ESP32専用のClassic exampleが必ず失敗していた問題を修正した。各sketchが持つ
  profileでbuildする。

- (EN) Original-ESP32 Classic-only sketches now select EspBle's custom host
  automatically; published Classic examples no longer need `build_opt.h`.
  Only experimental dual-host and test-only instrumentation keep explicit flags.
- (JA) 無印ESP32のClassic-only sketchはEspBleの独自hostを自動選択するようにし、
  公開Classic exampleから`build_opt.h`を除去した。明示flagは実験的dual-hostと
  テスト専用instrumentationにだけ残す。

- (EN) Added a configurable Classic-only A2DP transport soak using the regular
  two-board fixture, with exact packet/byte accounting, backpressure recovery,
  and heap-watermark reporting.
- (JA) 通常の2台構成fixtureを使うClassic-only A2DP transport soakを追加し、
  packet/byteの厳密な照合、backpressure復帰、heap watermarkを記録するようにした。

- (EN) Fixed the compatibility-matrix representative GATT paths and made the
  tool fail when a requested example no longer exists, preventing silent
  coverage gaps after example moves.
- (JA) compatibility matrixの代表GATT pathを修正し、指定exampleが存在しない場合は
  error終了するようにした。example移動後に検証が黙って欠落することを防止する。

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
  and bonded LE plus Classic HID recover after an abrupt peer reboot. HID Host
  now reports final asynchronous connection failures; hardware coverage verifies
  wrong-passkey recovery without a stale bond and Classic reconnection while
  encrypted LE remains live. SPP and HID callback teardown now holds backend
  state until all callbacks that acquired it have returned.
- (JA) 無印ESP32でNimBLE + Classicを同時利用するopt-in実験機能を追加。HCI brokerが
  command FIFO/credit、event mask union、command応答・ACL handle routing、controller
  lifecycle、Classic再attach仮想化、host-based RPA復帰を管理する。暗号化GATTと
  Classic HID、bond、任意停止順、再起動、command競合、FIFO満杯復帰、数時間級soakまで
  実機検証済み。観測commandをtransportとscopeで分類し、dual-host時だけ未知／別host
  commandをfail-closedにした。不正HID reportは接続を維持したまま拒否し、peer突然再起動後も
  bond済みLEとClassic HIDを復旧する。HID Hostの最終的な非同期接続失敗を通知し、誤passkey後に
  stale bondを残さず再pairingできること、暗号化LEを維持してClassicを再接続できることも検証した。
  SPP/HID callback解除では、取得済みcallbackが完了するまでbackend stateを保持するbarrierを追加した。
- (EN) Added a reproducible Classic-host archive builder pinned to ESP-IDF
  v5.5.5 and xtensa-esp32 GCC 14.2.0, with link checks, global-symbol
  namespacing, required-symbol validation and SHA-256 reporting.
- (JA) Classic host archive生成をESP-IDF v5.5.5 / xtensa-esp32 GCC 14.2.0へ
  固定し、link check、global symbol名前空間化、必須symbol検査、SHA-256表示を追加した。
- (EN) Added original-ESP32 Classic-only A2DP Sink/Source encoded-media
  transport and AVRCP Controller/Target control. Two-board hardware coverage
  combines A2DP streaming with passthrough commands, responses, absolute volume,
  and volume-change notification. Codec/PCM/device I/O remains outside EspBle.
- (JA) 無印ESP32のClassic-only A2DP Sink/Source encode済みmedia transportと
  AVRCP Controller/Target制御を追加。ESP32 2台でA2DP streamとpassthrough操作・応答、
  absolute volume、音量変更通知の併用を確認した。codec/PCM/device I/OはEspBleの外に保つ。
- (EN) Added an experimental HFP Client transport for the original ESP32:
  service-level connection and call control, CVSD/mSBC Voice-over-HCI views and
  copy sends, bad-frame reporting, and packet statistics. Two ESP32 boards
  negotiate mSBC and exchange synchronous payloads through a test Audio Gateway.
- (JA) 無印ESP32向け実験的HFP Client transportを追加。SLCとcall control、CVSD/mSBCの
  Voice-over-HCI view/copy送信、bad-frame、packet statisticsを公開し、ESP32 2台の
  テスト用Audio Gatewayとの間でmSBC negotiationと同期payload往復を確認した。
- (EN) Added a public experimental HFP Audio Gateway with automatic mandatory
  SLC query responses, a deliberately small single-call model, application-side
  telephony command events, raw SCO transport, and process-wide Client/AG
  exclusion. The public Client and Gateway exchange mSBC payloads on hardware.
- (JA) 必須SLC照会の自動応答、意図的に小さい単一call model、application側telephony
  command event、raw SCO transportを持つ実験的HFP Audio Gatewayを公開した。
  Client/AGはprocess-wideで排他とし、公開API同士のmSBC payload往復を実機確認した。
- (EN) Added Audio Gateway codec selection and verified Classic-only CVSD raw
  SCO in both directions, including audio disconnect and reconnection within the
  same call and full SLC reconnect followed by another call. mSBC remains the default.
- (JA) Audio Gatewayのcodec選択を追加し、Classic-only CVSD raw SCOの双方向transport、
  同一call中のaudio再接続、SLC再接続後の再発信を実機確認した。既定値は引き続きmSBCである。
- (EN) Extended experimental dual-host routing to HFP mSBC SCO alongside a
  live LE GATT connection, including synchronous-handle ownership and Classic
  voice/eSCO command policy.
- (JA) 実験的dual-host routingをHFP mSBC SCOへ拡張し、BLE GATT接続中の双方向音声payload、
  同期接続handle所有権、Classic voice/eSCO command policyを実機確認した。
- (EN) Extended dual-host hardware coverage to A2DP encoded-media streaming and
  AVRCP playback/absolute-volume control while LE GATT stays usable before,
  during, and after the stream. The broker now classifies ESP's A2DP coexistence
  vendor command instead of rejecting it.
- (JA) dual-host実機検証をA2DP encode済みmedia転送とAVRCP再生・absolute volumeへ
  拡張し、stream前・中・切断後もLE GATTが利用できることを確認した。ESPのA2DP共存状態
  vendor commandをbroker policyへ分類し、拒否せずcontrollerへ配送する。

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
