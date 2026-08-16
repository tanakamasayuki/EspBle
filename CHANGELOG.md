# Changelog / 変更履歴

## Unreleased

- (EN) No breaking change: every public declaration of 1.2.0 is still present with
  the same signature, so sketches written against 1.2.0 compile unchanged. The BLE
  binaries of targets other than the original ESP32 are unaffected — Classic and the
  bundled Classic host exist on the original ESP32 alone, and using
  `EspBleClassic` is what pulls them in.
- (JA) 破壊的変更なし。1.2.0のpublicな宣言はすべて同じsignatureで残っているので、1.2.0向けに
  書いたsketchはそのままcompileできる。無印ESP32以外のtargetのBLE生成物も変わらない——Classicと
  同梱Classic hostは無印ESP32にしか存在せず、`EspBleClassic`を使うかどうかで取り込みが決まる。
- (EN) Documented the intentional mixed distribution (vendored NimBLE source plus
  a precompiled Classic-only Bluedroid host), limited the Classic compatibility
  contract to Arduino-ESP32 3.3.11, and added the archive's machine-readable
  provenance, complete third-party notices, and automated integrity/final-link gate.
- (JA) NimBLE sourceとprecompiled Classic-only Bluedroid hostを組み合わせる意図的なmixed
  distribution、ClassicのArduino-ESP32 3.3.11限定契約を明記した。archiveの機械可読な来歴、
  第三者ライセンス全文、integrity / final-linkの自動gateも追加した。
- (EN) Widened the original ESP32's supported core range from 3.3.11 alone to
  Arduino-ESP32 3.2.0 and newer, measured rather than assumed. The Bluedroid API
  headers the precompiled Classic host was built against now ship in
  `src/esp32/include/` and the Classic sources include those instead of the
  core's copies, so the compile-time contract no longer drifts with the core
  version; a small compat source supplies IDF 5.5's `esp_log` on IDF 5.4 cores.
  3.2.0–3.3.11 all compile and link, and 3.2.1 / 3.3.0 / 3.3.10 / 3.3.11 are
  hardware-verified. Arduino-ESP32 3.3.7 and 3.3.8 use a transitional
  BT-memory-claim scheme (`esp32-hal-bt-mem.h`); EspBle now participates in it,
  where previously the whole controller memory was released before `setup()` and
  neither BLE nor Classic could start on those two versions. HFP audio (SCO)
  needs 3.3.8 or newer, because up to 3.3.7 the prebuilt controller is built
  with the PCM audio path — outside what a host library can change. Below 3.2.0
  stops at an `#error` that says so; newer, unverified cores get a `#warning`.
  Other SoCs are unaffected.
- (JA) 無印ESP32の対応Coreを3.3.11のみからArduino-ESP32 3.2.0以上へ広げた（仮定ではなく実測）。
  precompiled Classic hostのbuildに使ったBluedroid API headerを`src/esp32/include/`へ同梱し、
  Classic sourceがCore側の同名headerではなくこちらを読むようにしたため、compile時の契約が
  Coreの版でずれなくなった。IDF 5.4系のcoreにはIDF 5.5の`esp_log`が無いので、小さな互換sourceで
  補う。3.2.0〜3.3.11のcompile / linkが全て通り、3.2.1 / 3.3.0 / 3.3.10 / 3.3.11は実機確認済み。
  Arduino-ESP32 3.3.7 / 3.3.8はBTメモリ保持が過渡期の方式（`esp32-hal-bt-mem.h`）で、
  従来はEspBleがこれに参加せずcontrollerメモリが`setup()`前に解放され、この2版では
  BLEもClassicも起動できなかった。今回から参加する。HFP audio（SCO）だけは3.3.8以上が
  必要で、3.3.7以前のprebuilt controllerがPCM audio pathでbuildされているためであり、
  host library側では変更できない。3.2.0未満は理由を書いた`#error`で止まり、未検証の
  新しいcoreは`#warning`を出す。他のSoCへの影響は無い。

### Bluetooth Classic on the original ESP32 / 無印ESP32のBluetooth Classic

