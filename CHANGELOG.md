# Changelog / 変更履歴

## Unreleased

- (EN) A Classic peer can be asked what it offers and what it is called, given
  only its address. `inquiry().requestServices()` returns the peer's service
  UUIDs through `onRemoteServices()`, and `inquiry().requestName()` fetches the
  name through `onRemoteName()` — an inquiry result may carry no name at all, and
  the service list is what tells a sketch which channel to connect to now that a
  device can publish several. Neither is answered while a scan is running,
  because both need the radio; that is stated where the calls are declared.
- (JA) addressだけ分かっているClassicの相手に「何を提供しているか」「何と名乗るか」を
  問い合わせられるようにした。`inquiry().requestServices()`は相手のservice UUIDを
  `onRemoteServices()`へ、`inquiry().requestName()`は名前を`onRemoteName()`へ返す。
  inquiry結果には名前が入らないこともあり、deviceが複数serviceを公開できるようになった今、
  service一覧はどのchannelへ接続するかを決める材料になる。どちらもscan実行中は応答が来ない
  ——両方が無線を使うためで、宣言箇所に明記した。

- (EN) A Classic device can publish several SPP services. `startServer()` may be
  called repeatedly, up to four services, each getting its own RFCOMM channel;
  `onServerStarted()` reports which channel a service received, and
  `serverCount()` / `server(index)` list them. `connectToChannel()` dials a named
  channel, which is what a client needs once a peer offers more than one service
  — discovery returns every channel it publishes and cannot say which one is
  meant. Stopping is `stopServer()`, which takes down all of them; there is no
  per-channel stop, because starting the ones still wanted covers it without a
  second way to undo `startServer()`.
- (JA) ClassicのdeviceがSPP serviceを複数公開できるようになった。`startServer()`は
  繰り返し呼べ、最大4 serviceがそれぞれ自分のRFCOMM channelを得る。どのchannelになったかは
  `onServerStarted()`が渡し、`serverCount()` / `server(index)`で列挙できる。
  `connectToChannel()`はchannelを指定して接続する——相手が複数serviceを公開すると、
  discoveryは全channelを返すだけでどれを指すかを示せないため、clientにはこれが必要になる。
  停止は`stopServer()`で全停止のみとした。channel単位の停止は持たない。残したいserviceを
  start仕直せば足り、`startServer()`を取り消す手段を2つに増やさないためである。

- (EN) The Classic HID control channel is answered now. A Host that asks for a
  report with Get_Report gets one: `onReportRequested()` delivers the request and
  `respondToReportRequest()` answers it with the type and report ID the Host
  used, or `refuseReportRequest()` declines. A Set_Report is acknowledged with a
  HID handshake, which the library sends unless the sketch refuses the report —
  without it the Host's own request never completes. `onSetReport()` keeps the
  type the Host used, so a Feature report is no longer reported as an Output
  report. The device also exposes the protocol mode the Host selected
  (`protocolMode()`, `onProtocolMode()`) and can unplug its virtual cable. On the
  host side, `requestReport()`, `sendReport()`, `requestProtocolMode()`,
  `setProtocolMode()`, `requestIdleRate()`, `setIdleRate()` and
  `virtualCableUnplug()` drive the same channel, each reporting its result
  through its own callback because every one of them is a round trip.
- (JA) ClassicのHID制御チャネルに応答するようにした。HostがGet_Reportで問い合わせたら
  報告する——`onReportRequested()`が要求を配送し、`respondToReportRequest()`がHostの使った
  typeとreport IDで答える。`refuseReportRequest()`で断ることもできる。Set_Reportへは
  HID handshakeを返す。sketchが拒否しない限りlibraryが送る——返さないとHost側の要求が
  完了しない。`onSetReport()`はHostの使ったtypeを保つので、Feature reportがOutput reportとして
  届くことはなくなった。deviceはHostが選んだprotocol mode（`protocolMode()`、
  `onProtocolMode()`）を公開し、virtual cable unplugも行える。host側は`requestReport()`、
  `sendReport()`、`requestProtocolMode()`、`setProtocolMode()`、`requestIdleRate()`、
  `setIdleRate()`、`virtualCableUnplug()`で同じチャネルを操作する。いずれも往復なので、
  結果はそれぞれのcallbackへ届く。

