# Changelog / 変更履歴

## Unreleased
- (EN) Verified with Arduino-ESP32 3.3.13 (ESP-IDF release/v5.5, ESP-Hosted host
  2.13.1): every peer suite on ESP32-S3, the original ESP32 and P4 + C6, and the
  examples and tests now pin 3.3.13. The original-ESP32 `#warning` for unverified
  cores now starts above 3.3.13. On P4 + C6 the first LE Secure Connections
  pairing now succeeds, but security on a bonded reconnect is still not reported,
  so Security/bonding there remains unsupported.
- (JA) Arduino-ESP32 3.3.13（ESP-IDF release/v5.5、ESP-Hosted Host 2.13.1）で検証した。
  ESP32-S3、無印ESP32、P4 + C6で全Peer suiteを実行し、exampleとtestのpinを3.3.13にした。
  無印ESP32の未検証Coreの`#warning`は3.3.13超から出る。P4 + C6では初回のLE Secure
  Connections pairingが成功するようになったが、bond後の再接続でSecurityが報告されないため、
  そこでのSecurity/bondingは引き続き未対応。
- (EN) Moved to Arduino-ESP32 4.0.0 (ESP-IDF v6.1.1, GCC 15.2): the examples and tests
  now pin 4.0.0, and every peer pairing passes on it (S3, the original ESP32, P4 + C6,
  ESP32-S31). The original-ESP32 `#warning` for unverified cores now starts above
  4.0.0. Which cores this release builds on is recorded in
  `docs/COMPATIBILITY.<version>.md`. The test sketches now need 4.0.0, because ten
  counterpart sketches use the core's new `BLE.h` API; measuring an older core uses an
  earlier commit of the suite.
- (JA) Arduino-ESP32 4.0.0（ESP-IDF v6.1.1、GCC 15.2）へ移行した。exampleとtestのpinを4.0.0にし、
  全Peerの組（S3、無印ESP32、P4 + C6、ESP32-S31）が4.0.0で成功する。無印ESP32の未検証Coreの
  `#warning`は4.0.0超から出る。このリリースでbuildできるCoreは`docs/COMPATIBILITY.<version>.md`に記録する。
  相手側sketchのうち10本がCoreの新しい`BLE.h` APIを使うため、test sketchは
  4.0.0が必要になった。古いCoreを測るときはsuiteの以前のcommitを使う。
- (EN) Added the ESP32-S31 (BT 5.4 LE, built-in controller, the core's bundled
  NimBLE), available from core 4.0.0 only. Examples gain an `esp32s31` profile that
  the compile-examples workflow builds, and the peer tests gain `s31_peer_host` /
  `s31_peer_device` with the combos `s31_s3` and `s3_s31`.
- (JA) ESP32-S31（BT 5.4 LE、内蔵controller、Core同梱NimBLE）に対応した。Core 4.0.0以降のみ。
  exampleに`esp32s31` profileを追加してcompile-examples workflowでbuildし、Peer testに
  `s31_peer_host` / `s31_peer_device`と組`s31_s3` / `s3_s31`を追加した。
- (EN) Core 4.0.0 builds every bundled NimBLE host with extended advertising, which
  makes the legacy advertising calls fail. EspBle now advertises through
  extended-advertising set 0 with legacy PDUs and accepts legacy reports that arrive
  through extended scanning; nothing changes for sketches. 3.x cores and the host
  bundled for the original ESP32 keep the legacy calls.
- (JA) Core 4.0.0は同梱の全NimBLE hostをextended advertising有効でbuildしており、legacyの
  advertising呼び出しが失敗する。EspBleはextended advertisingのset 0からlegacy PDUで広告し、
  extended scanで届くlegacy reportも受け取るようにした。sketch側の変更は不要。3.x系Coreと
  無印ESP32向けの同梱hostは従来どおりlegacyの呼び出しを使う。
- (EN) The core's NimBLE drops the Encryption Change of a bonded reconnect when the
  Central's encryption and the Peripheral's Security Request cross; fast controllers
  (the ESP32-S31, and the C6 behind a P4) hit it on every reconnect. EspBle now
  reports such a link once it has stayed encrypted for 300 ms without an event and
  the peer has a stored bond, taking the bond flags from the store. This resolves the
  missing security event on a P4 central's bonded reconnect: on 4.0.0 (ESP-Hosted
  2.13.1 on both the P4 and the C6) `security_bond` and `hid_keyboard_host` pass with
  the P4 as Central, so Security, bonding and HID on P4 + C6 are now verified on
  4.0.0. Earlier cores have not been re-measured with this recovery.