- (EN) Added Bluetooth Classic (BR/EDR) through `EspBleClassic`, backed by a
  separately built, namespaced Classic-only Bluedroid host that ships as an
  archive. It needs no compiler flag — using the class is what selects it — and it
  exists on the original ESP32 alone, the only Arduino-ESP32 target with a BR/EDR
  radio. The project does not promise support or compatibility for this host;
  instead each feature's state (hardware-verified, unverified,
  unimplemented) is written down in `docs/CLASSIC_FEATURE_INVENTORY.ja.md`.
  Which radio to choose is covered in `docs/CLASSIC_VS_BLE.md`, and the concepts
  in `docs/GUIDE_CLASSIC_BASICS.md`.
- (JA) `EspBleClassic`でBluetooth Classic（BR/EDR）を利用できるようにした。独自build・
  名前空間化したClassic-only Bluedroid hostをarchiveとして同梱している。compiler flagは
  不要で、このclassを使うかどうかだけで決まる。BR/EDR無線を持つArduino-ESP32のtargetは
  無印ESP32だけなので、対象もそのchipに限られる。このhostのサポートや互換性は保証せず、
  代わりに機能ごとの状態（実機検証済み / 未検証 / 未実装）を
  `docs/CLASSIC_FEATURE_INVENTORY.ja.md`へ明記する。どちらの無線を選ぶかは
  `docs/CLASSIC_VS_BLE.ja.md`、概念は`docs/GUIDE_CLASSIC_BASICS.ja.md`にある。
- (EN) Discovery, identity and pairing: `inquiry()` reports address, name, Class of
  Device and RSSI, and answers `requestServices()` / `requestName()` for a peer
  known only by address — an inquiry result may carry no name, and the service list
  is what tells a sketch which channel to dial. Neither is answered during a scan,
  because both need the radio. The sketch owns `classOfDevice` and `visibility`
  (hidden, connectable, connectable plus discoverable) rather than whichever
  profile happened to start last. Secure Simple Pairing involves the application:
  numeric comparison and passkey requests arrive as events, an unanswered request
  is refused at its timeout, bonds can be listed and removed, and legacy PIN
  pairing is refused instead of answered with a guessable key. The radio itself
  takes `setTxPower()` (a range, because BR/EDR power control picks a level per
  packet, or a single value), `setPageTimeout()` and
  `setMinimumEncryptionKeySize()`. New example `Classic/RadioSettings`.
- (JA) 探索・素性・pairing: `inquiry()`はaddress、name、Class of Device、RSSIを返し、
  addressだけ分かっている相手へ`requestServices()` / `requestName()`で問い合わせられる
  ——inquiry結果に名前が入らないこともあり、どのchannelへ接続するかはservice一覧が決める。
  どちらもscan中は応答しない（両方が無線を使う）。`classOfDevice`と`visibility`
  （非表示 / 接続可能 / 接続可能かつ発見可能）はsketchが所有する。従来は最後にstartした
  profileが決めていた。Secure Simple Pairingはapplicationが関与し、numeric comparisonと
  passkey要求はeventで届き、無応答の要求はtimeoutで拒否、bondは一覧・削除でき、legacy PIN
  pairingは推測可能な鍵で答えるのではなく拒否する。無線側は`setTxPower()`（範囲——BR/EDRの
  電力制御はpacketごとにlevelを選ぶため——または単一値）、`setPageTimeout()`、
  `setMinimumEncryptionKeySize()`を受ける。example `Classic/RadioSettings`を追加した。
- (EN) SPP: a device publishes up to four services, each `startServer()` taking its
  own RFCOMM channel reported through `onServerStarted()`, and a client dials a
  named one with `connectToChannel()` — discovery returns every channel a peer
  publishes and cannot say which is meant. `stopServer()` stops all of them; there
  is no per-channel stop, because starting the ones still wanted covers it. The
  stream is binary-safe: `0x00` terminates nothing. `EspBleClassicSppStream`
  presents a session as an Arduino `Stream`, so `print()`, `readStringUntil()` and
  `parseInt()` work over Bluetooth. Two differences from `Serial` are documented
  rather than hidden: a write becomes one SPP packet, so lines are cheaper than
  characters, and the outgoing queue is finite, so a write with no room waits up to
  `setWriteTimeout()` and reports how much it took. `esp_spp_vfs_register()` stays
  unused — the same need is covered without a file-descriptor path. New examples
  `Classic/SppClient` and `Classic/SppStream`.
