# 無印ESP32 NimBLE / Classic共存 引き継ぎ

2026-08-11時点の実装、検証済み範囲、未完了事項、作業再開手順をまとめます。新しく作業する人は
この文書、[Classic設計・検証記録](PLAN_ESP32_CLASSIC.ja.md)、
[技術検証](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)の順に読んでください。

## 現在の結論

無印ESP32では、独自buildしたClassic-only Bluedroid hostとEspBle同梱NimBLE hostを、単一BTDM
controllerへ同時接続できます。Classic hostはcore内蔵archiveではなく、SPP、HID Device、HID Host、
A2DP Sink/Source、AVRCP CT/TG、HFP Client/Audio Gateway（いずれもexternal codec）、SMPを
有効にした名前空間化済み`libespble_bluedroid_classic.a`を使います。

host構成はsketchが`begin()`したhostだけで決まり、build flagはありません。1 hostならbrokerは
pass-through、`EspBle`と`EspBleClassic`の両方をbeginすればbrokerがHCIをroutingします。
無印ESP32以外はcore同梱NimBLE経路を変更しません。Classicは次回releaseへ含めます
（[決定台帳](DECISIONS.ja.md)のスコープ6）。dual-hostも同梱しますが、検証はEspBle同士と
core内蔵host相手までで、**外部機器との相互運用は未検証**です。不安定な場合は一方を`end()`して
単一hostで使います。

Classic-onlyは`EspBleClassic`を使うだけで独自hostを自動選択し、公開exampleに`build_opt.h`はありません。
明示flagはtest instrumentationにだけ残ります。

Classic-only A2DP Sink / Source、AVRCP CT/TG、HFP Client / Audio Gatewayの公開APIと実機転送まで完了しました。
dual-hostでもBLE GATT接続中のmSBC SCO双方向転送、A2DP encode済みmedia転送、AVRCP操作と、各link切断後のGATT継続まで確認済みです。EspBleはBluetooth profile、codec negotiation、encode済みmedia/SCO payloadの受け渡し
までを担当し、PCM処理やdevice I/Oは担当しません。詳細は
[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md)を正本とします。

## 完了済み