- (JA) Core同梱のNimBLEは、bond後の再接続でCentralの暗号化要求とPeripheralのSecurity Requestが
  交差するとEncryption Changeを落とす。応答の速いcontroller（ESP32-S31、P4の先のC6）では毎回の
  再接続で起きる。EspBleは、eventが無いまま300 ms暗号化が続き、相手のbondが保存されている
  linkを、保存済みのbond情報で報告するようにした。これでP4 centralのbond再接続でSecurity eventが
  出ない問題が解消し、4.0.0（P4とC6ともESP-Hosted 2.13.1）ではP4をCentralにした
  `security_bond`と`hid_keyboard_host`が成功する。P4 + C6のSecurity、bonding、HIDは4.0.0で
  検証済みになった。それより前のCoreは、この対処を入れてからは未測定。
- (EN) The NimBLE host bundled for the original ESP32 moves from esp-nimble `685675c`
  (ESP-IDF v5.5.5) to `41057af`, the pin of ESP-IDF `release/v5.5` `a45de8cc` that
  Arduino-ESP32 3.3.13 is built from (it contains everything in ESP-IDF v6.1.1's pin).
  It brings the fixes for CVE-2026-45816 and CVE-2026-45815, hardened BLERP re-pairing
  and bond-store fixes. The configuration is frozen from 3.3.13's ESP32-S3 sdkconfig,
  which adds hardened SMP re-pairing, bond removal on a failed pairing as Central and
  deferred connection events. In dual-host mode the HCI broker now routes LE Create
  Connection Cancel (0x200E) to NimBLE.
- (JA) 無印ESP32向けの同梱NimBLE hostを、esp-nimble `685675c`（ESP-IDF v5.5.5）から`41057af`へ
  更新した。Arduino-ESP32 3.3.13のbuild元であるESP-IDF `release/v5.5` `a45de8cc`のpinで、
  ESP-IDF v6.1.1のpinの内容をすべて含む。CVE-2026-45816 / CVE-2026-45815の修正、BLERP
  再pairingの強化、bond storeの修正が入る。設定は3.3.13のESP32-S3 sdkconfigから固定し、
  SMPの再pairing強化、Central側でのpairing失敗時のbond削除、接続eventの遅延処理が加わった。
  dual-host動作ではHCI brokerがLE Create Connection Cancel（0x200E）をNimBLEへroutingする。
- (EN) The Classic Bluedroid archive deliberately stays at ESP-IDF v5.5.5 / GCC 14.2.0,
  verified on 4.0.0. Archives rebuilt from v5.5.6 and from `release/v5.5` `a45de8cc`
  assert in `xQueueSemaphoreTake` when the Classic host is ended (a shutdown race in
  `hci_layer` introduced after v5.5.5), so the Bluedroid security fixes made since
  v5.5.5 are not taken yet. Details are in `docs/CLASSIC_HOST_BUILD.ja.md`.
- (JA) Classic Bluedroid archiveは意図してESP-IDF v5.5.5 / GCC 14.2.0のままとし、4.0.0で動作を
  確認した。v5.5.6と`release/v5.5` `a45de8cc`から作り直したarchiveは、Classic hostの終了時に
  `xQueueSemaphoreTake`でassertする（v5.5.5より後に入った`hci_layer`の終了処理の競合）。この
  ため、v5.5.5以降のBluedroidのセキュリティ修正はまだ取り込んでいない。詳細は
  `docs/CLASSIC_HOST_BUILD.ja.md`にある。

## 1.3.0
- (EN) No breaking change: every public declaration of 1.2.0 keeps its signature,
  so sketches written against 1.2.0 compile unchanged. Targets other than the
  original ESP32 produce the same BLE binaries; everything below concerns the
  original ESP32 unless it says otherwise.
- (JA) 破壊的変更なし。1.2.0のpublicな宣言はすべて同じsignatureで残っているので、1.2.0向けの
  sketchはそのままcompileできる。無印ESP32以外のtargetのBLE生成物は変わらない。以下は
  断りが無い限り無印ESP32の話である。

### Bluetooth Classic / 無印ESP32のBluetooth Classic
- (EN) Added Bluetooth Classic (BR/EDR) through `EspBleClassic`: inquiry with
  name/service lookup, sketch-owned visibility, Class of Device and radio
  settings, Secure Simple Pairing with application involvement and bond
  management, SPP (up to four services, binary-safe, plus an Arduino `Stream`
  wrapper), HID Device and Host with the same API shape as BLE HID, A2DP
  Sink/Source and AVRCP Controller/Target carrying already-encoded media, and an
  HFP Client and Audio Gateway with selectable CVSD/mSBC raw SCO transport. It is
  backed by a separately built, namespaced Classic-only Bluedroid host that ships
  as an archive; no compiler flag — using the class is what selects it. Each
  feature's state (hardware-verified / unverified / unimplemented) and the
  deliberately-not-implemented list, with reasons, are in
  `docs/CLASSIC_FEATURE_INVENTORY.ja.md`; the concepts are in
  `docs/GUIDE_CLASSIC_BASICS.md` and the BLE-or-Classic choice in
  `docs/CLASSIC_VS_BLE.md`. New examples cover SPP, HID (composite, gamepad,
  NKRO, mouse, media keys), A2DP, AVRCP and radio settings, each with a bilingual
  README.
- (JA) `EspBleClassic`でBluetooth Classic（BR/EDR）を利用できるようにした。name / service
  問い合わせ付きのinquiry、sketchが所有するvisibilityとClass of Deviceと無線設定、
  applicationが関与するSecure Simple Pairingとbond管理、SPP（最大4 service、binary safe、
  Arduino `Stream`ラッパ付き）、BLE HIDと同じAPI形状のHID Device / Host、encode済みmediaを
  運ぶA2DP Sink / SourceとAVRCP Controller / Target、CVSD / mSBCを選べるraw SCO transportの
  HFP Client / Audio Gatewayを持つ。独自build・名前空間化したClassic-only Bluedroid hostを
  archiveとして同梱し、compiler flagは不要——このclassを使うかどうかで決まる。機能ごとの状態
  （実機検証済み / 未検証 / 未実装）と、理由付きの「意図して実装しない」一覧は
  `docs/CLASSIC_FEATURE_INVENTORY.ja.md`、概念は`docs/GUIDE_CLASSIC_BASICS.ja.md`、
  無線の選び方は`docs/CLASSIC_VS_BLE.ja.md`にある。SPP、HID（複合・gamepad・NKRO・mouse・
  メディアキー）、A2DP、AVRCP、無線設定のexampleを追加し、全数が日英READMEを持つ。
- (EN) Supported core range, measured rather than assumed: Arduino-ESP32 3.2.0
  through 3.3.11, hardware-verified at 3.2.1 / 3.3.0 / 3.3.7 / 3.3.8 / 3.3.10 /
  3.3.11. The Bluedroid API headers the archive was built against ship in
  `src/esp32/include/`, so the compile-time contract cannot drift with the core
  version; a small compat source supplies IDF 5.5's `esp_log` on IDF 5.4 cores,
  and EspBle now participates in the transitional BT-memory-claim scheme of
  3.3.7/3.3.8, where previously neither BLE nor Classic could start. HFP audio
  (SCO) needs 3.3.8 or newer — older cores ship a controller built with the PCM
  audio path, which a host library cannot change. Below 3.2.0 the build stops at
  an `#error` that says why; newer, unverified cores build with a `#warning`.
- (JA) 対応Coreの範囲を実測で確定した: Arduino-ESP32 3.2.0〜3.3.11。3.2.1 / 3.3.0 / 3.3.7 /
  3.3.8 / 3.3.10 / 3.3.11は実機確認済み。archiveのbuildに使ったBluedroid API headerを
  `src/esp32/include/`へ同梱したため、compile時の契約はCoreの版でずれない。IDF 5.4系の
  coreには5.5の`esp_log`が無いので小さな互換sourceで補い、3.3.7 / 3.3.8だけが使う過渡期の
  BTメモリ保持方式にも参加する（従来この2版ではBLEもClassicも起動できなかった）。
  HFP audio（SCO）だけは3.3.8以上が必要——それ以前のcontrollerはPCM audio pathでbuildされて
  おり、host library側では変更できない。3.2.0未満は理由を書いた`#error`で止まり、未検証の
  新しいcoreは`#warning`付きでbuildできる。

### BLE and Classic at the same time (experimental) / BLEとClassicの同時利用（実験）
- (EN) On the original ESP32, starting both `EspBle` and `EspBleClassic` routes
  them through an HCI broker; starting one leaves it a pass-through, and there is
  no build flag. The broker owns the command FIFO and credits, both hosts' event
  masks, response and ACL routing, controller lifecycle (either host may stop
  first) and controller-to-host ACL flow control, which no single host can do on
  a shared controller. Hardware coverage includes encrypted GATT alongside
  Classic HID, both shutdown orders, HFP mSBC SCO, A2DP with AVRCP and a
  multi-hour soak. It stays experimental; internals and diagnostics are in
  `docs/GUIDE_ADVANCED.md`.
- (JA) 無印ESP32で`EspBle`と`EspBleClassic`の両方を開始するとHCI brokerが両者をroutingする。
  片方だけならpass-throughで、build flagは無い。brokerはcommand FIFOとcredit、両hostの
  event mask、応答とACLのrouting、controller lifecycle（どちらから停止してもよい）、そして
  単独hostでは実行できないcontroller-to-host ACL flow controlを所有する。実機では暗号化GATTと
  Classic HIDの併用、両方の停止順、HFP mSBC SCO、A2DPとAVRCP、数時間級soakまで確認した。
  実験扱いのままで、内部と診断は`docs/GUIDE_ADVANCED.ja.md`にある。

### BLE
- (EN) Fixed a crash in the bundled NimBLE port: removing an event from a queue
  raced a concurrent dequeue by the host task on the other core and aborted the
  firmware; removal is best effort now. Added examples for features that had
  none: `Hid/Gamepad`, `Gap/MultiConnection` and `Hosted/WifiCoexistence`.
- (JA) 同梱NimBLE portのcrashを修正した。event queueからの削除が別coreのhost taskの
  dequeueと競合してfirmwareがabortしていたため、削除をbest effortにした。exampleの無かった
  機能へ`Hid/Gamepad`、`Gap/MultiConnection`、`Hosted/WifiCoexistence`を追加した。

### Fixes / 修正
- (EN) Classic fixes: pairing no longer settles on Just Works regardless of the
  configured IO capability; a failed pairing now ends the pending SPP attempt and
  notifies the caller; the Classic HID Host no longer discards input reports from
  devices that use report IDs; first-time dual-host pairing no longer times out
  at the HCI layer (bonded peers already worked); and `respondToUnknownAt()`
  closes AT exchanges the backend does not decode, which used to leave the peer
  waiting forever.
- (JA) Classicの修正: IO capabilityを何に設定してもpairingがJust Worksになっていたのを直した。
  pairing失敗時に宙吊りだったSPP接続試行を終わらせ、呼び出し側へ通知する。Classic HID Hostが
  report IDを使うdeviceのInput Reportをすべて捨てていたのを直した。dual-hostの初回pairingが
  HCI層でtimeoutしていたのを直した（bond済みの相手は元から通っていた）。backendが解釈しない
  AT commandへの応答で交換が閉じず相手が待ち続けていたのを、`respondToUnknownAt()`が
  閉じるようにした。
- (EN) Tool fixes: the example-compile workflow builds each sketch with its own
  profile (the original-ESP32-only Classic examples always failed before), and
  the compatibility-matrix tool fails on a missing requested example instead of
  silently skipping it.
- (JA) toolの修正: example compile workflowが各sketch自身のprofileでbuildするようにした
  （無印ESP32専用のClassic exampleが必ず失敗していた）。compatibility matrix toolは指定
  exampleが存在しないとき黙って飛ばさずerror終了にした。

### Documentation and internals / 文書・内部
- (EN) New bilingual guides: EspBle in depth (`docs/GUIDE_ADVANCED.md` — callback
  tasks, capacities, backpressure, reconnection, dual-host internals, footprint,
  failure signatures), migration from `BLEDevice`, NimBLE-Arduino and
  `BluetoothSerial` (`docs/GUIDE_MIGRATION.md`), and writing HID Report
  Descriptors (`docs/GUIDE_HID_DESCRIPTORS.md`). The API design rules now exist
  in English as well (`docs/API_DESIGN.md`).
- (JA) ガイドを日英で追加した: EspBleを深く使う（`docs/GUIDE_ADVANCED.ja.md`——callbackの
  実行task、固定容量、backpressure、再接続、dual-host内部、size、不具合の見取り図）、
  他のライブラリからの移行（`docs/GUIDE_MIGRATION.ja.md`）、HID Report Descriptorの書き方
  （`docs/GUIDE_HID_DESCRIPTORS.ja.md`）。API設計規則の英語版（`docs/API_DESIGN.md`）も
  用意した。
- (EN) Internals: the HCI layer is split into three dependency tiers checked by a
  host test; the router, command scheduler and controller policy run randomized
  fault injection under sanitizers at full line coverage; and the Classic host
  archive is generated reproducibly (pinned ESP-IDF v5.5.5 / GCC 14.2.0) with a
  machine-readable provenance manifest, complete third-party notices and
  automated integrity/final-link gates.
- (JA) 内部: HCI層を依存の強さで3段へ分けhost testで検査する。router・command scheduler・
  controller policyはsanitizer下のrandomized fault injectionで行カバレッジ100%。Classic host
  archiveは再現可能に生成し（ESP-IDF v5.5.5 / GCC 14.2.0固定）、機械可読な来歴manifest、
  第三者ライセンス全文、integrity / final-linkの自動gateを持つ。

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