- (JA) SPP: 1台で最大4 serviceを公開でき、`startServer()`ごとに自分のRFCOMM channelを得て
  `onServerStarted()`が渡す。client側は`connectToChannel()`でchannelを指定する——discoveryは
  相手が公開する全channelを返すだけで、どれを指すかを示せないためである。`stopServer()`は
  全停止のみとした。残したいserviceをstartし直せば足りるためchannel単位の停止は持たない。
  byte streamはbinary safeで、`0x00`は終端にならない。`EspBleClassicSppStream`はsessionを
  Arduinoの`Stream`として見せるので、`print()`や`readStringUntil()`、`parseInt()`が
  Bluetooth越しに使える。`Serial`との違いは隠さず書いた——write 1回が1 SPP packetになるため
  文字単位より行単位が安く、送信queueは有限なので空きが無いwriteは`setWriteTimeout()`まで
  待って書けた分を返す。`esp_spp_vfs_register()`は使わない。file descriptor経路を増やさずに
  同じ用途を満たせるためである。example `Classic/SppClient`と`Classic/SppStream`を追加した。
- (EN) HID: the device side has the same API shape as BLE — `hidKeyboard()`,
  `hidMouse()`, `hidConsumerControl()`, `hidSystemControl()`, `hidGamepad()` with
  the same names, signatures and keyboard-layout handling — and the host side
  parses the Report Descriptor it receives over SDP, so a sketch written against
  BLE HID host events can consume Classic ones unchanged. Descriptors and report
  packing live in one module shared by both transports so they cannot drift, with
  the byte-for-byte output pinned by a host test. The control channel is answered:
  `onReportRequested()` / `respondToReportRequest()` / `refuseReportRequest()` for
  Get_Report, a HID handshake for Set_Report (without it the Host's request never
  completes), the Host-selected protocol mode, virtual cable unplug, and the host
  counterparts that each report through their own callback because each is a round
  trip. A device that would not fit its SDP record is refused at `begin()` with
  `ResourceExhausted`: the descriptor and the three profile strings share a
  300-byte pad with the standard attributes, leaving 214 bytes, and the backend
  logs the overflow but still reports success — it used to come up as a device no
  Host can find. With the default strings keyboard + mouse + consumer fits and
  adding the gamepad does not, so `Classic/HidComposite` is those three and
  `Classic/HidGamepad` pairs the gamepad with a keyboard. BLE has no such limit.
- (JA) HID: device側はBLEと同じAPI形状で、`hidKeyboard()`、`hidMouse()`、
  `hidConsumerControl()`、`hidSystemControl()`、`hidGamepad()`の名前・signature・keyboard
  layoutの扱いを揃えた。host側はSDPで受け取ったReport Descriptorを解析するので、BLEのHID
  host eventを使っていたsketchはそのまま移せる。descriptorとreport packingは両transportで
  同じmoduleを共有し、生成byte列をhost testで固定している。制御チャネルには応答する
  ——Get_Reportは`onReportRequested()` / `respondToReportRequest()` /
  `refuseReportRequest()`、Set_ReportはHID handshake（返さないとHost側の要求が完了しない）、
  Hostが選んだprotocol mode、virtual cable unplug、そしてhost側の対応API（いずれも往復なので
  結果はそれぞれのcallbackへ届く）である。SDP recordに収まらない構成は`begin()`が
  `ResourceExhausted`で拒否する。descriptorと3つの文字列は標準属性と共有する300 byteのpadの
  うち214 byteに収まる必要があり、backendは溢れをlogに出すだけで登録は成功として返すため、
  以前はHostから見えないdeviceとして起動していた。既定の文字列ではkeyboard + mouse +
  consumerが上限でgamepadは加えられないため、`Classic/HidComposite`はその3つ、
  `Classic/HidGamepad`はgamepadとkeyboardにした。BLEにこの制限は無い。