| 領域 | 状態 |
|---|---|
| Classic host独自build | IDF v5.5.5 / GCC 14.2.0、controller/BLE無効、SPP/HID Device/HID Host/SMP、A2DP/AVRCP、HFP Client/AG external codec有効 |
| archive clean再現 | cleanなv5.5.5 worktreeから一時生成し、格納済み`.a`とbyte単位・SHA-256一致 |
| symbol分離 | archiveのglobal defined symbolを`espble_bd_`へ名前空間化。core Bluedroidと衝突なし |
| Classic排他モード | SPP、HID Device/Host、双方向report、切断、再初期化、再接続を実機確認 |
| A2DP Sink transport | 接続、SBC設定、stream状態、raw media view、buffer解放、callback停止を公開API化。ESP32同士でexternal-codec mediaを実機受信 |
| A2DP Source transport | 固定SBC endpoint、接続、start/suspend、copy送信、MTU検査、`WouldBlock` retryを公開API化。通常回帰と20,000 packet連続転送を欠損なく完走 |
| AVRCP CT/TG | 接続、remote feature、passthrough送受信、metadata/play-status要求と応答event、absolute volumeとone-shot通知を公開API化。A2DP併用でPlayと音量変更を実機確認 |
| HFP Client | SLC、発信/応答/終了等のcall control、call/volume/AT event、CVSD/mSBC raw SCO送受信、bad-frame、packet statisticsを公開API化。ESP32 AG probeとmSBC双方向転送を確認 |
| HFP Audio Gateway | CIND/COPS/CNUM/CLCC自動応答、単一call model、application command event、CVSD/mSBC raw SCO送受信を公開API化。公開Clientとの発信・mSBC往復とClient/AG process-wide排他を確認 |
| Classic-only HFP CVSD | AGの`preferredAudioCodec`でCVSDを選択。120-byte raw SCO viewの双方向transport、audio再接続、SLC切断・再接続後の再発信を公開Client/AG間で確認 |
| dual-host HFP | BLE GATT接続中のSLC、発信、mSBC SCO双方向payload、SCO中・切断後のGATT readを確認。Voice Setting/eSCO command policy、BD_ADDR指定command、SCO handle ownershipを実装 |
| dual-host A2DP / AVRCP | BLE GATT接続中にSBC media、Play、absolute volume、stream中・切断後のGATT readを確認。ESP A2DP coexistence command `0xfc82`をClassic radio policyへ分類 |
| dual-host HCI routing | Command Complete/Status、LE/BR-EDR handle、ACL、切断、Completed Packetsをrouting |
| command scheduler | broker所有16 packet FIFO、controller credit、opcode照合、1 response command in-flight |
| controller-wide policy | General/Page 2/LE event mask union、再attach時Resetとflow-control設定の仮想完了 |
| lifecycle | controller停止責任をbrokerへ委譲。任意停止順、再attach、両destructor順、再起動に加え、SPP/HID callback解除の参照寿命barrierを確認 |
| Security / privacy | Classic接続中のBLE pairing、bond復元、暗号化GATT、host-based RPA rotationと再起動復元 |
| 負荷 | GATT/Classic ACL反復、command同時発行、FIFO満杯・超過拒否、数時間級再登録soak、heap非減少を確認 |
| 異常report / peer消失 | null・1025 byte HID reportを接続維持のまま拒否。peer突然再起動後にbond済みBLEとClassic HIDを再接続 |
| 接続・pairing失敗 | 誤passkey後にbondを残さず再pairing。HID Hostの非同期接続失敗通知後もBLEを維持し、正しいpeerへ再接続 |
| 他SoC分離 | ESP32-S3 buildでClassic archive非リンク、無印ESP32専用privacy patch非適用を確認 |

詳細な数値と失敗から得た知見は[技術検証](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)にあります。

## 重要な設計境界

- controllerを先に起動したhostが停止責任をbrokerへ委譲し、後から入るhostは再初期化せずhostだけattachする。
  両hostがlinkされたsketchはどちらの順でも起動でき、controllerはBTDMで起動する。
- 最後のlogical hostが外れたときだけbrokerがcontrollerを停止する。
- HCI commandをhostから物理VHCIへ直接流さず、broker FIFOと単一transaction ownershipを通す。
- event種別だけで配送せず、command owner、connection handle、LE Meta / BR-EDR event semanticsを使う。
- controller-to-host ACL flow controlはbrokerが所有する。2 host目の登録で有効化し、最後のhostが
  外れるまで維持する。受信したACLは経路に関係なく必ず1 creditを返す。
- test-only backpressure holdは`ESPBLE_HCI_BACKPRESSURE_TEST`時だけ存在し、通常buildには入らない。
- `.a`のglobal defined symbolだけをprefixし、platform依存のundefined symbolはArduino coreから解決する。

## 未完了・優先順位

### P0: Classic-only Audio

1. **完了:** A2DP/AVRCP/HFPをexternal codec / Voice over HCI構成でarchiveへ追加し、生成scriptのlink checkを拡張した。
2. **完了:** A2DP Sinkの接続、SBC設定、stream状態、encoded media callback、所有権と停止barrierを公開API化した。
3. **完了:** Classic-only ESP32同士で接続、codec設定、5 packet受信、suspend、切断を実機確認した。
4. **完了:** A2DP Sourceのencoded media copy送信と`WouldBlock` backpressureを公開API化し、100 packetを実機確認した。
5. **基本操作完了:** AVRCP CT/TGのpassthroughとabsolute volumeをA2DP接続上で実機確認した。公開TG APIに
   metadata / play-status応答送信がないため、Controller側応答eventは外部Targetとの相互運用確認を残す。
