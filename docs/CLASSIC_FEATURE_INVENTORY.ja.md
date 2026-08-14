# Classic機能の棚卸し

無印ESP32のClassic対応について、公開APIで到達できる範囲と、独自archiveが持っているのに
まだ公開していない範囲を突き合わせた記録です。2026-08-14時点。

分類は次の3つです。

- **公開**: `EspBleClassic`系のクラスから利用でき、Peer testがある
- **部分**: 使えるが範囲が限定されている、または固定値で動いている
- **未公開**: archiveにAPIの実体はあるが、EspBleの公開APIから到達できない

「未公開」は欠陥ではなく未着手です。採用するときは仕様、example、testを同時に追加します。
何を先に実装するかは末尾の優先度で決めます。

## GAP・デバイス管理

| 機能 | 状態 | 補足 |
|---|---|---|
| device name設定 | 公開 | `EspBleClassicConfig::deviceName` |
| 接続可能・発見可能状態 | 公開 | `EspBleClassicConfig::visibility`と`setVisibility()` / `visibility()`（`Hidden` / `ConnectableOnly` / `ConnectableDiscoverable`）。所有者は`EspBleClassic`で、profileは値を決めず再適用のみ行う。A2DP Sinkはsource接続中だけ自分で隠し、復帰時は所有者の値へ戻す |
| inquiry（周辺機器探索） | 公開 | `EspBleClassicInquiry`。address、name（EIR fallbackつき）、Class of Device、RSSIを返す。名前の個別照会は`requestName()` / `onRemoteName()` |
| SDP（相手のservice照会） | 公開 | `inquiry().requestServices()` / `onRemoteServices()`。UUIDを文字列で最大12件返す。scan中は応答が来ないので完了後に照会する |
| Class of Device | 公開 | `EspBleClassicConfig::classOfDevice`と`setClassOfDevice()` / `classOfDevice()`。反映は非同期で、profile登録による上書きとcontrollerのinquiry scan取り込みまでlibraryが面倒を見る |
| bond一覧・削除 | 公開 | `bondCount()` / `bond(index)` / `deleteBond()` / `deleteAllBonds()` |
| pairing（SSP） | 公開 | `EspBleClassicSecurityConfig`でIO capabilityを選び、numeric comparisonとpasskey要求をアプリへ通知して`confirmNumericComparison()` / `providePasskey()`で応答する。無応答はtimeoutで拒否。設定を有効にするとservice側がMITMを要求する |
| pairing（legacy PIN） | 未公開 | 応答経路が無いため拒否する。以前の固定PIN `1234`は廃止した |
| 暗号鍵長・QoS・page timeout・AFH・EIR | 未公開 | `set_min_enc_key_size` / `set_qos` / `set_page_timeout` / `set_afh_channels` / `config_eir_data` |
| RSSI・送信電力 | 未公開 | `read_rssi_delta` / `read_tx_pwr_lvl` |

## SPP

| 機能 | 状態 | 補足 |
|---|---|---|
| server（1 service） | 公開 | `startServer()` / `stopServer()` |
| client接続 | 公開 | `connect(address, timeout)`はSDPからchannelを解決する。相手が複数serviceを公開している場合は`connectToChannel(address, channel)`で選ぶ。送信側は同時1接続 |
| 複数session | 公開 | `sessionCount()` / session単位のread/write |
| 送受信・queue・統計 | 公開 | `write()` / `read()` / `pendingWriteCount()` / `droppedWriteCount()` |
| 複数serverの同時公開 | 公開 | `startServer()`を繰り返し呼ぶと最大4 serviceを公開する。`serverCount()` / `server(index)`で列挙し、`onServerStarted()`がchannelを渡す。停止は`stopServer()`（全停止）のみ |
| VFS（Stream風API） | 未公開 | `esp_spp_vfs_register`。Arduino利用者には`BluetoothSerial`相当の使い勝手になる |

## HID Device