- (EN) Audio: A2DP Sink and Source carry already-encoded media, with delay
  reporting (`setDelay()` / `requestDelay()` / `onSinkDelay()`) because a Source
  rendering video needs to hold pictures back by however long the Sink takes.
  AVRCP covers Controller and Target in one `avrcp()` — passthrough keys, absolute
  volume, notification registration and responses, player settings — and the
  bundled host lets a Target declare volume changes only, which
  `supportedNotifications()` reports and declaring anything else refuses with a
  reason. HFP has both a Client and an Audio Gateway with a deliberately
  single-call model, selectable CVSD or mSBC raw SCO transport, bad-frame
  reporting, packet statistics, and process-wide exclusion between the two roles.
  The Client can also ask about and describe itself: `queryOperatorName()`,
  `requestSubscriberNumber()`, `dialMemory()`, `requestLastVoiceTagNumber()`,
  `disableNoiseReduction()`, `enableAppleExtensions()` and `reportBatteryLevel()`,
  the last being what most accessories need and only accepted after the Apple
  extensions are enabled. A memory dial reaches the Audio Gateway as `DialMemory`
  rather than `Dial`, because dialling a memory position as digits calls the wrong
  party. `setInBandRingTone()` says which side makes the ring sound, and an
  accessory told the wrong thing either rings twice or waits for audio that never
  comes. EspBle stops at encoded media and raw SCO payloads; codecs, PCM and
  device I/O belong to a separate library. New examples `Classic/A2dpSource` and
  `Classic/AvrcpController`.
- (JA) 音声: A2DP Sink / Sourceはencode済みmediaを運び、delay reporting
  （`setDelay()` / `requestDelay()` / `onSinkDelay()`）を持つ——映像を出すSourceは、Sinkが
  再生までに要する時間だけ絵を遅らせる必要があるためである。AVRCPはControllerとTargetを
  1つの`avrcp()`が持ち、passthrough、absolute volume、notificationの登録と応答、player
  settingを扱う。同梱hostがTargetに許すのはvolume changeだけで、`supportedNotifications()`が
  それを返し、許可外の宣言は理由を添えて拒否する。HFPはClientとAudio Gatewayの両方を持ち、
  意図的に単一call model、CVSD / mSBCを選べるraw SCO transport、bad-frame、packet統計、
  process-wideなrole排他を備える。Clientは相手への問い合わせと自分の申告も行える
  ——`queryOperatorName()`、`requestSubscriberNumber()`、`dialMemory()`、
  `requestLastVoiceTagNumber()`、`disableNoiseReduction()`、`enableAppleExtensions()`、
  `reportBatteryLevel()`である。多くの機器が必要とするのは最後の電池残量通知で、Apple拡張を
  有効にした後でなければ受け付けられない。memory dialはAudio Gatewayへ`Dial`ではなく
  `DialMemory`として届く。memoryの位置を桁として掛けると別の相手に繋がるためである。
  `setInBandRingTone()`は呼出音を鳴らす側を伝える。誤って伝えると二重に鳴るか、来ない
  呼出音を待ち続ける。EspBleが扱うのはencode済みmediaとraw SCO payloadまでで、codec・PCM・
  device I/Oは別libraryの担当である。example `Classic/A2dpSource`と
  `Classic/AvrcpController`を追加した。
- (EN) Deliberately not implemented, with the reason recorded in the feature
  inventory: call waiting and three-way calling (CHLD, BTRH), because this
  library's Audio Gateway has a single-call model and there is nothing here to
  verify them against; connected RSSI, QoS, AFH and ACL packet types, because the
  backend's connected RSSI is a delta from the golden receive range rather than
  dBm and inquiry results already carry a real one; EIR composition; SPP over VFS;
  AVRCP Target metadata and play-status transmission, for which the backend's
  public API has no means; and more than one simultaneous HID Host device, which
  would change existing signatures to take a per-device id.