- (EN) Classic devices can now say what they are and control whether they can be
  found. `EspBleClassicConfig::classOfDevice` and `setClassOfDevice()` set the
  Class of Device — the value a Host uses to pick an icon and, on some Hosts, to
  decide whether it offers to connect at all — and `visibility` /
  `setVisibility()` choose between hidden, connectable and
  connectable-plus-discoverable. Both were previously decided by whichever
  profile happened to start last, because each profile set the scan mode itself;
  the sketch owns them now and profiles re-assert them. Getting the class onto
  the air needs three things the backend does not do on its own: re-applying it
  after a profile's service registration overwrites it, writing it before the
  scan mode because the controller takes it in when inquiry scan is enabled, and
  toggling discoverability for a change made while running. The library handles
  all three, so `setClassOfDevice()` reports acceptance and the class becomes
  live shortly after.
- (JA) Classic機器が「自分が何であるか」を示し、見つけられるかどうかを制御できるように
  なった。`EspBleClassicConfig::classOfDevice`と`setClassOfDevice()`でClass of Deviceを
  設定する——Hostがiconを選び、機種によっては接続を提案するかどうかを決める値である。
  `visibility` / `setVisibility()`で非表示・接続可能・接続可能かつ発見可能を選ぶ。
  従来はprofileが各自scan modeを設定していたため、最後にstartしたprofileがどちらも決めていた。
  所有者をsketch側へ移し、profileは再適用だけを行う。classを電波へ乗せるにはbackendが
  自動でやらない3点が必要で、profileのservice登録による上書き後の再適用、controllerが
  inquiry scan有効化時に取り込むためscan modeより先に書くこと、実行時変更では
  discoverabilityを切り替えること、をlibraryが引き受ける。したがって
  `setClassOfDevice()`は受理を返し、classはその直後に有効になる。

- (EN) Fixed dual-host pairing from scratch. The broker rejected the link key
  and Secure Simple Pairing replies as unclassified, so the first pairing with
  a peer timed out at the HCI layer while a peer that was already bonded worked
  — the tests had never started from an empty bond list, which is why this
  survived. The replies are classified now, the dual-host test drops both
  bonds before connecting so pairing is always exercised, and two commands
  that name a connection handle (Change Connection Packet Type, Read Clock
  Offset) are checked against handle ownership instead of being treated as
  address-scoped.
- (JA) dual-hostでの初回pairingを修正した。brokerがlink key応答とSecure Simple
  Pairing応答を未分類として拒否するため、bond済みのpeerとは通信できるのに、
  初めてpairingする相手ではHCI層でtimeoutしていた。testがbondを空にした状態から
  始めたことが無く、それで見逃されていた。これらの応答を分類し、dual-host testは
  接続前に両側のbondを削除してpairingを必ず通すようにした。あわせてconnection handleを
  指す2つのcommand（Change Connection Packet Type、Read Clock Offset）を
  address scope扱いから外し、handle所有権の検査対象にした。
- (EN) Fixed three Classic defects that hardware testing exposed. Classic
  pairing settled on Just Works no matter which IO capability was configured,
  because the SPP service asked for no security and Secure Simple Pairing only
  involves the application when a service demands it; the service requirement
  now follows the configured security, so a configured IO capability actually
  reaches the peer. A failed pairing left the SPP connection attempt in flight
  until its own timeout, with no notification and no way to retry in between;
  the attempt now ends when the pairing fails, and the caller is told. The
  Classic HID Host discarded every input report from a device that uses report
  IDs: the transport delivers the report with its ID in front, while the
  descriptor's field offsets are relative to the payload.
- (JA) 実機テストで見つかったClassicの不具合3件を修正した。Classicのpairingは
  IO capabilityを何に設定してもJust Worksになっていた——SPP serviceがsecurityを
  要求しておらず、Secure Simple Pairingはserviceが要求したときだけapplicationを
  介するため。service側の要求水準を設定したsecurityから導くようにし、設定した
  IO capabilityが実際に相手へ伝わるようにした。pairingが失敗するとSPPの接続試行が
  自前のtimeoutまで宙吊りになり、通知も無く再試行もできなかった。pairing失敗で
  試行を終わらせ、呼び出し側へ通知するようにした。Classic HID Hostはreport IDを使う
  deviceからのInput Reportをすべて捨てていた——transportはreport IDを先頭に付けて
  渡すが、descriptorのfield offsetはpayload起点であるため。