6. **完了:** HFP Clientのcontrol / Voice over HCI transportを実装し、mSBC/CVSDで実機確認した。
7. **基本完了:** HFP AGを公開API化し、Client/AGのruntime排他、発信、着信、応答、終了、call active、
   mSBC/CVSD往復、CVSD SCO再接続、SLC再接続後の再発信を実機確認した。外部HFP機器との相互運用は残す。
8. codec/PCM/device処理はEspBleへ入れず、`../PCMFlowBluetooth/SPEC.ja.md`を契約として
   独立libraryを並行実装する。PCMFlow coreの既存`PCMSource`/`PCMSink`を再利用する。

HFPの公開境界は[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md#hfp実装前調査)まで調査済み。
受信bufferはcallback復帰後にEspBleが解放、送信bufferはAPI成功時にBluedroidが消費する。HFP送信には
A2DPのような同期的`WouldBlock`がないため、受理結果と後続packet discard statisticsを混同しないこと。
実機ではmSBC 57-byte送信が受信側で58/60 byteのpadding付きviewになり、bad-frameも60 byteで届いた。
PCMFlowBluetoothへは`badFrame`とraw lengthを失わず渡し、decoder側で57 byte frameを切り出す。

### 完了: dual-host実験機能の信頼性

完了。失敗復旧、異常入力、peer消失、callback解除時の参照寿命を実装・監査し、clean実機回帰まで通した。

### P1: 一般対応・upstream品質

1. **完了:** brokerがincoming ACLを一元管理し、controller-to-host flow controlを
   有効にしたまま両host分のcreditを返す。geometryは`Read Buffer Size`応答から学習し、
   `Host Buffer Size`と`Set Controller To Host Flow Control`はbrokerが物理controllerへ送る。
   hostの同commandは仮想完了で握り潰し、hostの`Host Number Of Completed Packets`も消費する。
2. **完了:** HCI parser、transaction、handle table、credit分配へfuzz / fault injectionを追加した。
   sanitizer付きhost testで、3モジュールとも行カバレッジ100%と500 seed掃引を通した。
3. **完了:** routing logic（platform非依存）、broker（ESP-IDFまで）、統合層（Arduino）の3層へ
   境界を定め、同梱NimBLE portのArduino依存を`espble_hci_broker_set_classic_host_expected()`へ
   置き換えた。境界はhost testが機械的に検査する。詳細は
   [Classic設計・検証記録](PLAN_ESP32_CLASSIC.ja.md#hci-componentの境界)にある。
4. **完了: release scopeを確定した。** 次回releaseへClassicを含める。build flagは設けず、`EspBleClassic`を使うかどうかだけで決まる。exampleはBLE側と同じ範囲まで用意する。MIT OSSとして厳密なサポート保証は掲げず、機能ごとに「実機検証済み / 未検証 / 未実装」を文書で区別する。releaseまでは未実装項目を減らす作業を続ける（[決定台帳](DECISIONS.ja.md)のスコープ6、[次回リリース前タスクリスト](PLAN_RELEASE_NEXT.ja.md)のGate A）。

### P2: 配布・保守

1. NimBLE source / Classic `.a`のmixed distributionを維持し、理由と生成手順を利用者・保守者文書へ明記する。
2. Arduino-ESP32更新時のIDF/toolchain ABI matrixとarchive再生成gateをCIまたはrelease手順へ組み込む。
3. 外部Classic HID Host/Deviceとの相互運用を追加する。

### 今後の作業メモ

1. **完了: Classic機能の棚卸し。** 結果は[Classic機能の棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)にある。
   公開APIとarchiveが持つAPIを突き合わせ、公開・部分・未公開へ分類した。優先度の上位は
   inquiry / SDP照会、pairing制御、bond管理、Class of Deviceで、いずれもprofileではなくGAP層である。
   旧メモ: 現在の公開APIはSPP server、generic HID Device/Host、A2DP Sink/Source、
   AVRCP CT/TG、HFP Client/AGだが、利用者から見た機能はまだ足りない。少なくともSPP client
   （`esp_spp_connect()`は内部で使用済み、公開APIが未整備）、SPP複数session、inquiry / device discovery、
   pairing UI（PIN・SSP確認）の公開度、HID Deviceのboot protocolとHID Hostのreport出力範囲、
   AVRCP TGのmetadata / play-status応答送信を洗い出し、実装済み・部分実装・未実装へ分類してから
   優先度を決める。棚卸し結果は[Feature Matrix](FEATURE_MATRIX.ja.md)と本書へ反映する。
2. **一部完了: core内蔵Bluedroid Classicとの相互接続Peer test。** SPPを`classic_core_host_spp`として
   追加した（peerは`BluetoothSerial`だけを使いEspBleをlinkしない）。A2DP / AVRCP / HFPは同じ方式で
   追加できるが未着手。旧メモ: 外部機器が揃うまでの相互運用確認として、
   peer boardをArduino-ESP32同梱のBluedroid（EspBleの独自hostを使わない別sketch）で動かし、
   独自host ⇄ core hostのpair testを追加する。core 3.3.11のesp32 sdkconfigは
   `CONFIG_BT_SPP_ENABLED` / `CONFIG_BT_A2DP_ENABLE` / `CONFIG_BT_HFP_ENABLE`
   （Client/AG、`CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI`）が有効で、`CONFIG_BT_HID_ENABLED`は無効。
   したがってSPP、A2DP（core側はinternal codec、EspBle側はexternal codecなのでencode済みframe境界の
   検証になる）、AVRCP、HFPは相互接続できるが、Classic HIDはcore hostでは試験できない。
   独自archiveの名前空間化が正しければ同一sketchへ両hostをlinkできない前提も、この構成で確認できる。
3. **完了: ClassicのHID APIをBLEと同形にする。** device側のprofile API（keyboard / mouse /
   consumer / system / gamepad）とhost側のReport Descriptor解析・event配送を、BLEと同名・
   同signature・同event型で公開した。Report Descriptor、report構造、packingは
   `src/EspBleHidProfile.h`へ集約して両transportで共有するので、片側だけ変わることはない。
   生成されるdescriptorのbyte列はhost testで固定し、API形状の一致は
   Peer test `classic_hid_api`が両側から確認する。Get_Report / Set_Report / protocol modeは
   BLE側にも無いため、棚卸しの優先度に従って別途扱う。
4. **完了: ClassicのexampleをBLEと同じ一覧にする。** HIDはgamepad、composite、mouse、consumer、
   NKROを揃え（gamepadはBLE側にも追加した）、SPP client、SPP Stream、A2DP Source、
   AVRCP Controller、無線設定も加えた。Classic exampleは全数が日英READMEを持つ。一覧は
   [次回リリース前タスクリスト](PLAN_RELEASE_NEXT.ja.md)のGate Fを正とする。
5. **完了: Classic入門ガイドと、BLE / Classicの接続先の違いの明文化。**
   `GUIDE_CLASSIC_BASICS`（日英）と`CLASSIC_VS_BLE`（日英）を作成し、両方にある機能の差
   ——HIDの合成上限、report IDの位置、Hostが復号する範囲、探索とデータ転送——を表で示した。
   さらに`GUIDE_ADVANCED`（実行model・上限・backpressure・dual-host内部）、`GUIDE_MIGRATION`、
   `GUIDE_HID_DESCRIPTORS`を追加した。

## 既知の落とし穴

- `Write Local Name`をsoak刺激として反復するとcontrollerのNVDSが`nvds.c:400`でassertする。永続設定を負荷生成に使わず、scan mode切替を使う。
- logical hostごとの`can_send()` slot予約はBluedroidの先読みと両立せず、NimBLEをstarveさせるため採用しない。
- Classicのcontroller-to-host flow controlをそのまま有効化すると、NimBLEへroutingしたLE ACLのcreditをClassicが返せず、共有bufferが枯渇する。brokerが所有する場合も同じ枯渇が起きる条件が3つあり、いずれも実機でのみ再現した。geometryを学習する`Read Buffer Size`応答はClassic bootstrap中（single-host）に届くので観測はmodeを問わず行う。creditは routed modeだけでなく受信した全ACLに対して返す（片側hostを停止している間のLE ACLが無計上になる）。host再attach時にflow control設定をやり直してはならない（既に有効なcontrollerが再有効化を拒否し、flow controlは有効なままcreditだけ止まる）。
- NimBLE停止を別taskから直接行うとNPL event queueと競合する。停止開始はNimBLE host task自身へ要求する。
- 上流の`npl_freertos_eventq_remove()`はfunction-localのspinlockでcritical sectionを作るため、別coreのhost taskによるdequeueと競合してassertする。vendor patchで受信失敗時はassertせずloopを抜ける。
- RPA更新はadvertising / scanをpreemptする。元の設定と有限deadlineを保持して再開しないと、処理が停止または期限延長する。
- Classic再attach時の物理HCI Resetは既存LE接続を破壊するため、Classicだけへ仮想Command Completeを返す。
- bondが残っているとpairingのHCI経路がまるごと走らない。link key応答とSSP応答をpolicyへ
  分類し忘れていても、bond済みのpeerとは正常に通信できるためtestが通ってしまう。dual-host testは
  接続前にbondを削除して初回pairingを必ず通す。
- AVRCP Targetが宣言できるnotificationは同梱hostで**volume（`0x0d`）1件だけ**である。`esp_avrc_tg_set_rn_evt_cap`は
  allowed eventの部分集合しか受け付けず、それ以外を渡すと`ESP_FAIL`になる。profileが許すかどうかとは
  別問題で、EspBle側では解消できない。許可集合は`esp_avrc_tg_get_rn_evt_cap(ESP_AVRC_RN_CAP_ALLOWED_EVT)`で
  読める。Controller側にこの制限は無い。
- inquiry実行中のSDP照会（`get_remote_services`）と`read_remote_name`は受理されるが応答が来ない。
  どちらも無線を使うため、scan完了を待ってから照会する。呼び出しが`true`を返すので、
  待たずに投げると「成功したのにcallbackが来ない」形で詰まる。
- HIDの制御チャネルは応答が必須で、返さないとHostは待ち続ける。Get_Reportには報告か拒否
  （`esp_bt_hid_device_send_report` / `report_error`）、Set_ReportにはHID handshakeを返す。
  handshakeの成功応答も`esp_bt_hid_device_report_error()`に`ESP_HID_PAR_HANDSHAKE_RSP_SUCCESS`を
  渡して送る——名前は error だが成功も通す関数である。Set_Reportを無応答にすると、Host側の
  `Set_Report`完了eventが永久に来ない。
- report IDの位置がchannelで異なる。Set_Reportは`report_id`フィールドで別に渡り payload に含まれないが、
  interrupt channelのreportは payload 先頭に含む。Get_Reportの応答もHost側では先頭に付いて届く。
  device側は`reportId`を正として payload のみを渡すよう正規化した。
- Class of Deviceは「設定しても電波に出ない」経路が3つある。順に踏んだ。(1) profileのservice登録が
  自分のservice由来の値で上書きするため、`begin()`で設定するだけでは消える——登録完了event
  （`ESP_SPP_START_EVT`、`ESP_HIDD_REGISTER_APP_EVT`）で再適用する。登録は非同期なので、
  start呼び出しの前に適用しても間に合わない。(2) controllerはinquiry scanを有効化する時点で
  classを取り込むため、scan modeより後にclassを書くと古い値が電波に残る——classを先、scan modeを後にする。
  (3) 実行時に変更する場合、同じscan modeを書き直してもHCI上は変化が無く、controllerは古いclassを返し続ける
  ——discoverabilityを一度落として戻す。connectabilityは維持して、その間の接続要求を拒否しない。
  なお`esp_bt_gap_set_cod()`はBTC taskへの非同期要求なので、直後の`esp_bt_gap_get_cod()`は前の値を返す。
  反映完了のeventは存在せず、読み直して一致を見るしかない。
- 可視性（connectable / discoverable）はprofileが各自`esp_bt_gap_set_scan_mode()`を呼んでいたため、
  最後にstartしたprofileが決めていた。所有者を`EspBleClassic`に一元化し、profileは値を決めず再適用だけを行う。
- Secure Simple Pairingはserviceが要求したときだけapplicationへ確認を求める。`esp_bt_gap_set_security_param`で
  IO capabilityを設定しても、SPPを`ESP_SPP_SEC_NONE`で開いていると両端がJust Worksで合意し、
  numeric comparisonもpasskeyも発生しないまま pairing が完了する。IO capabilityを設定に反映させるには
  service側で`ESP_SPP_SEC_AUTHENTICATE | ESP_SPP_SEC_ENCRYPT | ESP_SPP_SEC_MITM`を要求する必要がある。
- pairing失敗ではSPPのeventが来ない。接続を開始したSPP側は`ESP_BT_GAP_AUTH_CMPL_EVT`の失敗で
  自分の接続試行を終わらせないと、自前のtimeoutまで再接続を拒否し続ける。
- Classic HIDのInput Reportはreport IDを先頭に付けて配送されるが、Report Descriptorのfield offsetは
  payload起点である。descriptorから復号するときはID分をずらす。剥がす条件は「先頭byteが当該reportのID」かつ
  「剥がすと宣言長と一致する」の両方にする——長さだけで判断すると、たまたま同じ値で始まるpayloadを削ってしまう。
- HID deviceのSDP recordは`CONFIG_BT_SDP_PAD_LEN` = 300 byteのpadに全属性の値を詰める。
  `HID_DevAddRecord()`が書く固定属性（record handle 4、service class list 3、protocol list 13、
  language base 9、additional protocol list 15、profile descriptor list 8、HIDのintとboolean 20、
  language id base 8、browse group 3、descriptor listのheader 6、文字列3つのNUL 3）で86 byteを使うため、
  Report Descriptorと`name` / `description` / `provider`に残るのは214 byteである。
  超えると`SDP_AddAttribute`が`attr_len:0`で失敗し（logに`length exceed maximum: ID 5`が出る）、
  それでも`esp_bt_hid_device_register_app()`は成功を返す——recordの無いdeviceが起動し、
  Hostからは見えない。`begin()`が登録前に検査して`ResourceExhausted`で拒否する。
  実機でも一致を確認した（144 + 57 = 201は登録でき、158 + 57 = 215は失敗する）。
  なおdescriptor長はv5.5.5では1 byte長で符号化されるため、pad以前に255 byteが上限である。
- AG側の`esp_hf_ag_unknown_at_send()`は応答行を送るだけで、終端のOKを送らない
  （`btc_hf_unat_response`が`ok_flag`を`BTA_AG_OK_CONTINUE`のままにする）。OKを送らないと
  client側は同じcommandの応答を待ち続け、**次のAT commandがqueueから出ない**。Apple拡張で
  実測した: `AT+XAPL`へ`+XAPL=...`だけを返すと、続く`AT+IPHONEACCEV`が消える。
  `respondToUnknownAt()`が応答行の後にOKを送り、nullptrならerrorを返して必ず交換を閉じる。
- Bluedroid公開HID Host APIにはpage中の接続試行を取り消す手段がない。未接続peerへの任意timeoutを
  `esp_bt_hid_host_disconnect()`で実装すると内部のconnecting状態が残り、次の接続を拒否するため採用しない。
  APIはbackendの最終`OPEN`失敗を`onConnectionFailed()`で非同期通知する。

## 作業環境と再開方法

- Arduino-ESP32: 3.3.11
- Classic archive: ESP-IDF v5.5.5、xtensa-esp32 GCC 14.2.0
- 実機: 無印ESP32 2台、pytest profile `esp32_peer_host` / `esp32_peer_device`
- Python環境: `tests/`で`uv run --env-file .env ...`

代表回帰:

```sh
cd tests
uv run --env-file .env pytest -s peer/dual_host_smoke/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest -s peer/dual_host_rpa/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest -s peer/classic_a2dp_media/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest --clean -s peer/classic_hfp_cvsd/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest --clean -s peer/dual_host_hfp/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest --clean -s peer/dual_host_a2dp/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run pytest -q unit
```

長時間soakはrepository rootから次で開始します。

```sh
ESPBLE_DUAL_SOAK_RUNS=20 tools/run_dual_host_soak.sh
```

既定では各runでcommand競合100サイクル、restart 100サイクルを実行し、最初のrunだけclean buildします。
ログは`tests/.soak/`へ保存し、1 runでも失敗すればその時点で停止します。実行前に他のprocessが対象boardを
使っていないことと、`tests/.env`のprofile/port設定を確認します。

Classic-only A2DPの連続media転送は同じfixtureをpacket数だけ拡張して実行します。

```sh
ESPBLE_A2DP_SOAK_PACKETS=20000 tools/run_classic_a2dp_soak.sh
```

既定では最初にclean buildし、packet数は100〜500,000の範囲で指定できます。既存buildを使う調査時だけ
`ESPBLE_A2DP_SOAK_CLEAN=0`を指定します。送受信packet/byteの完全一致、`WouldBlock`からの再開、両基板の
free/minimum/largest heapを確認し、logは`tests/.soak/classic-a2dp-<UTC時刻>/run.log`へ保存します。

archive再生成は[Classic host archive再生成](CLASSIC_HOST_BUILD.ja.md)を参照してください。

2026-08-11に`v5.5.5` tagの独立cloneへ全submoduleをcheckoutし、GCC 14.2.0でAudio設定を含む一時出力へ
clean buildした。生成物は格納済みarchiveと`cmp`で一致し、SHA-256は双方
`d64d3a40a3f598e206c5aaf798e9d8fda5c867b632224ed72a616b1221089421`だった。
固定Kconfig、必須prefixed symbol、unprefixed global defined symbolがないことも確認した。

## 完了判定と記録

soak完了時は、run数、開始/終了時刻、総経過時間、各runのpytest結果、heap初回/最終値、brokerの
enqueue/send/qmax/qfull/mismatch/busy/unknown、panic/backtraceの有無を
[技術検証](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)へ追記します。失敗時は試験を緩めず、最初の失敗logを
保存して再現条件を最小化します。作業終了時は[次回リリース計画](PLAN_RELEASE_NEXT.ja.md)とSTATUSも
同じ結論へ更新します。

## 2026-08-09 長時間soak結果

`ESPBLE_DUAL_SOAK_RUNS=20 tools/run_dual_host_soak.sh`を実行し、全runが成功しました。期間は
2026-08-09 14:29:26〜16:11:10 JST、総経過時間は1時間41分44秒です。各runはcommand競合100サイクルと
停止・再登録100サイクルを行い、暗号化GATT、Classic HID双方向、FIFO backpressure、再attach、任意順停止、
destructor後の再起動を含みます。

- 全runの最終diagnosticsでcommand enqueue数と物理send数が一致
- `qfull=0 mismatch=0 busy=0 unknown=0`、最大通常queue深度5
- 各run内のfree heapとlargest blockに減少なし。1 runで開始後にfree heapが8 byte増え、その後は一定
- panic、watchdog、backtrace、再接続失敗なし

logは`tests/.soak/dual-host-20260809T052926Z/`に保存しています。`.soak/`はgit管理外なので、長期保存が
必要な場合はrelease artifactまたは外部保存先へ退避してください。