- (JA) 意図して実装しないもの（理由は棚卸しに記録）: 通話待ち・三者通話（CHLD、BTRH）は
  このlibraryのAudio Gatewayが単一call modelで検証相手が無いため。接続後のRSSI・QoS・AFH・
  ACL packet typeは、backendの接続後RSSIがgolden receive rangeとの差分でdBmではなく、実際の
  値はinquiry結果が既に持つため。EIRの構成、SPPのVFS経路も同様に見送った。AVRCP Targetの
  metadata / play status送信はbackendの公開APIに手段が無い。HID Hostの複数device同時接続は、
  既存signatureがdevice単位のidを取る形へ変わるため次回以降とした。

### BLE and Classic at the same time (experimental) / BLEとClassicの同時利用（実験）

- (EN) On the original ESP32, starting both `EspBle` and `EspBleClassic` makes an
  HCI broker route between them; starting one leaves it a pass-through. There is no
  build flag — the hosts a sketch starts decide it. The broker owns the command
  FIFO and credits, merges both hosts' event masks, routes command responses and
  ACL handles, owns controller lifecycle so either host may stop first, virtualizes
  HCI Reset and flow-control setup for a reattaching Classic host, and owns
  controller-to-host ACL flow control, which neither host can do on a shared
  controller (Bluedroid credits only its own traffic and the bundled NimBLE credits
  none, which drained the controller's buffers and stalled both transports).
  Observed commands are classified by transport and scope, and unknown or
  wrong-host commands fail closed in this mode only. Hardware coverage includes
  encrypted GATT alongside Classic HID, host-based RPA with bonding, both shutdown
  orders, restart, command contention, full-FIFO recovery, HFP mSBC SCO, A2DP media
  with AVRCP control, and a multi-hour soak. It stays experimental: stop one host
  to fall back to a single-host path. Outgoing ACL buffers are not apportioned
  between the hosts, so neither can account for the other's traffic.
- (JA) 無印ESP32では`EspBle`と`EspBleClassic`の両方を開始するとHCI brokerが両者をroutingし、
  片方だけならpass-throughのままになる。build flagは無く、sketchが開始したhostで決まる。
  brokerはcommand FIFOとcredit、両hostのevent mask union、command応答とACL handleのrouting、
  どちらのhostから停止してもよいcontroller lifecycle、再attachするClassic hostへのHCI Reset・
  flow control設定の仮想完了、そしてcontroller-to-host ACL flow controlを所有する。最後は
  共有controllerではどちらのhostも実行できない（Bluedroidは自分宛ての分しかcreditせず、同梱
  NimBLEは一切返さないため、controllerのbufferが枯渇して双方の通信が停止していた）。観測
  commandはtransportとscopeで分類し、未知／別host commandはこのmodeでだけfail-closedにする。
  実機では暗号化GATTとClassic HIDの併用、host-based RPAとbond、任意の停止順、再起動、command
  競合、FIFO満杯からの復帰、HFP mSBC SCO、A2DP mediaとAVRCP操作、数時間級soakまで確認した。
  実験扱いのままで、一方を停止すれば単一host経路へ戻る。送信ACL bufferは2つのhost間で按分して
  いないため、各hostは相手の送信を勘定できない。

### BLE

- (EN) Fixed a crash in the bundled NimBLE port for the original ESP32: removing an
  event from an event queue sampled the queue depth under a function-local
  spinlock, so a concurrent dequeue by the host task on the other core made the
  following receive fail and abort the firmware. Hardware runs hit it through GATT
  discovery issued from EspBle's operation task. Removal is best effort now and
  stops when the queue drains early.
- (JA) 無印ESP32向け同梱NimBLE portのcrashを修正した。event queueからのevent削除がqueue長を
  function-localなspinlock下で読んでいたため、別coreのhost taskが先にdequeueすると続く受信が
  失敗してfirmwareがabortしていた。実機ではEspBleのoperation taskから出すGATT discoveryで
  発生した。削除をbest effortにし、queueが先に空になったら中断する。
- (EN) Added the BLE examples for features that had none: `Hid/Gamepad`
  (`hidGamepad()` was never shown), `Gap/MultiConnection` (several simultaneous
  connections, where `AutoReconnectClient` only covers one) and
  `Hosted/WifiCoexistence` (Wi-Fi and BLE sharing one transport on P4/C6, and
  `EspBle::end()` leaving Wi-Fi up).
- (JA) exampleが無かったBLE機能へexampleを追加した。`Hid/Gamepad`（`hidGamepad()`はどの
  exampleからも呼ばれていなかった）、`Gap/MultiConnection`（複数同時接続。
  `AutoReconnectClient`は1接続のみ）、`Hosted/WifiCoexistence`（P4/C6でWi-FiとBLEが同一
  transportを共有し、`EspBle::end()`がWi-Fiを残すこと）である。

### Fixes / 修正

- (EN) Classic pairing settled on Just Works whatever IO capability was configured,
  because the SPP service asked for no security and Secure Simple Pairing only
  involves the application when a service demands it; the service requirement now
  follows the configured security. A failed pairing left the SPP connection attempt
  in flight until its own timeout with no notification; it ends with the pairing
  now, and the caller is told. The Classic HID Host discarded every input report
  from a device that uses report IDs, because the transport delivers the report with
  its ID in front while the descriptor's field offsets are relative to the payload.
  Dual-host pairing from scratch timed out at the HCI layer, because the broker
  rejected the link key and Secure Simple Pairing replies as unclassified — bonded
  peers worked, and no test had started from an empty bond list; two commands that
  name a connection handle (Change Connection Packet Type, Read Clock Offset) are
  checked against handle ownership rather than treated as address-scoped. An Audio
  Gateway answering an AT command the backend does not decode left the exchange
  open, because the backend sends the response line without the terminating OK and
  the peer then waits with its next command still queued; `respondToUnknownAt()`
  finishes the exchange, with OK for a string and an error for null.
- (JA) Classicのpairingは、IO capabilityを何に設定してもJust Worksになっていた——SPP service
  がsecurityを要求せず、Secure Simple Pairingはserviceが要求したときだけapplicationを介する
  ため。service側の要求水準を設定したsecurityから導くようにした。pairing失敗時にSPPの接続試行
  が自前のtimeoutまで宙吊りになり通知も無かったので、pairingの失敗で試行を終わらせ呼び出し側へ
  通知する。Classic HID Hostはreport IDを使うdeviceからのInput Reportをすべて捨てていた
  ——transportはreport IDを先頭に付けて渡すが、descriptorのfield offsetはpayload起点であるため。
  dual-hostの初回pairingはHCI層でtimeoutしていた——brokerがlink key応答とSecure Simple Pairing
  応答を未分類として拒否していたためで、bond済みの相手とは通信できており、bondを空にした状態
  から始めるtestが無かった。あわせてconnection handleを指す2つのcommand（Change Connection
  Packet Type、Read Clock Offset）をaddress scope扱いから外し、handle所有権で検査する。Audio
  Gatewayがbackendの解釈しないAT commandへ応答すると交換が閉じないままだった——backendは応答行
  を送るが終端のOKを送らず、相手は次のcommandをqueueに抱えたまま待ち続ける。
  `respondToUnknownAt()`が交換を閉じるようにした（文字列ならOK、nullptrならerror）。
- (EN) Fixed two tool defects: the example compile workflow built every sketch with
  the `esp32s3` profile, so the original-ESP32-only Classic examples always failed,
  and the compatibility-matrix tool used wrong representative GATT paths and
  skipped a requested example that no longer existed instead of failing, which hid
  coverage gaps after example moves.
- (JA) tool側の不具合2件を修正した。example compile workflowが全sketchを`esp32s3` profileで
  buildしていたため無印ESP32専用のClassic exampleが必ず失敗していたので、各sketchが持つ
  profileでbuildする。compatibility matrix toolは代表GATT pathが誤っていたので直し、指定example
  が存在しないときは黙って飛ばさずerror終了にした。example移動後に検証が欠落するのを防ぐ。

### Examples, documentation and internals / example・文書・内部

- (EN) Classic HID now has the same example set as BLE HID — gamepad, mouse, media
  keys, NKRO and a composite device — because an implemented feature with no
  example reads as a missing one. `Classic/HidGamepad` is the case BLE cannot
  replace: older consoles accept BR/EDR HID only. Every Classic example has a
  README in both languages saying which radio reaches which peers, and the Classic
  beginner guide and the BLE-or-Classic comparison exist in both languages. Two new
  guides go past the basics, also in both languages: **EspBle in depth**
  (`docs/GUIDE_ADVANCED.md`) documents which task each callback runs on and the four
  that cannot wait for `update()`, every fixed capacity and what overflowing it
  does, the accepted-versus-applied contract, backpressure, reconnection and
  identity, the dual-host broker's internals and diagnostics, how to measure
  footprint, and a playbook of known failure signatures; **Coming to EspBle from
  another library** (`docs/GUIDE_MIGRATION.md`) maps `BLEDevice`, NimBLE-Arduino and
  `BluetoothSerial` onto EspBle call by call. **Writing a HID Report Descriptor**
  (`docs/GUIDE_HID_DESCRIPTORS.md`) covers the three routes to a descriptor, how
  fields pack, where the report ID sits on each transport, the Classic SDP budget
  and how to verify a descriptor rather than reason about it. The API design rules
  now exist in English as well (`docs/API_DESIGN.md`), and every example README
  links to the guide sections that explain it.
- (JA) Classic HIDのexampleをBLE HIDと同じ一覧にした——gamepad、mouse、メディアキー、NKRO、
  複合device。exampleが無い機能は無い機能として読まれるためである。`Classic/HidGamepad`はBLE
  で代替できない用途で、旧世代のゲーム機はBR/EDR HIDしか受け付けない。Classic exampleは全数が
  両言語のREADMEを持ち、「どちらの無線がどの相手に届くか」を書いた。Classicの入門ガイドと
  BLE / Classicの選び方も日英とも用意した。入門の先を扱うガイドも日英で2本追加した。
  **EspBleを深く使う**（`docs/GUIDE_ADVANCED.ja.md`）は、callbackがどのtaskで動くかと
  `update()`を待てない4つ、固定容量の一覧と満杯時の挙動、「受理」と「反映」の契約、
  backpressure、再接続と素性、dual-host brokerの内部と診断、sizeの測り方、既知の不具合の
  見取り図を書いた。**他のライブラリからEspBleへ**（`docs/GUIDE_MIGRATION.ja.md`）は
  `BLEDevice`系・NimBLE-Arduino・`BluetoothSerial`との対応を1行ずつ示した。
  **HID Report Descriptorを書く**（`docs/GUIDE_HID_DESCRIPTORS.ja.md`）はdescriptorへ至る
  3経路、fieldの詰まり方、transportごとのreport IDの位置、ClassicのSDP予算、そして頭の中で
  済ませずに確かめる方法を書いた。API設計規則の英語版（`docs/API_DESIGN.md`）も用意し、
  各example READMEから該当するガイドの章へリンクした。
- (EN) Internals: the HCI layer is split into three dependency tiers checked by a
  host test (routing depends on the C library alone, the broker stops at ESP-IDF,
  only the integration layer reads Arduino headers); the router, command scheduler
  and controller policy are covered by randomized fault injection under
  AddressSanitizer and UndefinedBehaviorSanitizer, reaching full line coverage; and
  the Classic host archive is generated reproducibly, pinned to ESP-IDF v5.5.5 and
  xtensa-esp32 GCC 14.2.0, with link checks, global-symbol namespacing,
  required-symbol validation and SHA-256 reporting.
- (JA) 内部: HCI層を依存の強さで3段へ分け、host testでincludeを検査する（routingはC標準
  ライブラリのみ、brokerはESP-IDFまで、Arduinoのheaderを見るのは統合層だけ）。router・command
  scheduler・controller policyはAddressSanitizer / UndefinedBehaviorSanitizer下のrandomized
  fault injectionで行カバレッジ100%に到達する。Classic host archiveはESP-IDF v5.5.5 /
  xtensa-esp32 GCC 14.2.0へ固定して再現可能に生成し、link check、global symbolの名前空間化、
  必須symbol検査、SHA-256表示を行う。

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