| 機能 | 状態 | 補足 |
|---|---|---|
| register / connect / disconnect | 公開 | 任意のReport Descriptorを渡せる |
| profile API（keyboard / mouse / consumer / system / gamepad） | 公開 | `hidKeyboard()`などBLEと同名・同シグネチャ。configureした分だけReport Descriptorを合成する |
| keyboard layout・NKRO | 公開 | `setLayout()` / `write()` / `pressKey()`。BLEと同じ変換表を共有する |
| Input Report送信 | 公開 | `sendInputReport()` / `sendReport()` |
| Output Report受信（LED） | 公開 | `onOutputReport()`。profile利用時は`EspBleClassicHidKeyboardLeds`へ展開する |
| Get_Report要求への応答 | 公開 | `onReportRequested()`と`respondToReportRequest()` / `refuseReportRequest()`。要求が無いときの応答は拒否する |
| Feature Report | 公開 | `onSetReport()`がHostの使ったtypeを保って配送し、Get_Reportの応答もtype指定で返せる |
| protocol mode（Boot / Report） | 公開 | `protocolMode()` / `onProtocolMode()`。Hostが決めるものなのでsetterは持たない |
| virtual cable unplug | 公開 | `virtualCableUnplug()`。Host側のペア情報を明示的に解除する |
| report error応答 | 公開 | `refuseReportRequest()`。Set_Reportは拒否しなければ成功応答を自動で返す |

## HID Host

| 機能 | 状態 | 補足 |
|---|---|---|
| connect / disconnect / Input Report受信 | 公開 | 1接続のみ |
| Report Descriptor解析とkeyboard / mouse eventへの復号 | 公開 | `onKeyboardState()` / `onKeyboard()` / `onMouse()` / `setKeyboardLayout()`。SDPで受け取ったdescriptorを解析するので、独自layoutのdeviceも扱える |
| 不正Input Reportの計上 | 公開 | `invalidInputReportCount()`。BLEと同じくrollover（usage 0x01〜0x03）は押下として配らない |
| Output Report送信 | 公開 | `sendOutputReport()`（raw）と`setKeyboardLeds()`（BLEと同名。report IDは相手のdescriptor由来） |
| 非同期の接続失敗通知 | 公開 | `onConnectionFailed()` |
| Get_Report / Set_Report | 公開 | `requestReport()` / `sendReport()`。結果は`onReportResult()` / `onReportSent()`へ届く |
| protocol mode取得・設定 | 公開 | `requestProtocolMode()` / `setProtocolMode()` / `onProtocolMode()` |
| idle rate | 公開 | `requestIdleRate()` / `setIdleRate()` / `onIdleRate()` |
| virtual cable unplug | 公開 | `virtualCableUnplug()` |
| 複数device同時接続 | 未公開 | backendは複数を扱えるが公開APIは単一接続を前提にしている |

## A2DP

| 機能 | 状態 | 補足 |
|---|---|---|
| Sink接続・SBC設定・encode済みmedia受信 | 公開 | 実機で連続転送を確認済み |
| Source接続・start/suspend・encode済み送信 | 公開 | `WouldBlock`のbackpressureつき。20,000 packet soakを完走 |
| codec configurationの公開 | 公開 | `codecConfig()`（sample rate、channel、bitpool、raw） |
| Source endpoint | 部分 | SBC固定。`register_stream_endpoint`で別codecやendpointを増やせる |
| delay reporting | 未公開 | `esp_a2d_sink_get_delay_value` / `set_delay_value`。動画再生のlip syncで効く |
| SBC以外のcodec | 未公開 | archiveはexternal codec構成なので、negotiationを公開すればAAC等も扱えるがEspBleはencode/decodeを持たない方針 |

## AVRCP

| 機能 | 状態 | 補足 |
|---|---|---|
| CT/TG接続、remote features | 公開 | |
| passthrough送受信 | 公開 | `sendKey()` / `sendPassthrough()` / `onPassthrough()` |
| metadata・play status要求と応答受信 | 公開 | 送信側はCT。応答は外部Targetとの相互運用確認が残る |
| absolute volume・volume notification | 公開 | one-shot再登録の扱いを含む |
| TGのmetadata / play status応答送信 | 未実装（backend側にAPIなし） | ESP-IDF v5.5.5の公開TG APIに送信手段がない。EspBle側だけでは解消できない |
| TGのnotification応答・capability設定 | 未公開 | `esp_avrc_tg_send_rn_rsp` / `set_rn_evt_cap` / `set_psth_cmd_filter` |
| player setting（repeat・shuffle等） | 未公開 | `esp_avrc_ct_send_set_player_value_cmd` |
| browsing・cover art | 対象外 | archive生成時に無効化している |

## HFP Client