- (EN) Classic HID now uses the same API shape as BLE HID. The device side
  gained `hidKeyboard()`, `hidMouse()`, `hidConsumerControl()`,
  `hidSystemControl()` and `hidGamepad()`, with the same names, signatures and
  keyboard layout handling as their BLE counterparts; the Report Descriptor is
  composed from whichever profiles the sketch configured. The host side parses
  the Report Descriptor it receives over SDP and delivers keyboard state,
  per-usage keyboard events and mouse events, so a sketch that consumed BLE HID
  host events can consume Classic ones unchanged. Report Descriptors, report
  layouts and packing now live in one module shared by both transports, so the
  two cannot drift apart; the byte-for-byte descriptors are held in place by a
  host test. The host also gained `setKeyboardLeds()` with the BLE signature,
  which takes the report ID from the peer's descriptor instead of assuming one.
- (JA) ClassicのHIDをBLEと同じAPI形状にした。device側に`hidKeyboard()`、
  `hidMouse()`、`hidConsumerControl()`、`hidSystemControl()`、`hidGamepad()`を
  追加し、名前・signature・keyboard layoutの扱いをBLE側と揃えた。Report Descriptorは
  sketchがconfigureしたprofileだけを合成する。host側はSDPで受け取ったReport Descriptorを
  解析し、keyboardのstate、usage単位のkeyboard event、mouse eventを配送するので、
  BLEのHID host eventを使っていたsketchはそのままClassicへ移せる。Report Descriptorと
  report構造・packingは両transportで同じmoduleを共有するようにし、片側だけずれないように
  した。生成されるdescriptorのbyte列はhost testで固定している。host側にはBLEと同signatureの
  `setKeyboardLeds()`も追加した。report IDは仮定せず相手のdescriptorから取る。

- (EN) Added Classic device discovery, application-driven pairing and bond
  management on the original ESP32. `EspBleClassicInquiry` reports address,
  name (falling back to the extended inquiry response), class of device and
  RSSI. `EspBleClassicSecurityConfig` selects the IO capability, and numeric
  comparison and passkey requests are delivered to the sketch, which answers
  with `confirmNumericComparison()` or `providePasskey()`; an unanswered
  request is rejected when its timeout elapses. Bonds can be listed and
  removed. Pairing previously accepted every request automatically and replied
  to legacy PIN requests with a fixed `1234`; it now refuses legacy PIN pairing
  instead of using a guessable key.
- (JA) 無印ESP32へClassicのdevice discovery、application制御のpairing、bond管理を
  追加した。`EspBleClassicInquiry`はaddress、name（EIRへfallback）、Class of Device、
  RSSIを返す。`EspBleClassicSecurityConfig`でIO capabilityを選び、numeric comparisonと
  passkey要求はsketchへ通知して`confirmNumericComparison()` / `providePasskey()`で
  応答する。無応答の要求はtimeoutで拒否する。bondは一覧・削除できる。従来のpairingは
  全要求を自動承諾し、legacy PIN要求へ固定値`1234`を返していたが、推測可能な鍵を使う代わりに
  legacy PIN pairingを拒否するようにした。

- (EN) Split the HCI layer into three dependency tiers and made the split
  enforceable: the routing modules depend on the C library alone, the broker
  stops at ESP-IDF, and only the integration layer reads Arduino core headers.
  The bundled NimBLE port no longer includes an Arduino header to learn whether
  a Classic host shares the controller; it asks the broker, which the
  integration layer answers once at startup. A host test checks the tiers by
  their includes.
- (JA) HCI層を依存の強さで3段（platform非依存のrouting、ESP-IDFまでのbroker、
  Arduinoを見る統合層）へ分け、host testでincludeを検査して退行を防ぐようにした。
  同梱NimBLE portはClassic hostが共有controllerを使うかどうかをArduino coreのheaderで
  はなくbrokerへ問い合わせ、統合層が起動時に一度答える。

- (EN) The HCI broker now owns controller-to-host ACL flow control on the
  original ESP32 instead of disabling it. It learns the controller's buffer
  geometry from the Read Buffer Size response, configures the controller
  itself, answers both hosts' flow-control commands virtually, and returns one
  credit for every ACL packet it received. Neither host could run this loop on
  a shared controller: Bluedroid credits only its own traffic and the bundled
  NimBLE credits none, which drained the controller's host buffers and stalled
  both transports.
- (JA) 無印ESP32のHCI brokerがcontroller-to-host ACL flow controlを無効化せず
  自分で所有するようにした。`Read Buffer Size`応答からcontrollerのbuffer geometryを学習し、
  controllerの設定はbroker自身が行い、両hostのflow control commandは仮想完了で返し、
  受信したACL 1 packetにつき1 creditを返す。共有controllerではどちらのhostもこのloopを
  実行できず（Bluedroidは自分宛ての分しかcreditせず、同梱NimBLEは一切返さない）、
  controllerのhost bufferが枯渇して双方の通信が停止していた。

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