| 機能 | 状態 | 補足 |
|---|---|---|
| SLC・発信・着信・応答・終了・DTMF | 公開 | |
| audio接続、CVSD/mSBC raw SCO送受信、packet統計 | 公開 | |
| 音量・voice recognition | 公開 | |
| 通話一覧（CLCC） | 公開 | `queryCurrentCalls()` |
| 三者通話・保留（CHLD） | 未公開 | `esp_hf_client_send_chld_cmd` / `send_btrh_cmd` |
| memory dial・last voice tag | 未公開 | `dial_memory` / `request_last_voice_tag_number` |
| operator名・subscriber情報 | 未公開 | `query_current_operator_name` / `retrieve_subscriber_info` |
| NREC・Apple拡張（XAPL / IPHONEACCEV） | 未公開 | `send_nrec` / `send_xapl` / `send_iphoneaccev` |

## HFP Audio Gateway

| 機能 | 状態 | 補足 |
|---|---|---|
| 自動SLC応答（CIND/COPS/CNUM/CLCC）、単一call model | 公開 | |
| audio接続、CVSD/mSBC raw SCO、codec選択 | 公開 | |
| network status、音量、voice recognition | 公開 | |
| 任意ATへの応答 | 公開 | `respondToUnknownAt()` |
| in-band ring tone | 未公開 | `esp_hf_ag_bsir` |
| indicator個別通知 | 部分 | `setNetworkStatus()`が内部で使う。任意のCIEVを送る手段はない |
| 複数call・保留・三者通話 | 未公開 | 単一call modelを意図的に選んでいる。拡張するならmodel/provider interfaceを足す |
| 電話帳 | 対象外 | EspBleに電話網や電話帳は実装しない |

## dual-host（実験機能）

| 機能 | 状態 | 補足 |
|---|---|---|
| BLEとClassicの同時利用 | 公開（実験） | begin()したhostで決まる。build flagはない |
| HCI routing、command scheduler、event mask union、controller lifecycle | 公開（内部） | |
| controller-to-host ACL flow control | 公開（内部） | brokerが所有し、受信ACLごとにcreditを返す |
| 公開API・対応profile・制限の確定 | 未確定 | 正式サポート範囲を決めるまで互換性を保証しない |

## 優先度の考え方

利用者が最初に詰まるのは「相手を探せない」「pairingを制御できない」「HIDとして正しく見えない」の3つで、
どれもGAP層です。profileの細かい拡張より先にここを埋めるのが自然です。

1. **完了: inquiry**。`EspBleClassicInquiry`として公開した。SDP照会（`get_remote_services`）と
   `read_remote_name`の個別照会は残る。
2. **完了: pairing制御**。IO capability選択とnumeric comparison / passkeyのアプリ応答を公開した。
   固定PINの自動承諾は廃止した。
3. **完了: bond管理**。一覧・削除をBLE側と揃えた。
4. **完了: HIDのAPI形状**。Device側のprofile API（keyboard / mouse / consumer / system / gamepad）と
   Host側のReport Descriptor解析をBLEと同じ名前・同じevent形で公開した。descriptorとpackingは
   BLEと同じmoduleを共有するので、片側だけ変わることはない。
5. **完了: Class of Device**。設定・読み出しを公開した。profile登録による上書き、controllerが
   inquiry scan有効化時にclassを取り込む性質、反映が非同期であることの3点をlibrary側で吸収している。
6. **完了: 接続可能・発見可能の制御**。所有者を`EspBleClassic`へ一元化し、profileは再適用だけを行う。
7. **完了: HID Device / HostのGet_Report・Set_Report・protocol mode・idle rate・virtual cable unplug**。
   制御チャネルは応答が必須で、Get_Reportの無応答とSet_Reportのhandshake欠落はどちらもHostを
   待たせ続けていた。Peer test `classic_hid_control`で両方向を検証している。
8. **完了: SPPの複数server**。`startServer()`の繰り返しで最大4 serviceを公開する。channel単位の停止は
   意図的に持たない——`stopServer()`で全停止して必要な分を再startすれば足り、取り消す手段を2つに
   増やさないため。相手側から特定serviceへ届くよう`connectToChannel()`を用意した。
9. **A2DP delay reporting、AVRCP TG notification応答**: 相互運用の作り込み段階で必要になる。
10. **完了: SDP照会と`read_remote_name`**。addressだけ分かっている相手に「何を提供しているか」「何と
    名乗るか」を問い合わせられるようにした。複数serviceを公開できるようになったので、接続先channelを
    決める材料としても対になる。scan中は応答が来ない点をAPI docと落とし穴へ記録した。

外部機器との相互運用（Gate D）と、core内蔵Bluedroidとの相互接続Peer testは、
[引き継ぎ](HANDOFF_ESP32_CLASSIC.ja.md)の作業メモにあるとおり別途進めます。
